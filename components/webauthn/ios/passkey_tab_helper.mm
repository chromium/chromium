// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_tab_helper.h"

#import "base/base64.h"
#import "base/base64url.h"
#import "base/check_deref.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "components/webauthn/ios/passkey_java_script_feature.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/web_state.h"

namespace {

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange
enum class WebAuthenticationIOSContentAreaEvent {
  kGetRequested,
  kCreateRequested,
  kGetResolvedGpm,
  kGetResolvedNonGpm,
  kCreateResolvedGpm,
  kCreateResolvedNonGpm,
  kMaxValue = kCreateResolvedNonGpm,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml)

// Logs a metric indicating that an `event` occurred.
void LogEvent(WebAuthenticationIOSContentAreaEvent event) {
  base::UmaHistogramEnumeration("WebAuthentication.IOS.ContentAreaEvent",
                                event);
}

class [[maybe_unused, nodiscard]] ScopedAllowPasskeyCreationInfobar {
 public:
  ScopedAllowPasskeyCreationInfobar(IOSPasskeyClient* client)
      : client_(client) {
    client_->AllowPasskeyCreationInfobar(true);
  }
  ~ScopedAllowPasskeyCreationInfobar() {
    client_->AllowPasskeyCreationInfobar(false);
  }

 private:
  raw_ptr<IOSPasskeyClient> client_;
};

}  // namespace

PasskeyTabHelper::~PasskeyTabHelper() = default;

void PasskeyTabHelper::LogEventFromString(const std::string& event) {
  if (event == "getRequested") {
    LogEvent(WebAuthenticationIOSContentAreaEvent::kGetRequested);
  } else if (event == "createRequested") {
    LogEvent(WebAuthenticationIOSContentAreaEvent::kCreateRequested);
  } else if (event == "createResolvedGpm") {
    LogEvent(WebAuthenticationIOSContentAreaEvent::kCreateResolvedGpm);
  } else if (event == "createResolvedNonGpm") {
    LogEvent(WebAuthenticationIOSContentAreaEvent::kCreateResolvedNonGpm);
  } else {
    NOTREACHED();
  }
}

void PasskeyTabHelper::HandleGetResolvedEvent(
    const std::string& credential_id_base64url_encoded,
    const std::string& rp_id) {
  std::string credential_id;
  if (!base::Base64UrlDecode(credential_id_base64url_encoded,
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &credential_id)) {
    return;
  }

  if (passkey_model_->GetPasskeyByCredentialId(rp_id, credential_id)
          .has_value()) {
    LogEvent(WebAuthenticationIOSContentAreaEvent::kGetResolvedGpm);
  } else {
    LogEvent(WebAuthenticationIOSContentAreaEvent::kGetResolvedNonGpm);
  }
}

PasskeyTabHelper::PasskeyTabHelper(web::WebState* web_state,
                                   webauthn::PasskeyModel* passkey_model,
                                   std::unique_ptr<IOSPasskeyClient> client)
    : passkey_model_(CHECK_DEREF(passkey_model)), client_(std::move(client)) {
  CHECK(client_);
  CHECK(web_state);
  web_state->AddObserver(this);

  PasskeyJavaScriptFeature::GetInstance()->SetAllowModalLogin(
      web_state, client_->IsModalLoginWithShimAllowed());
}

void PasskeyTabHelper::AddNewPasskey(
    sync_pb::WebauthnCredentialSpecifics& passkey) {
  ScopedAllowPasskeyCreationInfobar scopedAllowPasskeyCreationInfobar(
      client_.get());
  passkey_model_->CreatePasskey(passkey);
}

// WebStateObserver

void PasskeyTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  PasskeyJavaScriptFeature::GetInstance()->SetAllowModalLogin(
      web_state, client_->IsModalLoginWithShimAllowed());
}

void PasskeyTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}
