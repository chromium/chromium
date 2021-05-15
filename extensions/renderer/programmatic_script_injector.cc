// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/programmatic_script_injector.h"

#include <utility>
#include <vector>

#include "base/values.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/action_type.mojom-shared.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/script_constants.h"
#include "extensions/renderer/injection_host.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"

namespace extensions {

ProgrammaticScriptInjector::ProgrammaticScriptInjector(
    mojom::ExecuteCodeParamsPtr params,
    mojom::LocalFrame::ExecuteCodeCallback callback)
    : params_(std::move(params)), callback_(std::move(callback)) {}

ProgrammaticScriptInjector::~ProgrammaticScriptInjector() {
}

mojom::InjectionType ProgrammaticScriptInjector::script_type() const {
  return mojom::InjectionType::kProgrammaticScript;
}

bool ProgrammaticScriptInjector::IsUserGesture() const {
  return params_->user_gesture;
}

mojom::CSSOrigin ProgrammaticScriptInjector::GetCssOrigin() const {
  return params_->css_origin;
}

bool ProgrammaticScriptInjector::IsRemovingCSS() const {
  return params_->action_type == mojom::ActionType::kRemoveCss;
}

bool ProgrammaticScriptInjector::IsAddingCSS() const {
  return params_->action_type == mojom::ActionType::kAddCss;
}

const absl::optional<std::string> ProgrammaticScriptInjector::GetInjectionKey()
    const {
  return params_->injection_key;
}

bool ProgrammaticScriptInjector::ExpectsResults() const {
  return params_->wants_result;
}

bool ProgrammaticScriptInjector::ShouldInjectJs(
    mojom::RunLocation run_location,
    const std::set<std::string>& executing_scripts) const {
  return params_->run_at == run_location &&
         params_->action_type == mojom::ActionType::kAddJavascript;
}

bool ProgrammaticScriptInjector::ShouldInjectOrRemoveCss(
    mojom::RunLocation run_location,
    const std::set<std::string>& injected_stylesheets) const {
  return params_->run_at == run_location &&
         (params_->action_type == mojom::ActionType::kAddCss ||
          params_->action_type == mojom::ActionType::kRemoveCss);
}

PermissionsData::PageAccess ProgrammaticScriptInjector::CanExecuteOnFrame(
    const InjectionHost* injection_host,
    blink::WebLocalFrame* frame,
    int tab_id) {
  // Note: we calculate url_ now and not in the constructor because we don't
  // have the URL at that point when loads start. The browser issues the request
  // and only when it has a response does the renderer see the provisional data
  // source which the method below uses.
  url_ = ScriptContext::GetDocumentLoaderURLForFrame(frame);
  if (url_.SchemeIs(url::kAboutScheme)) {
    origin_for_about_error_ = frame->GetSecurityOrigin().ToString().Utf8();
  }
  GURL effective_document_url =
      ScriptContext::GetEffectiveDocumentURLForInjection(
          frame, frame->GetDocument().Url(),
          params_->match_about_blank
              ? MatchOriginAsFallbackBehavior::kMatchForAboutSchemeAndClimbTree
              : MatchOriginAsFallbackBehavior::kNever);
  if (params_->is_web_view) {
    if (frame->Parent()) {
      // This is a subframe inside <webview>, so allow it.
      return PermissionsData::PageAccess::kAllowed;
    }

    return effective_document_url == params_->webview_src
               ? PermissionsData::PageAccess::kAllowed
               : PermissionsData::PageAccess::kDenied;
  }
  DCHECK_EQ(injection_host->id().type, mojom::HostID::HostType::kExtensions);

  return injection_host->CanExecuteOnFrame(
      effective_document_url,
      content::RenderFrame::FromWebFrame(frame),
      tab_id,
      true /* is_declarative */);
}

std::vector<blink::WebScriptSource> ProgrammaticScriptInjector::GetJsSources(
    mojom::RunLocation run_location,
    std::set<std::string>* executing_scripts,
    size_t* num_injected_js_scripts) const {
  DCHECK_EQ(params_->run_at, run_location);
  DCHECK_EQ(params_->action_type, mojom::ActionType::kAddJavascript);

  return std::vector<blink::WebScriptSource>(
      1, blink::WebScriptSource(blink::WebString::FromUTF8(params_->code),
                                params_->script_url));
}

std::vector<blink::WebString> ProgrammaticScriptInjector::GetCssSources(
    mojom::RunLocation run_location,
    std::set<std::string>* injected_stylesheets,
    size_t* num_injected_stylesheets) const {
  DCHECK_EQ(params_->run_at, run_location);
  DCHECK(params_->action_type == mojom::ActionType::kAddCss ||
         params_->action_type == mojom::ActionType::kRemoveCss);

  return std::vector<blink::WebString>(
      1, blink::WebString::FromUTF8(params_->code));
}

void ProgrammaticScriptInjector::OnInjectionComplete(
    std::unique_ptr<base::Value> execution_result,
    mojom::RunLocation run_location) {
  DCHECK(!result_.has_value());
  if (execution_result) {
    result_ = base::Value::FromUniquePtrValue(std::move(execution_result));
  }
  Finish(std::string());
}

void ProgrammaticScriptInjector::OnWillNotInject(InjectFailureReason reason) {
  std::string error;
  switch (reason) {
    case NOT_ALLOWED:
      if (!CanShowUrlInError()) {
        error = manifest_errors::kCannotAccessPage;
      } else if (!origin_for_about_error_.empty()) {
        error = ErrorUtils::FormatErrorMessage(
            manifest_errors::kCannotAccessAboutUrl, url_.spec(),
            origin_for_about_error_);
      } else {
        error = ErrorUtils::FormatErrorMessage(
            manifest_errors::kCannotAccessPageWithUrl, url_.spec());
      }
      break;
    case EXTENSION_REMOVED:  // no special error here.
    case WONT_INJECT:
      break;
  }
  Finish(error);
}

bool ProgrammaticScriptInjector::CanShowUrlInError() const {
  if (params_->host_id->type != mojom::HostID::HostType::kExtensions)
    return false;
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(params_->host_id->id);
  if (!extension)
    return false;
  return extension->permissions_data()->active_permissions().HasAPIPermission(
      mojom::APIPermissionID::kTab);
}

void ProgrammaticScriptInjector::Finish(const std::string& error) {
  DCHECK(!finished_);
  finished_ = true;

  if (callback_)
    std::move(callback_).Run(error, url_, std::move(result_));
}

}  // namespace extensions
