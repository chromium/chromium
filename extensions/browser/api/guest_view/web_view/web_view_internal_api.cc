// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/guest_view/web_view/web_view_internal_api.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/crash/core/common/crash_key.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/stop_find_action.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_content_script_manager.h"
#include "extensions/common/api/web_view_internal.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"

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

const char kAppCacheKey[] = "appcache";
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
const char kViewInstanceIdError[] = "view_instance_id is missing.";
const char kDuplicatedContentScriptNamesError[] =
    "The given content script name already exists.";

const char kGeneratedScriptFilePrefix[] = "generated_script_file:";

uint32_t MaskForKey(const char* key) {
  if (strcmp(key, kAppCacheKey) == 0)
    return webview::WEB_VIEW_REMOVE_DATA_MASK_APPCACHE;
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

HostID GenerateHostIDFromEmbedder(const extensions::Extension* extension,
                                  content::WebContents* web_contents) {
  if (extension)
    return HostID(HostID::EXTENSIONS, extension->id());

  if (web_contents && web_contents->GetWebUI()) {
    const GURL& url = web_contents->GetSiteInstance()->GetSiteURL();
    return HostID(HostID::WEBUI, url.spec());
  }
  NOTREACHED();
  return HostID();
}

// Creates content script files when parsing InjectionItems of "js" or "css"
// proterties, and stores them in the |result|.
void ParseScriptFiles(const GURL& owner_base_url,
                      const extensions::Extension* extension,
                      const InjectionItems& items,
                      UserScript::FileList* list) {
  DCHECK(list->empty());
  list->reserve((items.files ? items.files->size() : 0) + (items.code ? 1 : 0));
  // files:
  if (items.files) {
    for (const std::string& relative : *items.files) {
      GURL url = owner_base_url.Resolve(relative);
      if (extension) {
        ExtensionResource resource = extension->GetResource(relative);

        list->push_back(std::make_unique<extensions::UserScript::File>(
            resource.extension_root(), resource.relative_path(), url));
      } else {
        list->push_back(std::make_unique<extensions::UserScript::File>(
            base::FilePath(), base::FilePath(), url));
      }
    }
  }
  // code:
  if (items.code) {
    GURL url = owner_base_url.Resolve(base::StringPrintf(
        "%s%s", kGeneratedScriptFilePrefix, base::GenerateGUID().c_str()));
    std::unique_ptr<extensions::UserScript::File> file(
        new extensions::UserScript::File(base::FilePath(), base::FilePath(),
                                         url));
    file->set_content(*items.code);
    list->push_back(std::move(file));
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
    return std::unique_ptr<extensions::UserScript>();

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
      return std::unique_ptr<extensions::UserScript>();
    }
    script->add_url_pattern(pattern);
  }

  // exclude_matches:
  if (script_value.exclude_matches) {
    const std::vector<std::string>& exclude_matches =
        *(script_value.exclude_matches.get());
    for (const std::string& exclude_match : exclude_matches) {
      URLPattern pattern(
          UserScript::ValidUserScriptSchemes(allowed_everywhere));

      if (pattern.Parse(exclude_match) != URLPattern::ParseResult::kSuccess) {
        *error = errors::kInvalidExcludeMatches;
        return std::unique_ptr<extensions::UserScript>();
      }
      script->add_exclude_url_pattern(pattern);
    }
  }
  // run_at:
  if (script_value.run_at) {
    UserScript::RunLocation run_at = UserScript::UNDEFINED;
    switch (script_value.run_at) {
      case extensions::api::extension_types::RUN_AT_NONE:
      case extensions::api::extension_types::RUN_AT_DOCUMENT_IDLE:
        run_at = UserScript::DOCUMENT_IDLE;
        break;
      case extensions::api::extension_types::RUN_AT_DOCUMENT_START:
        run_at = UserScript::DOCUMENT_START;
        break;
      case extensions::api::extension_types::RUN_AT_DOCUMENT_END:
        run_at = UserScript::DOCUMENT_END;
        break;
    }
    // The default for run_at is RUN_AT_DOCUMENT_IDLE.
    script->set_run_location(run_at);
  }

  // match_about_blank:
  if (script_value.match_about_blank)
    script->set_match_about_blank(*script_value.match_about_blank);

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
    const HostID& host_id,
    bool incognito_enabled,
    const GURL& owner_base_url,
    std::string* error) {
  if (content_script_list.empty())
    return std::unique_ptr<extensions::UserScriptList>();

  std::unique_ptr<extensions::UserScriptList> result(
      new extensions::UserScriptList());
  std::set<std::string> names;
  for (const ContentScriptDetails& script_value : content_script_list) {
    const std::string& name = script_value.name;
    if (!names.insert(name).second) {
      // The name was already in the list.
      *error = kDuplicatedContentScriptNamesError;
      return std::unique_ptr<extensions::UserScriptList>();
    }

    std::unique_ptr<extensions::UserScript> script =
        ParseContentScript(script_value, extension, owner_base_url, error);
    if (!script)
      return std::unique_ptr<extensions::UserScriptList>();
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

  int instance_id = 0;
  EXTENSION_FUNCTION_PRERUN_VALIDATE(args_->GetInteger(0, &instance_id));
  // TODO(780728): Remove crash key once the cause of the kill is known.
  static crash_reporter::CrashKeyString<128> name_key("webview-function");
  crash_reporter::ScopedCrashKeyString name_key_scope(&name_key, name());
  guest_ = WebViewGuest::From(source_process_id(), instance_id);
  if (!guest_) {
    *error = "Could not find guest";
    return false;
  }
  return true;
}

WebViewInternalCaptureVisibleRegionFunction::
    WebViewInternalCaptureVisibleRegionFunction()
    : is_guest_transparent_(false) {}

ExtensionFunction::ResponseAction
WebViewInternalCaptureVisibleRegionFunction::Run() {
  using api::extension_types::ImageDetails;

  std::unique_ptr<web_view_internal::CaptureVisibleRegion::Params> params(
      web_view_internal::CaptureVisibleRegion::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::unique_ptr<ImageDetails> image_details;
  if (args_->GetSize() > 1) {
    base::Value* spec = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->Get(1, &spec) && spec);
    image_details = ImageDetails::FromValue(*spec);
  }

  is_guest_transparent_ = guest_->allow_transparency();
  const CaptureResult capture_result = CaptureAsync(
      guest_->web_contents(), image_details.get(),
      base::BindOnce(
          &WebViewInternalCaptureVisibleRegionFunction::CopyFromSurfaceComplete,
          this));
  if (capture_result == OK) {
    // CaptureAsync may have responded synchronously.
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  return RespondNow(Error(GetErrorMessage(capture_result)));
}
bool WebViewInternalCaptureVisibleRegionFunction::IsScreenshotEnabled() const {
  // TODO(wjmaclean): Is it ok to always return true here?
  return true;
}

bool WebViewInternalCaptureVisibleRegionFunction::ClientAllowsTransparency() {
  return is_guest_transparent_;
}

void WebViewInternalCaptureVisibleRegionFunction::OnCaptureSuccess(
    const SkBitmap& bitmap) {
  std::string base64_result;
  if (!EncodeBitmap(bitmap, &base64_result)) {
    OnCaptureFailure(FAILURE_REASON_ENCODING_FAILED);
    return;
  }

  Respond(OneArgument(std::make_unique<base::Value>(std::move(base64_result))));
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
      NOTREACHED() << "WebViewInternalCaptureVisibleRegionFunction always have "
                      "screenshots enabled";
      break;
    case OK:
      NOTREACHED()
          << "GetErrorMessage should not be called with a successful result";
      return "";
  }
  return ErrorUtils::FormatErrorMessage("Failed to capture webview: *",
                                        reason_description);
}

ExtensionFunction::ResponseAction WebViewInternalNavigateFunction::Run() {
  std::unique_ptr<web_view_internal::Navigate::Params> params(
      web_view_internal::Navigate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  std::string src = params->src;
  guest_->NavigateGuest(src, true /* force_navigation */);
  return RespondNow(NoArguments());
}

WebViewInternalExecuteCodeFunction::WebViewInternalExecuteCodeFunction()
    : guest_instance_id_(0) {}

WebViewInternalExecuteCodeFunction::~WebViewInternalExecuteCodeFunction() {
}

ExecuteCodeFunction::InitResult WebViewInternalExecuteCodeFunction::Init() {
  if (init_result_)
    return init_result_.value();

  if (!args_->GetInteger(0, &guest_instance_id_) || !guest_instance_id_)
    return set_init_result(VALIDATION_FAILURE);

  std::string src;
  if (!args_->GetString(1, &src))
    return set_init_result(VALIDATION_FAILURE);

  // Set |guest_src_| here, but do not return false if it is invalid.
  // Instead, let it continue with the normal page load sequence,
  // which will result in the usual LOAD_ABORT event in the case where
  // the URL is invalid.
  guest_src_ = GURL(src);

  base::DictionaryValue* details_value = NULL;
  if (!args_->GetDictionary(2, &details_value))
    return set_init_result(VALIDATION_FAILURE);
  std::unique_ptr<InjectDetails> details(new InjectDetails());
  if (!InjectDetails::Populate(*details_value, details.get()))
    return set_init_result(VALIDATION_FAILURE);

  details_ = std::move(details);

  if (extension()) {
    set_host_id(HostID(HostID::EXTENSIONS, extension()->id()));
    return set_init_result(SUCCESS);
  }

  WebContents* web_contents = GetSenderWebContents();
  if (web_contents && web_contents->GetWebUI()) {
    const GURL& url = render_frame_host()->GetSiteInstance()->GetSiteURL();
    set_host_id(HostID(HostID::WEBUI, url.spec()));
    return set_init_result(SUCCESS);
  }
  return set_init_result_error("");  // TODO(lazyboy): error?
}

bool WebViewInternalExecuteCodeFunction::ShouldInsertCSS() const {
  return false;
}

bool WebViewInternalExecuteCodeFunction::CanExecuteScriptOnPage(
    std::string* error) {
  return true;
}

extensions::ScriptExecutor*
WebViewInternalExecuteCodeFunction::GetScriptExecutor(std::string* error) {
  WebViewGuest* guest =
      WebViewGuest::From(source_process_id(), guest_instance_id_);
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

bool WebViewInternalExecuteCodeFunction::LoadFileForWebUI(
    const std::string& file_src,
    WebUIURLFetcher::WebUILoadFileCallback callback) {
  WebViewGuest* guest =
      WebViewGuest::From(source_process_id(), guest_instance_id_);
  if (!guest || host_id().type() != HostID::WEBUI)
    return false;

  GURL owner_base_url(guest->GetOwnerSiteURL().GetWithEmptyPath());
  GURL file_url(owner_base_url.Resolve(file_src));

  url_fetcher_ = std::make_unique<WebUIURLFetcher>(
      source_process_id(), render_frame_host()->GetRoutingID(), file_url,
      std::move(callback));
  url_fetcher_->Start();
  return true;
}

bool WebViewInternalExecuteCodeFunction::LoadFile(const std::string& file,
                                                  std::string* error) {
  if (!extension()) {
    if (LoadFileForWebUI(
            *details_->file,
            base::Bind(
                &WebViewInternalExecuteCodeFunction::DidLoadAndLocalizeFile,
                this, file)))
      return true;

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
  std::unique_ptr<web_view_internal::AddContentScripts::Params> params(
      web_view_internal::AddContentScripts::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!params->instance_id)
    return RespondNow(Error(kViewInstanceIdError));

  GURL owner_base_url(
      render_frame_host()->GetSiteInstance()->GetSiteURL().GetWithEmptyPath());
  content::WebContents* sender_web_contents = GetSenderWebContents();
  HostID host_id = GenerateHostIDFromEmbedder(extension(), sender_web_contents);
  bool incognito_enabled = browser_context()->IsOffTheRecord();

  std::string error;
  std::unique_ptr<UserScriptList> result =
      ParseContentScripts(params->content_script_list, extension(), host_id,
                          incognito_enabled, owner_base_url, &error);
  if (!result)
    return RespondNow(Error(error));

  WebViewContentScriptManager* manager =
      WebViewContentScriptManager::Get(browser_context());
  DCHECK(manager);

  manager->AddContentScripts(source_process_id(), render_frame_host(),
                             params->instance_id, host_id, std::move(result));

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
  std::unique_ptr<web_view_internal::RemoveContentScripts::Params> params(
      web_view_internal::RemoveContentScripts::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!params->instance_id)
    return RespondNow(Error(kViewInstanceIdError));

  WebViewContentScriptManager* manager =
      WebViewContentScriptManager::Get(browser_context());
  DCHECK(manager);

  content::WebContents* sender_web_contents = GetSenderWebContents();
  HostID host_id = GenerateHostIDFromEmbedder(extension(), sender_web_contents);

  std::vector<std::string> script_name_list;
  if (params->script_name_list)
    script_name_list.swap(*params->script_name_list);
  manager->RemoveContentScripts(source_process_id(), params->instance_id,
                                host_id, script_name_list);
  return RespondNow(NoArguments());
}

WebViewInternalSetNameFunction::WebViewInternalSetNameFunction() {
}

WebViewInternalSetNameFunction::~WebViewInternalSetNameFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalSetNameFunction::Run() {
  std::unique_ptr<web_view_internal::SetName::Params> params(
      web_view_internal::SetName::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest_->SetName(params->frame_name);
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
  std::unique_ptr<web_view_internal::SetAllowTransparency::Params> params(
      web_view_internal::SetAllowTransparency::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest_->SetAllowTransparency(params->allow);
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
  std::unique_ptr<web_view_internal::SetAllowScaling::Params> params(
      web_view_internal::SetAllowScaling::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest_->SetAllowScaling(params->allow);
  return RespondNow(NoArguments());
}

WebViewInternalSetZoomFunction::WebViewInternalSetZoomFunction() {
}

WebViewInternalSetZoomFunction::~WebViewInternalSetZoomFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalSetZoomFunction::Run() {
  std::unique_ptr<web_view_internal::SetZoom::Params> params(
      web_view_internal::SetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest_->SetZoom(params->zoom_factor);
  return RespondNow(NoArguments());
}

WebViewInternalGetZoomFunction::WebViewInternalGetZoomFunction() {
}

WebViewInternalGetZoomFunction::~WebViewInternalGetZoomFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalGetZoomFunction::Run() {
  std::unique_ptr<web_view_internal::GetZoom::Params> params(
      web_view_internal::GetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  double zoom_factor = guest_->GetZoom();
  return RespondNow(OneArgument(std::make_unique<base::Value>(zoom_factor)));
}

WebViewInternalSetZoomModeFunction::WebViewInternalSetZoomModeFunction() {
}

WebViewInternalSetZoomModeFunction::~WebViewInternalSetZoomModeFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalSetZoomModeFunction::Run() {
  std::unique_ptr<web_view_internal::SetZoomMode::Params> params(
      web_view_internal::SetZoomMode::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ZoomController::ZoomMode zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
  switch (params->zoom_mode) {
    case web_view_internal::ZOOM_MODE_PER_ORIGIN:
      zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
      break;
    case web_view_internal::ZOOM_MODE_PER_VIEW:
      zoom_mode = ZoomController::ZOOM_MODE_ISOLATED;
      break;
    case web_view_internal::ZOOM_MODE_DISABLED:
      zoom_mode = ZoomController::ZOOM_MODE_DISABLED;
      break;
    default:
      NOTREACHED();
  }

  guest_->SetZoomMode(zoom_mode);
  return RespondNow(NoArguments());
}

WebViewInternalGetZoomModeFunction::WebViewInternalGetZoomModeFunction() {
}

WebViewInternalGetZoomModeFunction::~WebViewInternalGetZoomModeFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalGetZoomModeFunction::Run() {
  std::unique_ptr<web_view_internal::GetZoomMode::Params> params(
      web_view_internal::GetZoomMode::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  web_view_internal::ZoomMode zoom_mode = web_view_internal::ZOOM_MODE_NONE;
  switch (guest_->GetZoomMode()) {
    case ZoomController::ZOOM_MODE_DEFAULT:
      zoom_mode = web_view_internal::ZOOM_MODE_PER_ORIGIN;
      break;
    case ZoomController::ZOOM_MODE_ISOLATED:
      zoom_mode = web_view_internal::ZOOM_MODE_PER_VIEW;
      break;
    case ZoomController::ZOOM_MODE_DISABLED:
      zoom_mode = web_view_internal::ZOOM_MODE_DISABLED;
      break;
    default:
      NOTREACHED();
  }

  return RespondNow(OneArgument(
      std::make_unique<base::Value>(web_view_internal::ToString(zoom_mode))));
}

WebViewInternalFindFunction::WebViewInternalFindFunction() {
}

WebViewInternalFindFunction::~WebViewInternalFindFunction() {
}

void WebViewInternalFindFunction::ForwardResponse(
    const base::DictionaryValue& results) {
  Respond(OneArgument(results.CreateDeepCopy()));
}

ExtensionFunction::ResponseAction WebViewInternalFindFunction::Run() {
  std::unique_ptr<web_view_internal::Find::Params> params(
      web_view_internal::Find::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Convert the std::string search_text to string16.
  base::string16 search_text;
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

  guest_->StartFind(search_text, std::move(options), this);
  // It is possible that StartFind has already responded.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

WebViewInternalStopFindingFunction::WebViewInternalStopFindingFunction() {
}

WebViewInternalStopFindingFunction::~WebViewInternalStopFindingFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalStopFindingFunction::Run() {
  std::unique_ptr<web_view_internal::StopFinding::Params> params(
      web_view_internal::StopFinding::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Set the StopFindAction.
  content::StopFindAction action;
  switch (params->action) {
    case web_view_internal::STOP_FINDING_ACTION_CLEAR:
      action = content::STOP_FIND_ACTION_CLEAR_SELECTION;
      break;
    case web_view_internal::STOP_FINDING_ACTION_KEEP:
      action = content::STOP_FIND_ACTION_KEEP_SELECTION;
      break;
    case web_view_internal::STOP_FINDING_ACTION_ACTIVATE:
      action = content::STOP_FIND_ACTION_ACTIVATE_SELECTION;
      break;
    default:
      action = content::STOP_FIND_ACTION_KEEP_SELECTION;
  }

  guest_->StopFinding(action);
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
  std::unique_ptr<web_view_internal::LoadDataWithBaseUrl::Params> params(
      web_view_internal::LoadDataWithBaseUrl::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // If a virtual URL was provided, use it. Otherwise, the user will be shown
  // the data URL.
  std::string virtual_url =
      params->virtual_url ? *params->virtual_url : params->data_url;

  std::string error;
  bool successful = guest_->LoadDataWithBaseURL(
      params->data_url, params->base_url, virtual_url, &error);
  if (successful)
    return RespondNow(NoArguments());
  return RespondNow(Error(error));
}

WebViewInternalGoFunction::WebViewInternalGoFunction() {
}

WebViewInternalGoFunction::~WebViewInternalGoFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalGoFunction::Run() {
  std::unique_ptr<web_view_internal::Go::Params> params(
      web_view_internal::Go::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool successful = guest_->Go(params->relative_index);
  return RespondNow(OneArgument(std::make_unique<base::Value>(successful)));
}

WebViewInternalReloadFunction::WebViewInternalReloadFunction() {
}

WebViewInternalReloadFunction::~WebViewInternalReloadFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalReloadFunction::Run() {
  guest_->Reload();
  return RespondNow(NoArguments());
}

WebViewInternalSetPermissionFunction::WebViewInternalSetPermissionFunction() {
}

WebViewInternalSetPermissionFunction::~WebViewInternalSetPermissionFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalSetPermissionFunction::Run() {
  std::unique_ptr<web_view_internal::SetPermission::Params> params(
      web_view_internal::SetPermission::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebViewPermissionHelper::PermissionResponseAction action =
      WebViewPermissionHelper::DEFAULT;
  switch (params->action) {
    case api::web_view_internal::SET_PERMISSION_ACTION_ALLOW:
      action = WebViewPermissionHelper::ALLOW;
      break;
    case api::web_view_internal::SET_PERMISSION_ACTION_DENY:
      action = WebViewPermissionHelper::DENY;
      break;
    case api::web_view_internal::SET_PERMISSION_ACTION_DEFAULT:
      break;
    default:
      NOTREACHED();
  }

  std::string user_input;
  if (params->user_input)
    user_input = *params->user_input;

  WebViewPermissionHelper* web_view_permission_helper =
      WebViewPermissionHelper::FromWebContents(guest_->web_contents());

  WebViewPermissionHelper::SetPermissionResult result =
      web_view_permission_helper->SetPermission(
          params->request_id, action, user_input);

  EXTENSION_FUNCTION_VALIDATE(result !=
                              WebViewPermissionHelper::SET_PERMISSION_INVALID);

  return RespondNow(OneArgument(std::make_unique<base::Value>(
      result == WebViewPermissionHelper::SET_PERMISSION_ALLOWED)));
}

WebViewInternalOverrideUserAgentFunction::
    WebViewInternalOverrideUserAgentFunction() {
}

WebViewInternalOverrideUserAgentFunction::
    ~WebViewInternalOverrideUserAgentFunction() {
}

ExtensionFunction::ResponseAction
WebViewInternalOverrideUserAgentFunction::Run() {
  std::unique_ptr<web_view_internal::OverrideUserAgent::Params> params(
      web_view_internal::OverrideUserAgent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  guest_->SetUserAgentOverride(params->user_agent_override);
  return RespondNow(NoArguments());
}

WebViewInternalStopFunction::WebViewInternalStopFunction() {
}

WebViewInternalStopFunction::~WebViewInternalStopFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalStopFunction::Run() {
  guest_->Stop();
  return RespondNow(NoArguments());
}

WebViewInternalSetAudioMutedFunction::WebViewInternalSetAudioMutedFunction() {}

WebViewInternalSetAudioMutedFunction::~WebViewInternalSetAudioMutedFunction() {}

ExtensionFunction::ResponseAction WebViewInternalSetAudioMutedFunction::Run() {
  std::unique_ptr<web_view_internal::SetAudioMuted::Params> params(
      web_view_internal::SetAudioMuted::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  guest_->web_contents()->SetAudioMuted(params->mute);
  return RespondNow(NoArguments());
}

WebViewInternalIsAudioMutedFunction::WebViewInternalIsAudioMutedFunction() {}

WebViewInternalIsAudioMutedFunction::~WebViewInternalIsAudioMutedFunction() {}

ExtensionFunction::ResponseAction WebViewInternalIsAudioMutedFunction::Run() {
  std::unique_ptr<web_view_internal::IsAudioMuted::Params> params(
      web_view_internal::IsAudioMuted::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::WebContents* web_contents = guest_->web_contents();
  return RespondNow(
      OneArgument(std::make_unique<base::Value>(web_contents->IsAudioMuted())));
}

WebViewInternalGetAudioStateFunction::WebViewInternalGetAudioStateFunction() {}

WebViewInternalGetAudioStateFunction::~WebViewInternalGetAudioStateFunction() {}

ExtensionFunction::ResponseAction WebViewInternalGetAudioStateFunction::Run() {
  std::unique_ptr<web_view_internal::GetAudioState::Params> params(
      web_view_internal::GetAudioState::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::WebContents* web_contents = guest_->web_contents();
  return RespondNow(OneArgument(
      std::make_unique<base::Value>(web_contents->IsCurrentlyAudible())));
}

WebViewInternalTerminateFunction::WebViewInternalTerminateFunction() {
}

WebViewInternalTerminateFunction::~WebViewInternalTerminateFunction() {
}

ExtensionFunction::ResponseAction WebViewInternalTerminateFunction::Run() {
  guest_->Terminate();
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
  std::unique_ptr<web_view_internal::SetSpatialNavigationEnabled::Params>
      params(web_view_internal::SetSpatialNavigationEnabled::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  guest_->SetSpatialNavigationEnabled(params->spatial_nav_enabled);
  return RespondNow(NoArguments());
}

WebViewInternalIsSpatialNavigationEnabledFunction::
    WebViewInternalIsSpatialNavigationEnabledFunction() {}

WebViewInternalIsSpatialNavigationEnabledFunction::
    ~WebViewInternalIsSpatialNavigationEnabledFunction() {}

ExtensionFunction::ResponseAction
WebViewInternalIsSpatialNavigationEnabledFunction::Run() {
  std::unique_ptr<web_view_internal::IsSpatialNavigationEnabled::Params> params(
      web_view_internal::IsSpatialNavigationEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  return RespondNow(OneArgument(
      std::make_unique<base::Value>(guest_->IsSpatialNavigationEnabled())));
}

// Parses the |dataToRemove| argument to generate the remove mask. Sets
// |bad_message_| (like EXTENSION_FUNCTION_VALIDATE would if this were a bool
// method) if 'dataToRemove' is not present.
uint32_t WebViewInternalClearDataFunction::GetRemovalMask() {
  base::DictionaryValue* data_to_remove;
  if (!args_->GetDictionary(2, &data_to_remove)) {
    bad_message_ = true;
    return 0;
  }

  uint32_t remove_mask = 0;
  for (base::DictionaryValue::Iterator i(*data_to_remove); !i.IsAtEnd();
       i.Advance()) {
    bool selected = false;
    if (!i.value().GetAsBoolean(&selected)) {
      bad_message_ = true;
      return 0;
    }
    if (selected)
      remove_mask |= MaskForKey(i.key().c_str());
  }

  return remove_mask;
}

// TODO(lazyboy): Parameters in this extension function are similar (or a
// sub-set) to BrowsingDataRemoverFunction. How can we share this code?
ExtensionFunction::ResponseAction WebViewInternalClearDataFunction::Run() {
  // Grab the initial |options| parameter, and parse out the arguments.
  base::DictionaryValue* options;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &options));
  DCHECK(options);

  // If |ms_since_epoch| isn't set, default it to 0.
  double ms_since_epoch;
  if (!options->GetDouble(kSinceKey, &ms_since_epoch)) {
    ms_since_epoch = 0;
  }

  // base::Time takes a double that represents seconds since epoch. JavaScript
  // gives developers milliseconds, so do a quick conversion before populating
  // the object. Also, Time::FromDoubleT converts double time 0 to empty Time
  // object. So we need to do special handling here.
  remove_since_ = (ms_since_epoch == 0)
                      ? base::Time::UnixEpoch()
                      : base::Time::FromDoubleT(ms_since_epoch / 1000.0);

  remove_mask_ = GetRemovalMask();
  if (bad_message_)
    return RespondNow(Error(kUnknownErrorDoNotUse));

  AddRef();  // Balanced below or in WebViewInternalClearDataFunction::Done().

  bool scheduled = false;
  if (remove_mask_) {
    scheduled = guest_->ClearData(
        remove_since_, remove_mask_,
        base::Bind(&WebViewInternalClearDataFunction::ClearDataDone, this));
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
