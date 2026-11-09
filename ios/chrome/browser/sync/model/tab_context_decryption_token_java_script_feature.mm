// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/tab_context_decryption_token_java_script_feature.h"

#import <stdint.h>

#import <optional>
#import <string>
#import <utility>

#import "base/check.h"
#import "base/containers/span.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/no_destructor.h"
#import "base/uuid.h"
#import "base/values.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/base/features.h"
#import "components/sync_tab_context/container_id.h"
#import "components/sync_tab_context/http_rpc_constants.h"
#import "components/sync_tab_context/tab_context_sync_service.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/tab_context_sync_service_factory.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/web_state.h"

namespace {

constexpr char kScriptName[] = "tab_context_decryption_token";
constexpr char kTabContextDecryptionTokenHandlerName[] =
    "TabContextDecryptionTokenMessage";
constexpr char kObfuscatedGaiaIdKey[] = "obfuscatedGaiaId";
constexpr char kContainerIdKey[] = "containerId";

struct ParsedTokenRequest {
  GaiaId gaia_id;
  sync_tab_context::ContainerId container_id;
};

// Parses and validates the `obfuscatedGaiaId` and `containerId` fields from
// `body`, returning `std::nullopt` if `body` is malformed.
std::optional<ParsedTokenRequest> ParseTokenRequest(
    const web::ScriptMessageValue& body) {
  if (body.type() != base::Value::Type::DICT) {
    return std::nullopt;
  }

  const web::ScriptMessageDictValue& dict = body.GetDict();
  const std::optional<std::string> obfuscated_gaia_id =
      dict.FindString(kObfuscatedGaiaIdKey);
  const std::optional<std::string> container_id_str =
      dict.FindString(kContainerIdKey);
  if (!obfuscated_gaia_id || obfuscated_gaia_id->empty() || !container_id_str) {
    return std::nullopt;
  }

  const base::Uuid container_uuid =
      base::Uuid::ParseCaseInsensitive(*container_id_str);
  if (!container_uuid.is_valid()) {
    return std::nullopt;
  }

  return ParsedTokenRequest{
      .gaia_id = GaiaId(*obfuscated_gaia_id),
      .container_id = sync_tab_context::ContainerId(container_uuid),
  };
}

// Runs `callback` with a `base::ListValue` of byte integers when `token` is
// present, or an empty `base::Value` (which converts to `null` in JavaScript)
// otherwise.
void RunCallbackWithOptionalToken(
    base::OnceCallback<void(const base::Value*, NSString*)> callback,
    std::optional<std::string> token) {
  if (!token.has_value()) {
    base::Value empty_reply;
    std::move(callback).Run(&empty_reply, nil);
    return;
  }

  base::ListValue byte_list;
  byte_list.reserve(token->size());
  for (uint8_t byte : base::as_byte_span(*token)) {
    byte_list.Append(int{byte});
  }
  base::Value reply(std::move(byte_list));
  std::move(callback).Run(&reply, nil);
}

}  // namespace

// static
TabContextDecryptionTokenJavaScriptFeature*
TabContextDecryptionTokenJavaScriptFeature::GetInstance() {
  static base::NoDestructor<TabContextDecryptionTokenJavaScriptFeature>
      instance;
  return instance.get();
}

TabContextDecryptionTokenJavaScriptFeature::
    TabContextDecryptionTokenJavaScriptFeature()
    : web::JavaScriptFeature(
          // Inject into `kPageContentWorld` so the page can call the exposed
          // `chrome.getContainerDecryptionToken` API directly.
          web::ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow,
              FeatureScript::PlaceholderReplacementsCallback(),
              web::OriginFilter::kSyncTabContext)},
          {},
          web::OriginFilter::kSyncTabContext) {}

TabContextDecryptionTokenJavaScriptFeature::
    ~TabContextDecryptionTokenJavaScriptFeature() = default;

std::optional<std::string>
TabContextDecryptionTokenJavaScriptFeature::GetScriptMessageHandlerName()
    const {
  return kTabContextDecryptionTokenHandlerName;
}

bool TabContextDecryptionTokenJavaScriptFeature::GetFeatureRepliesToMessages()
    const {
  return true;
}

void TabContextDecryptionTokenJavaScriptFeature::ScriptMessageReceivedWithReply(
    web::WebState* web_state,
    const web::ScriptMessage& message,
    ScriptMessageReplyCallback callback) {
  // Guaranteed non-null by the `JavaScriptFeature` API contract.
  CHECK(web_state);
  CHECK(base::FeatureList::IsEnabled(syncer::kSyncEncryptedTabContext));

  if (!message.is_main_frame() ||
      message.security_origin() !=
          sync_tab_context::GetAllowedTabContextOrigin()) {
    RunCallbackWithOptionalToken(std::move(callback), std::nullopt);
    return;
  }

  const std::optional<ParsedTokenRequest> request =
      ParseTokenRequest(message.body());
  if (!request) {
    RunCallbackWithOptionalToken(std::move(callback), std::nullopt);
    return;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  CHECK(profile);
  if (profile->IsOffTheRecord()) {
    RunCallbackWithOptionalToken(std::move(callback), std::nullopt);
    return;
  }

  const signin::IdentityManager* const identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager ||
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
              .gaia != request->gaia_id) {
    RunCallbackWithOptionalToken(std::move(callback), std::nullopt);
    return;
  }

  sync_tab_context::TabContextSyncService* const service =
      TabContextSyncServiceFactory::GetForProfile(profile);
  if (!service) {
    RunCallbackWithOptionalToken(std::move(callback), std::nullopt);
    return;
  }

  service->GetContainerAccessToken(
      request->container_id,
      base::BindOnce(&RunCallbackWithOptionalToken, std::move(callback)));
}
