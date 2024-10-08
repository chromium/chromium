// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/guest_view/web_view/web_view_internal_api.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/optional_util.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/crash/core/common/crash_key.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/stop_find_action.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/web_view/controlled_frame_embedder_url_fetcher.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_content_script_manager.h"
#include "extensions/common/api/web_view_internal.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/script_constants.h"
#include "extensions/common/user_script.h"
#include "third_party/skia/include/core/SkBitmap.h"

using content::WebContents;
using extensions::ExtensionResource;
using extensions::api::web_view_internal::ContentScriptDetails;
using extensions::api::web_view_internal::InjectionItems;
using extensions::api::web_view_internal::SetPermission::Params;
using extensions::api::extension_types::InjectDetails;
using extensions::UserScript;
using zoom::ZoomController;
// error messages for content scripts:
namespace errors = extensions::manifest_errors;
namespace web_view_internal = extensions::api::web_view_internal;

namespace {

const char kCacheKey[] = "cache";
const char kCookiesKey[] = "cookies";
const char kSessionCookiesKey[] = "sessionCookies";
const char kPersistentCookiesKey[] = "persistentCookies";
const char kFileSystemsKey[] = "fileSystems";
const char kIndexedDBKey[] = "indexedDB";
const char kLocalStorageKey[] = "localStorage";
const char kWebSQLKey[] = "webSQL";
const char kSinceKey[] = "since";
const char kLoadFileError[] = "Failed to load file: \"*\". ";
const char kHostIDError[] = "Failed to generate HostID.";
const char kViewInstanceIdError[] = "view_instance_id is missing.";
const char kDuplicatedContentScriptNamesError[] =
    "The given content script name already exists.";

const char kGeneratedScriptFilePrefix[] = "generated_script_file:";

uint32_t MaskForKey(const char* key) {
  if (strcmp(key, kCacheKey) == 0)
    return webview::WEB_VIEW_REMOVE_DATA_MASK_CACHE;
  if (strcmp(key, kSessionCookiesKey) == 0)
    return webview::WEB_VIEW_REMOVE_DATA_MASK_SESSION_COOKIES;
  if (strcmp(key, kPersistentCookiesKey) == 0)
    return webview::WEB_VIEW_REMOVE_DATA_MASK_PERSISTENT_COOKIES;
  if (strcmp(key, kCookiesKey) == 0)
    return webview::WEB_VIEW_REMOVE_DATA_MASK_COOKIES;
  if (strcmp(key, kFileSystemsKey) == 0)
    return webview::WEB_VIEW_REMOVE_DATA_MASK_FILE_SYSTEMS;
  if (strcmp(key, kIndexedDBKey) == 0)
    return webview::WEB_VIEW_REMOVE_DATA_MASK_INDEXEDDB;
  if (strcmp(key, kLocalStorageKey) == 0)
    return webview::WEB_VIEW_REMOVE_DATA_MASK_LOCAL_STORAGE;
  if (strcmp(key, kWebSQLKey) == 0)
    return webview::WEB_VIEW_REMOVE_DATA_MASK_WEBSQL;
  return 0;
}

std::optional<extensions::mojom::HostID> GenerateHostIDFromEmbedder(
    const extensions::Extension* extension,
    content::RenderFrameHost* embedder_rfh) {
  if (extension) {
    return extensions::mojom::HostID(
        extensions::mojom::HostID::HostType::kExtensions, extension->id());
  }

  if (embedder_rfh && embedder_rfh->GetMainFrame()->GetWebUI()) {
    const GURL& url = embedder_rfh->GetSiteInstance()->GetSiteURL();
    return extensions::mojom::HostID(
        extensions::mojom::HostID::HostType::kWebUi, url.spec());
  }

  if (embedder_rfh->GetWebExposedIsolationLevel() >=
      content::WebExposedIsolationLevel::kIsolatedApplication) {
    const std::string origin =
        embedder_rfh->GetMainFrame()->GetLastCommittedOrigin().Serialize();
    return extensions::mojom::HostID(
        extensions::mojom::HostID::HostType::kControlledFrameEmbedder, origin);
  }
  return std::nullopt;
}

// Creates content script files when parsing InjectionItems of "js" or "css"
// proterties, and stores them in the `contents`.
void ParseScriptFiles(const GURL& owner_base_url,
                      const extensions::Extension* extension,
                      const InjectionItems& items,
                      UserScript::ContentList* contents) {
  DCHECK(contents->empty());
  contents->reserve((items.files ? items.files->size() : 0) +
                    (items.code ? 1 : 0));
  // files:
  if (items.files) {
    for (const std::string& relative : *items.files) {
      GURL url = owner_base_url.Resolve(relative);
      if (extension) {
        ExtensionResource resource = extension->GetResource(relative);
        contents->push_back(UserScript::Content::CreateFile(
            resource.extension_root(), resource.relative_path(), url));
      } else {
        contents->push_back(UserScript::Content::CreateFile(
            base::FilePath(), base::FilePath(), url));
      }
    }
  }

  // code:
  if (items.code) {
    GURL url = owner_base_url.Resolve(base::StringPrintf(
        "%s%s", kGeneratedScriptFilePrefix,
        base::Uuid::GenerateRandomV4().AsLowercaseString().c_str()));
    auto content = UserScript::Content::CreateInlineCode(url);
    content->set_content(*items.code);
    contents->push_back(std::move(content));
  }
}

// Parses the values stored in ContentScriptDetails, and constructs a
// user script.
std::unique_ptr<extensions::UserScript> ParseContentScript(
    const ContentScriptDetails& script_value,
    const extensions::Extension* extension,
    const GURL& owner_base_url,
    std::string* error) {
  // matches (required):
  if (script_value.matches.empty())
    return nullptr;

  std::unique_ptr<extensions::UserScript> script(new extensions::UserScript());

  // The default for WebUI is not having special access, but we can change that
  // if needed.
  bool allowed_everywhere =
      extension && extensions::PermissionsData::CanExecuteScriptEverywhere(
                       extension->id(), extension->location());
  for (const std::string& match : script_value.matches) {
    URLPattern pattern(UserScript::ValidUserScriptSchemes(allowed_everywhere));
    if (pattern.Parse(match) != URLPattern::ParseResult::kSuccess) {
      *error = errors::kInvalidMatches;
      return nullptr;
    }
    script->add_url_pattern(pattern);
  }

  // exclude_matches:
  if (script_value.exclude_matches) {
    for (const std::string& exclude_match : *script_value.exclude_matches) {
      URLPattern pattern(
          UserScript::ValidUserScriptSchemes(allowed_everywhere));

      if (pattern.Parse(exclude_match) != URLPattern::ParseResult::kSuccess) {
        *error = errors::kInvalidExcludeMatches;
        return nullptr;
      }
      script->add_exclude_url_pattern(pattern);
    }
  }
  // run_at:
  if (script_value.run_at != extensions::api::extension_types::RunAt::kNone) {
    extensions::mojom::RunLocation run_at =
        extensions::mojom::RunLocation::kUndefined;
    switch (script_value.run_at) {
      case extensions::api::extension_types::RunAt::kNone:
      case extensions::api::extension_types::RunAt::kDocumentIdle:
        run_at = extensions::mojom::RunLocation::kDocumentIdle;
        break;
      case extensions::api::extension_types::RunAt::kDocumentStart:
        run_at = extensions::mojom::RunLocation::kDocumentStart;
        break;
      case extensions::api::extension_types::RunAt::kDocumentEnd:
        run_at = extensions::mojom::RunLocation::kDocumentEnd;
        break;
    }
    // The default for run_at is RUN_AT_DOCUMENT_IDLE.
    script->set_run_location(run_at);
  }

  // match_about_blank:
  if (script_value.match_about_blank) {
    script->set_match_origin_as_fallback(
        *script_value.match_about_blank
            ? extensions::MatchOriginAsFallbackBehavior::
                  kMatchForAboutSchemeAndClimbTree
            : extensions::MatchOriginAsFallbackBehavior::kNever);
  }

  // css:
  if (script_value.css) {
    ParseScriptFiles(owner_base_url, extension, *script_value.css,
                     &script->css_scripts());
  }

  // js:
  if (script_value.js) {
    ParseScriptFiles(owner_base_url, extension, *script_value.js,
                     &script->js_scripts());
  }

  // all_frames:
  if (script_value.all_frames)
    script->set_match_all_frames(*script_value.all_frames);

  // include_globs:
  if (script_value.include_globs) {
    for (const std::string& glob : *script_value.include_globs)
      script->add_glob(glob);
  }

  // exclude_globs:
  if (script_value.exclude_globs) {
    for (const std::string& glob : *script_value.exclude_globs)
      script->add_exclude_glob(glob);
  }

  return script;
}

std::unique_ptr<extensions::UserScriptList> ParseContentScripts(
    const std::vector<ContentScriptDetails>& content_script_list,
    const extensions::Extension* extension,
    const extensions::mojom::HostID& host_id,
    bool incognito_enabled,
    const GURL& owner_base_url,
    std::string* error) {
  if (content_script_list.empty())
    return nullptr;

  std::unique_ptr<extensions::UserScriptList> result(
      new extensions::UserScriptList());
  std::set<std::string> names;
  for (const ContentScriptDetails& script_value : content_script_list) {
    const std::string& name = script_value.name;
    if (!names.insert(name).second) {
      // The name was already in the list.
      *error = kDuplicatedContentScriptNamesError;
      return nullptr;
    }

    std::unique_ptr<extensions::UserScript> script =
        ParseContentScript(script_value, extension, owner_base_url, error);
    if (!script)
      return nullptr;
    script->set_id(UserScript::GenerateUserScriptID());
    script->set_name(name);
    script->set_incognito_enabled(incognito_enabled);
    script->set_host_id(host_id);
    script->set_consumer_instance_type(extensions::UserScript::WEBVIEW);
    result->push_back(std::move(script));
  }
  return result;
}

}  // namespace

namespace extensions {

bool WebViewInternalExtensionFunction::PreRunValidation(std::string* error) {
  if (!ExtensionFunction::PreRunValidation(error))
    return false;

  EXTENSION_FUNCTION_PRERUN_VALIDATE(args().size() >= 1);
  const auto& instance_id_value = args()[0];
  EXTENSION_FUNCTION_PRERUN_VALIDATE(instance_id_value.is_int());
  instance_id_ = instance_id_value.GetInt();
  // TODO(crbug.com/41353094): Remove crash key once the cause of the kill is
  // known.
  static crash_reporter::CrashKeyString<128> name_key("webview-function");
  crash_reporter::ScopedCrashKeyString name_key_scope(&name_key, name());
  if (!WebViewGuest::FromInstanceID(source_process_id(), instance_id_)) {
    *error = "Could not find guest";
    return false;
  }
  return true;
}

WebViewGuest& WebViewInternalExtensionFunction::GetGuest() {
  return CHECK_DEREF(
      WebViewGuest::FromInstanceID(source_process_id(), instance_id_));
}

WebViewInternalCaptureVisibleRegionFunction::
    WebViewInternalCaptureVisibleRegionFunction()
    : is_guest_transparent_(false) {}

ExtensionFunction::ResponseAction
WebViewInternalCaptureVisibleRegionFunction::Run() {
  using api::extension_types::ImageDetails;

  std::optional<web_view_internal::CaptureVisibleRegion::Params> params =
      web_view_internal::CaptureVisibleRegion::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::optional<ImageDetails> image_details;
  if (args().size() > 1) {
    image_details = ImageDetails::FromValue(args()[1]);
  }

  WebViewGuest& guest = GetGuest();
  is_guest_transparent_ = guest.allow_transparency();
  const CaptureResult capture_result = CaptureAsync(
      guest.web_contents(), base::OptionalToPtr(image_details),
      base::BindOnce(
          &WebViewInternalCaptureVisibleRegionFunction::CopyFromSurfaceComplete,
          this));
  if (capture_result == OK) {
    // CaptureAsync may have responded synchronously.
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  return RespondNow(Error(GetErrorMessage(capture_result)));
}

void WebViewInternalCaptureVisibleRegionFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  constexpr base::TimeDelta kSecond = base::Seconds(1);
  QuotaLimitHeuristic::Config limit = {
      web_view_internal::MAX_CAPTURE_VISIBLE_REGION_CALLS_PER_SECOND, kSecond};

  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      limit, std::make_unique<QuotaLimitHeuristic::SingletonBucketMapper>(),
      "MAX_CAPTURE_VISIBLE_REGION_CALLS_PER_SECOND"));
}

bool WebViewInternalCaptureVisibleRegionFunction::ShouldSkipQuotaLimiting()
    const {
  return user_gesture();
}

WebContentsCaptureClient::ScreenshotAccess
WebViewInternalCaptureVisibleRegionFunction::GetScreenshotAccess(
    content::WebContents* web_contents) const {
  if (ExtensionsBrowserClient::Get()->IsScreenshotRestricted(web_contents))
    return ScreenshotAccess::kDisabledByDlp;

  return ScreenshotAccess::kEnabled;
}

bool WebViewInternalCaptureVisibleRegionFunction::ClientAllowsTransparency() {
  return is_guest_transparent_;
}

void WebViewInternalCaptureVisibleRegionFunction::OnCaptureSuccess(
    const SkBitmap& bitmap) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&WebViewInternalCaptureVisibleRegionFunction::
                         EncodeBitmapOnWorkerThread,
                     this, base::SingleThreadTaskRunner::GetCurrentDefault(),
                     bitmap));
}

void WebViewInternalCaptureVisibleRegionFunction::EncodeBitmapOnWorkerThread(
    scoped_refptr<base::TaskRunner> reply_task_runner,
    const SkBitmap& bitmap) {
  std::string base64_result;
  bool success = EncodeBitmap(bitmap, &base64_result);
  reply_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&WebViewInternalCaptureVisibleRegionFunction::
                                    OnBitmapEncodedOnUIThread,
                                this, success, std::move(base64_result)));
}

void WebViewInternalCaptureVisibleRegionFunction::OnBitmapEncodedOnUIThread(
    bool success,
    std::string base64_result) {
  if (!success) {
    OnCaptureFailure(FAILURE_REASON_ENCODING_FAILED);
    return;
  }

  Respond(WithArguments(std::move(base64_result)));
}

void WebViewInternalCaptureVisibleRegionFunction::OnCaptureFailure(
    CaptureResult result) {
  Respond(Error(GetErrorMessage(result)));
}

std::string WebViewInternalCaptureVisibleRegionFunction::GetErrorMessage(
    CaptureResult result) {
  const char* reason_description = "internal error";
  switch (result) {
    case FAILURE_REASON_READBACK_FAILED:
      reason_description = "image readback failed";
      break;
    case FAILURE_REASON_ENCODING_FAILED:
      reason_description = "encoding failed";
      break;
    case FAILURE_REASON_VIEW_INVISIBLE:
      reason_description = "view is invisible";
      break;
    case FAILURE_REASON_SCREEN_SHOTS_DISABLED:
    case FAILURE_REASON_SCREEN_SHOTS_DISABLED_BY_DLP:
      reason_description = "screenshot has been disabled";
      break;
    case OK:
      NOTREACHED_IN_MIGRATION()
          << "GetErrorMessage should not be called with a successful result";
      return "";
  }
  return ErrorUtils::FormatErrorMessage("Failed to capture webview: *",
                                        reason_description);
}

ExtensionFunction::ResponseAction WebViewInternalNavigateFunction::Run() {
  std::optional<web_view_internal::Navigate::Params> params =
      web_view_internal::Navigate::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string src = params->src;
  GetGuest().NavigateGuest(src, /*navigation_handle_callback=*/{},
                           true /* force_navigation */);
  return RespondNow(NoArguments());
}

WebViewInternalExecuteCodeFunction::WebViewInternalExecuteCodeFunction()
    : guest_instance_id_(0) {}

WebViewInternalExecuteCodeFunction::~WebViewInternalExecuteCodeFunction() {
}

ExecuteCodeFunction::InitResult WebViewInternalExecuteCodeFunction::Init() {
  if (init_result_)
    return init_result_.value();

  if (args().size() < 3)
    return set_init_result(VALIDATION_FAILURE);

  guest_instance_id_ = args()[0].GetIfInt().value_or(0);
  if (guest_instance_id_ == 0)
    return set_init_result(VALIDATION_FAILURE);

  const std::string* src = args()[1].GetIfString();
  if (!src)
    return set_init_result(VALIDATION_FAILURE);

  // Set |guest_src_| here, but do not return false if it is invalid.
  // Instead, let it continue with the normal page load sequence,
  // which will result in the usual LOAD_ABORT event in the case where
  // the URL is invalid.
  guest_src_ = GURL(*src);

  if (args().size() <= 2 || !args()[2].is_dict())
    return set_init_result(VALIDATION_FAILURE);
  auto details = InjectDetails::FromValue(args()[2]);
  if (!details) {
    return set_init_result(VALIDATION_FAILURE);
  }

  details_ = std::move(details);

  std::optional<extensions::mojom::HostID> host_id =
      GenerateHostIDFromEmbedder(extension(), render_frame_host());
  if (!host_id) {
    return set_init_result(VALIDATION_FAILURE);
  }
  set_host_id(std::move(*host_id));
  return set_init_result(SUCCESS);
}

bool WebViewInternalExecuteCodeFunction::ShouldInsertCSS() const {
  return false;
}

bool WebViewInternalExecuteCodeFunction::ShouldRemoveCSS() const {
  return false;
}

bool WebViewInternalExecuteCodeFunction::CanExecuteScriptOnPage(
    std::string* error) {
  return true;
}

extensions::ScriptExecutor*
WebViewInternalExecuteCodeFunction::GetScriptExecutor(std::string* error) {
  WebViewGuest* guest =
      WebViewGuest::FromInstanceID(source_process_id(), guest_instance_id_);
  if (!guest)
    return nullptr;

  return guest->script_executor();
}

bool WebViewInternalExecuteCodeFunction::IsWebView() const {
  return true;
}

const GURL& WebViewInternalExecuteCodeFunction::GetWebViewSrc() const {
  return guest_src_;
}

bool WebViewInternalExecuteCodeFunction::LoadFileForEmbedder(
    const std::string& file_src,
    LoadFileCallback callback) {
  WebViewGuest* guest =
      WebViewGuest::FromInstanceID(source_process_id(), guest_instance_id_);
  if (!guest || host_id().type == mojom::HostID::HostType::kExtensions) {
    return false;
  }

  GURL owner_base_url(guest->GetOwnerSiteURL().GetWithEmptyPath());
  GURL file_url(owner_base_url.Resolve(file_src));

  switch (host_id().type) {
    case mojom::HostID::HostType::kExtensions:
      NOTREACHED_IN_MIGRATION();
      return false;
    case mojom::HostID::HostType::kControlledFrameEmbedder:
      url_fetcher_ = std::make_unique<ControlledFrameEmbedderURLFetcher>(
          source_process_id(), render_frame_host()->GetRoutingID(), file_url,
          std::move(callback));
      break;
    case mojom::HostID::HostType::kWebUi:
      url_fetcher_ = std::make_unique<WebUIURLFetcher>(
          source_process_id(), render_frame_host()->GetRoutingID(), file_url,
          std::move(callback));
      break;
  }
  url_fetcher_->Start();

  return true;
}

void WebViewInternalExecuteCodeFunction::DidLoadFileForEmbedder(
    const std::string& file,
    bool success,
    std::unique_ptr<std::string> data) {
  std::vector<std::unique_ptr<std::string>> data_list;
  std::optional<std::string> error;
  if (success) {
    DCHECK(data);
    data_list.push_back(std::move(data));
  } else {
    error = base::StringPrintf("Failed to load file '%s'.", file.c_str());
  }

  DidLoadAndLocalizeFile(file, std::move(data_list), std::move(error));
}

bool WebViewInternalExecuteCodeFunction::LoadFile(const std::string& file,
                                                  std::string* error) {
  if (!extension()) {
    if (LoadFileForEmbedder(
            *details_->file,
            base::BindOnce(
                &WebViewInternalExecuteCodeFunction::DidLoadFileForEmbedder,
                this, file))) {
      return true;
    }

    *error = ErrorUtils::FormatErrorMessage(kLoadFileError, file);
    return false;
  }
  return ExecuteCodeFunction::LoadFile(file, error);
}

WebViewInternalExecuteScriptFunction::WebViewInternalExecuteScriptFunction() {
}

WebViewInternalInsertCSSFunction::WebViewInternalInsertCSSFunction() {
}

bool WebViewInternalInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

WebViewInternalAddContentScriptsFunction::
    WebViewInternalAddContentScriptsFunction() {
}

WebViewInternalAddContentScriptsFunction::
    ~WebViewInternalAddContentScriptsFunction() {
}

ExecuteCodeFunction::ResponseAction
WebViewInternalAddContentScriptsFunction::Run() {
  std::optional<web_view_internal::AddContentScripts::Params> params =
      web_view_internal::AddContentScripts::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!params->instance_id)
    return RespondNow(Error(kViewInstanceIdError));

  GURL owner_base_url(
      render_frame_host()->GetSiteInstance()->GetSiteURL().GetWithEmptyPath());
  std::optional<extensions::mojom::HostID> host_id =
      GenerateHostIDFromEmbedder(extension(), render_frame_host());
  if (!host_id) {
    return RespondNow(Error(kHostIDError));
  }
  bool incognito_enabled = browser_context()->IsOffTheRecord();

  std::string error;
  std::unique_ptr<UserScriptList> result =
      ParseContentScripts(params->content_script_list, extension(), *host_id,
                          incognito_enabled, owner_base_url, &error);
  if (!result)
    return RespondNow(Error(std::move(error)));

  WebViewContentScriptManager* manager =
      WebViewContentScriptManager::Get(browser_context());
  DCHECK(manager);

  manager->AddContentScripts(source_process_id(), render_frame_host(),
                             params->instance_id, std::move(*host_id),
                             std::move(*result));

  return RespondNow(NoArguments());
}

WebViewInternalRemoveContentScriptsFunction::
    WebViewInternalRemoveContentScriptsFunction() {
}

WebViewInternalRemoveContentScriptsFunction::
    ~WebViewInternalRemoveContentScriptsFunction() {
}

ExecuteCodeFunction::ResponseAction
WebViewInternalRemoveContentScriptsFunction::Run() {
  std::optional<web_view_internal::RemoveContentScripts::Params> params =
      web_view_internal::RemoveContentScripts::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!params->instance_id)
    return RespondNow(Error(kViewInstanceIdError));

  WebViewContentScriptManager* manager =
      WebViewContentScriptManager::Get(browser_context());
  DCHECK(manager);

  std::optional<extensions::mojom::HostID> host_id =
      GenerateHostIDFromEmbedder(extension(), render_frame_host());
  if (!host_id) {
    return RespondNow(Error(kHostIDError));
  }

  std::vector<std::string> script_name_list;
  if (params->script_name_list)
    script_name_list.swap(*params->script_name_list);
  manager->RemoveContentScripts(source_process_id(), params->instance_id,
                                std::move(*host_id), script_name_list);
  return RespondNow(NoArguments());
}

WebViewInternalSetNameFunction::WebViewInternalSetNameFunction() {
}

WebViewInternalSetNameFunction::~WebViewInternalSetNameFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalSetNameFunction::Run() {
  std::optional<web_view_internal::SetName::Params> params =
      web_view_internal::SetName::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  GetGuest().SetName(params->frame_name);
  return RespondNow(NoArguments());
}

WebViewInternalSetAllowTransparencyFunction::
WebViewInternalSetAllowTransparencyFunction() {
}

WebViewInternalSetAllowTransparencyFunction::
~WebViewInternalSetAllowTransparencyFunction() {
}

ExtensionFunction::ResponseAction
WebViewInternalSetAllowTransparencyFunction::Run() {
  std::optional<web_view_internal::SetAllowTransparency::Params> params =
      web_view_internal::SetAllowTransparency::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  GetGuest().SetAllowTransparency(params->allow);
  return RespondNow(NoArguments());
}

WebViewInternalSetAllowScalingFunction::
    WebViewInternalSetAllowScalingFunction() {
}

WebViewInternalSetAllowScalingFunction::
    ~WebViewInternalSetAllowScalingFunction() {
}

ExtensionFunction::ResponseAction
WebViewInternalSetAllowScalingFunction::Run() {
  std::optional<web_view_internal::SetAllowScaling::Params> params =
      web_view_internal::SetAllowScaling::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  GetGuest().SetAllowScaling(params->allow);
  return RespondNow(NoArguments());
}

WebViewInternalSetZoomFunction::WebViewInternalSetZoomFunction() {
}

WebViewInternalSetZoomFunction::~WebViewInternalSetZoomFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalSetZoomFunction::Run() {
  std::optional<web_view_internal::SetZoom::Params> params =
      web_view_internal::SetZoom::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  GetGuest().SetZoom(params->zoom_factor);
  return RespondNow(NoArguments());
}

WebViewInternalGetZoomFunction::WebViewInternalGetZoomFunction() {
}

WebViewInternalGetZoomFunction::~WebViewInternalGetZoomFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalGetZoomFunction::Run() {
  std::optional<web_view_internal::GetZoom::Params> params =
      web_view_internal::GetZoom::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  double zoom_factor = GetGuest().GetZoom();
  return RespondNow(WithArguments(zoom_factor));
}

WebViewInternalSetZoomModeFunction::WebViewInternalSetZoomModeFunction() {
}

WebViewInternalSetZoomModeFunction::~WebViewInternalSetZoomModeFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalSetZoomModeFunction::Run() {
  std::optional<web_view_internal::SetZoomMode::Params> params =
      web_view_internal::SetZoomMode::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ZoomController::ZoomMode zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
  switch (params->zoom_mode) {
    case web_view_internal::ZoomMode::kPerOrigin:
      zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
      break;
    case web_view_internal::ZoomMode::kPerView:
      zoom_mode = ZoomController::ZOOM_MODE_ISOLATED;
      break;
    case web_view_internal::ZoomMode::kDisabled:
      zoom_mode = ZoomController::ZOOM_MODE_DISABLED;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  GetGuest().SetZoomMode(zoom_mode);
  return RespondNow(NoArguments());
}

WebViewInternalGetZoomModeFunction::WebViewInternalGetZoomModeFunction() {
}

WebViewInternalGetZoomModeFunction::~WebViewInternalGetZoomModeFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalGetZoomModeFunction::Run() {
  std::optional<web_view_internal::GetZoomMode::Params> params =
      web_view_internal::GetZoomMode::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  web_view_internal::ZoomMode zoom_mode = web_view_internal::ZoomMode::kNone;
  switch (GetGuest().GetZoomMode()) {
    case ZoomController::ZOOM_MODE_DEFAULT:
      zoom_mode = web_view_internal::ZoomMode::kPerOrigin;
      break;
    case ZoomController::ZOOM_MODE_ISOLATED:
      zoom_mode = web_view_internal::ZoomMode::kPerView;
      break;
    case ZoomController::ZOOM_MODE_DISABLED:
      zoom_mode = web_view_internal::ZoomMode::kDisabled;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  return RespondNow(WithArguments(web_view_internal::ToString(zoom_mode)));
}

WebViewInternalFindFunction::WebViewInternalFindFunction() {
}

WebViewInternalFindFunction::~WebViewInternalFindFunction() {
}

void WebViewInternalFindFunction::ForwardResponse(base::Value::Dict results) {
  Respond(WithArguments(std::move(results)));
}

ExtensionFunction::ResponseAction WebViewInternalFindFunction::Run() {
  std::optional<web_view_internal::Find::Params> params =
      web_view_internal::Find::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Convert the std::string search_text to string16.
  std::u16string search_text;
  base::UTF8ToUTF16(
      params->search_text.c_str(), params->search_text.length(), &search_text);

  // Set the find options to their default values.
  auto options = blink::mojom::FindOptions::New();
  if (params->options) {
    options->forward =
        params->options->backward ? !*params->options->backward : true;
    options->match_case =
        params->options->match_case ? *params->options->match_case : false;
  }

  GetGuest().StartFind(search_text, std::move(options), this);
  // It is possible that StartFind has already responded.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

WebViewInternalStopFindingFunction::WebViewInternalStopFindingFunction() {
}

WebViewInternalStopFindingFunction::~WebViewInternalStopFindingFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalStopFindingFunction::Run() {
  std::optional<web_view_internal::StopFinding::Params> params =
      web_view_internal::StopFinding::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Set the StopFindAction.
  content::StopFindAction action;
  switch (params->action) {
    case web_view_internal::StopFindingAction::kClear:
      action = content::STOP_FIND_ACTION_CLEAR_SELECTION;
      break;
    case web_view_internal::StopFindingAction::kKeep:
      action = content::STOP_FIND_ACTION_KEEP_SELECTION;
      break;
    case web_view_internal::StopFindingAction::kActivate:
      action = content::STOP_FIND_ACTION_ACTIVATE_SELECTION;
      break;
    default:
      action = content::STOP_FIND_ACTION_KEEP_SELECTION;
  }

  GetGuest().StopFinding(action);
  return RespondNow(NoArguments());
}

WebViewInternalLoadDataWithBaseUrlFunction::
    WebViewInternalLoadDataWithBaseUrlFunction() {
}

WebViewInternalLoadDataWithBaseUrlFunction::
    ~WebViewInternalLoadDataWithBaseUrlFunction() {
}

ExtensionFunction::ResponseAction
WebViewInternalLoadDataWithBaseUrlFunction::Run() {
  std::optional<web_view_internal::LoadDataWithBaseUrl::Params> params =
      web_view_internal::LoadDataWithBaseUrl::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Check that the provided URLs are valid.
  // |data_url| must be a valid data URL.
  const GURL data_url(params->data_url);
  if (!data_url.is_valid() || !data_url.SchemeIs(url::kDataScheme)) {
    return RespondNow(ExtensionFunction::Error(
        "Invalid data URL \"*\".", data_url.possibly_invalid_spec()));
  }

  // |base_url| must be a valid URL. It is also limited to URLs that the owner
  // is trusted to have control over.
  WebViewGuest& guest = GetGuest();
  const url::Origin& owner_origin = guest.owner_rfh()->GetLastCommittedOrigin();
  const GURL base_url(params->base_url);
  const bool base_in_owner_origin = owner_origin.IsSameOriginWith(base_url);
  if (!base_url.is_valid() ||
      (!base_url.SchemeIsHTTPOrHTTPS() && !base_in_owner_origin)) {
    return RespondNow(ExtensionFunction::Error(
        "Invalid base URL \"*\".", base_url.possibly_invalid_spec()));
  }

  // If a virtual URL was provided, use it. Otherwise, the user will be shown
  // the data URL.
  const GURL virtual_url(params->virtual_url ? *params->virtual_url
                                             : params->data_url);
  // |virtual_url| must be a valid URL.
  if (!virtual_url.is_valid()) {
    return RespondNow(ExtensionFunction::Error(
        "Invalid virtual URL \"*\".", virtual_url.possibly_invalid_spec()));
  }

  // Set up the parameters to load |data_url| with the specified |base_url|.
  content::NavigationController::LoadURLParams load_params(data_url);
  load_params.load_type = content::NavigationController::LOAD_TYPE_DATA;
  load_params.base_url_for_data_url = base_url;
  load_params.virtual_url_for_special_cases = virtual_url;
  load_params.override_user_agent =
      content::NavigationController::UA_OVERRIDE_INHERIT;

  // Navigate to the data URL.
  guest.GetController().LoadURLWithParams(load_params);

  return RespondNow(ExtensionFunction::NoArguments());
}

WebViewInternalGoFunction::WebViewInternalGoFunction() {
}

WebViewInternalGoFunction::~WebViewInternalGoFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalGoFunction::Run() {
  std::optional<web_view_internal::Go::Params> params =
      web_view_internal::Go::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool successful = GetGuest().Go(params->relative_index);
  return RespondNow(WithArguments(successful));
}

WebViewInternalReloadFunction::WebViewInternalReloadFunction() {
}

WebViewInternalReloadFunction::~WebViewInternalReloadFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalReloadFunction::Run() {
  GetGuest().Reload();
  return RespondNow(NoArguments());
}

WebViewInternalSetPermissionFunction::WebViewInternalSetPermissionFunction() {
}

WebViewInternalSetPermissionFunction::~WebViewInternalSetPermissionFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalSetPermissionFunction::Run() {
  std::optional<web_view_internal::SetPermission::Params> params =
      web_view_internal::SetPermission::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  WebViewPermissionHelper::PermissionResponseAction action =
      WebViewPermissionHelper::DEFAULT;
  switch (params->action) {
    case api::web_view_internal::SetPermissionAction::kAllow:
      action = WebViewPermissionHelper::ALLOW;
      break;
    case api::web_view_internal::SetPermissionAction::kDeny:
      action = WebViewPermissionHelper::DENY;
      break;
    case api::web_view_internal::SetPermissionAction::kDefault:
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  std::string user_input;
  if (params->user_input)
    user_input = *params->user_input;

  WebViewPermissionHelper* web_view_permission_helper =
      GetGuest().web_view_permission_helper();

  WebViewPermissionHelper::SetPermissionResult result =
      web_view_permission_helper->SetPermission(
          params->request_id, action, user_input);

  EXTENSION_FUNCTION_VALIDATE(result !=
                              WebViewPermissionHelper::SET_PERMISSION_INVALID);

  return RespondNow(
      WithArguments(result == WebViewPermissionHelper::SET_PERMISSION_ALLOWED));
}

WebViewInternalOverrideUserAgentFunction::
    WebViewInternalOverrideUserAgentFunction() {
}

WebViewInternalOverrideUserAgentFunction::
    ~WebViewInternalOverrideUserAgentFunction() {
}

ExtensionFunction::ResponseAction
WebViewInternalOverrideUserAgentFunction::Run() {
  std::optional<web_view_internal::OverrideUserAgent::Params> params =
      web_view_internal::OverrideUserAgent::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetGuest().SetUserAgentOverride(params->user_agent_override);
  return RespondNow(NoArguments());
}

WebViewInternalStopFunction::WebViewInternalStopFunction() {
}

WebViewInternalStopFunction::~WebViewInternalStopFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalStopFunction::Run() {
  GetGuest().Stop();
  return RespondNow(NoArguments());
}

WebViewInternalSetAudioMutedFunction::WebViewInternalSetAudioMutedFunction() =
    default;

WebViewInternalSetAudioMutedFunction::~WebViewInternalSetAudioMutedFunction() =
    default;

ExtensionFunction::ResponseAction WebViewInternalSetAudioMutedFunction::Run() {
  std::optional<web_view_internal::SetAudioMuted::Params> params =
      web_view_internal::SetAudioMuted::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetGuest().SetAudioMuted(params->mute);
  return RespondNow(NoArguments());
}

WebViewInternalIsAudioMutedFunction::WebViewInternalIsAudioMutedFunction() =
    default;

WebViewInternalIsAudioMutedFunction::~WebViewInternalIsAudioMutedFunction() =
    default;

ExtensionFunction::ResponseAction WebViewInternalIsAudioMutedFunction::Run() {
  std::optional<web_view_internal::IsAudioMuted::Params> params =
      web_view_internal::IsAudioMuted::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  return RespondNow(WithArguments(GetGuest().IsAudioMuted()));
}

WebViewInternalGetAudioStateFunction::WebViewInternalGetAudioStateFunction() =
    default;

WebViewInternalGetAudioStateFunction::~WebViewInternalGetAudioStateFunction() =
    default;

ExtensionFunction::ResponseAction WebViewInternalGetAudioStateFunction::Run() {
  std::optional<web_view_internal::GetAudioState::Params> params =
      web_view_internal::GetAudioState::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::WebContents* web_contents = GetGuest().web_contents();
  return RespondNow(WithArguments(web_contents->IsCurrentlyAudible()));
}

WebViewInternalTerminateFunction::WebViewInternalTerminateFunction() {
}

WebViewInternalTerminateFunction::~WebViewInternalTerminateFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalTerminateFunction::Run() {
  GetGuest().Terminate();
  return RespondNow(NoArguments());
}

WebViewInternalClearDataFunction::WebViewInternalClearDataFunction()
    : remove_mask_(0), bad_message_(false) {
}

WebViewInternalClearDataFunction::~WebViewInternalClearDataFunction() {
}

WebViewInternalSetSpatialNavigationEnabledFunction::
    WebViewInternalSetSpatialNavigationEnabledFunction() {}

WebViewInternalSetSpatialNavigationEnabledFunction::
    ~WebViewInternalSetSpatialNavigationEnabledFunction() {}

ExtensionFunction::ResponseAction
WebViewInternalSetSpatialNavigationEnabledFunction::Run() {
  std::optional<web_view_internal::SetSpatialNavigationEnabled::Params> params =
      web_view_internal::SetSpatialNavigationEnabled::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetGuest().SetSpatialNavigationEnabled(params->spatial_nav_enabled);
  return RespondNow(NoArguments());
}

WebViewInternalIsSpatialNavigationEnabledFunction::
    WebViewInternalIsSpatialNavigationEnabledFunction() {}

WebViewInternalIsSpatialNavigationEnabledFunction::
    ~WebViewInternalIsSpatialNavigationEnabledFunction() {}

ExtensionFunction::ResponseAction
WebViewInternalIsSpatialNavigationEnabledFunction::Run() {
  std::optional<web_view_internal::IsSpatialNavigationEnabled::Params> params =
      web_view_internal::IsSpatialNavigationEnabled::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  return RespondNow(WithArguments(GetGuest().IsSpatialNavigationEnabled()));
}

// Parses the |dataToRemove| argument to generate the remove mask. Sets
// |bad_message_| (like EXTENSION_FUNCTION_VALIDATE would if this were a bool
// method) if 'dataToRemove' is not present.
uint32_t WebViewInternalClearDataFunction::GetRemovalMask() {
  if (args().size() <= 2 || !args()[2].is_dict()) {
    bad_message_ = true;
    return 0;
  }

  uint32_t remove_mask = 0;
  for (const auto kv : args()[2].GetDict()) {
    if (!kv.second.is_bool()) {
      bad_message_ = true;
      return 0;
    }
    if (kv.second.GetBool())
      remove_mask |= MaskForKey(kv.first.c_str());
  }

  return remove_mask;
}

// TODO(lazyboy): Parameters in this extension function are similar (or a
// sub-set) to BrowsingDataRemoverFunction. How can we share this code?
ExtensionFunction::ResponseAction WebViewInternalClearDataFunction::Run() {
  // Grab the initial |options| parameter, and parse out the arguments.
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  const base::Value& options = args()[1];
  EXTENSION_FUNCTION_VALIDATE(options.is_dict());

  // If |ms_since_epoch| isn't set, default it to 0.
  double ms_since_epoch = options.GetDict().FindDouble(kSinceKey).value_or(0);
  remove_since_ = base::Time::FromMillisecondsSinceUnixEpoch(ms_since_epoch);

  remove_mask_ = GetRemovalMask();
  if (bad_message_)
    return RespondNow(Error(kUnknownErrorDoNotUse));

  AddRef();  // Balanced below or in WebViewInternalClearDataFunction::Done().

  bool scheduled = false;
  if (remove_mask_) {
    scheduled = GetGuest().ClearData(
        remove_since_, remove_mask_,
        base::BindOnce(&WebViewInternalClearDataFunction::ClearDataDone, this));
  }
  if (!remove_mask_ || !scheduled) {
    Release();  // Balanced above.
    return RespondNow(Error(kUnknownErrorDoNotUse));
  }

  // Will finish asynchronously.
  return RespondLater();
}

void WebViewInternalClearDataFunction::ClearDataDone() {
  Release();  // Balanced in RunAsync().
  Respond(NoArguments());
}

}  // namespace extensions
