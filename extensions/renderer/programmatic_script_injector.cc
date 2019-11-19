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
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/injection_host.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"

namespace extensions {

ProgrammaticScriptInjector::ProgrammaticScriptInjector(
    const ExtensionMsg_ExecuteCode_Params& params)
    : params_(new ExtensionMsg_ExecuteCode_Params(params)),
      finished_(false) {
}

ProgrammaticScriptInjector::~ProgrammaticScriptInjector() {
}

UserScript::InjectionType ProgrammaticScriptInjector::script_type()
    const {
  return UserScript::PROGRAMMATIC_SCRIPT;
}

bool ProgrammaticScriptInjector::IsUserGesture() const {
  return params_->user_gesture;
}

base::Optional<CSSOrigin> ProgrammaticScriptInjector::GetCssOrigin() const {
  return params_->css_origin;
}

const base::Optional<std::string>
ProgrammaticScriptInjector::GetInjectionKey() const {
  return params_->injection_key;
}

bool ProgrammaticScriptInjector::ExpectsResults() const {
  return params_->wants_result;
}

bool ProgrammaticScriptInjector::ShouldInjectJs(
    UserScript::RunLocation run_location,
    const std::set<std::string>& executing_scripts) const {
  return params_->run_at == run_location && params_->is_javascript;
}

bool ProgrammaticScriptInjector::ShouldInjectCss(
    UserScript::RunLocation run_location,
    const std::set<std::string>& injected_stylesheets) const {
  return params_->run_at == run_location && !params_->is_javascript;
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
  GURL effective_document_url = ScriptContext::GetEffectiveDocumentURL(
      frame, frame->GetDocument().Url(), params_->match_about_blank);
  if (params_->is_web_view) {
    if (frame->Parent()) {
      // This is a subframe inside <webview>, so allow it.
      return PermissionsData::PageAccess::kAllowed;
    }

    return effective_document_url == params_->webview_src
               ? PermissionsData::PageAccess::kAllowed
               : PermissionsData::PageAccess::kDenied;
  }
  DCHECK_EQ(injection_host->id().type(), HostID::EXTENSIONS);

  return injection_host->CanExecuteOnFrame(
      effective_document_url,
      content::RenderFrame::FromWebFrame(frame),
      tab_id,
      true /* is_declarative */);
}

std::vector<blink::WebScriptSource> ProgrammaticScriptInjector::GetJsSources(
    UserScript::RunLocation run_location,
    std::set<std::string>* executing_scripts,
    size_t* num_injected_js_scripts) const {
  DCHECK_EQ(params_->run_at, run_location);
  DCHECK(params_->is_javascript);

  return std::vector<blink::WebScriptSource>(
      1, blink::WebScriptSource(blink::WebString::FromUTF8(params_->code),
                                params_->file_url));
}

std::vector<blink::WebString> ProgrammaticScriptInjector::GetCssSources(
    UserScript::RunLocation run_location,
    std::set<std::string>* injected_stylesheets,
    size_t* num_injected_stylesheets) const {
  DCHECK_EQ(params_->run_at, run_location);
  DCHECK(!params_->is_javascript);

  return std::vector<blink::WebString>(
      1, blink::WebString::FromUTF8(params_->code));
}

void ProgrammaticScriptInjector::OnInjectionComplete(
    std::unique_ptr<base::Value> execution_result,
    UserScript::RunLocation run_location,
    content::RenderFrame* render_frame) {
  DCHECK(results_.empty());
  if (execution_result)
    results_.Append(std::move(execution_result));
  Finish(std::string(), render_frame);
}

void ProgrammaticScriptInjector::OnWillNotInject(
    InjectFailureReason reason,
    content::RenderFrame* render_frame) {
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
  Finish(error, render_frame);
}

bool ProgrammaticScriptInjector::CanShowUrlInError() const {
  if (params_->host_id.type() != HostID::EXTENSIONS)
    return false;
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(params_->host_id.id());
  if (!extension)
    return false;
  return extension->permissions_data()->active_permissions().HasAPIPermission(
      APIPermission::kTab);
}

void ProgrammaticScriptInjector::Finish(const std::string& error,
                                        content::RenderFrame* render_frame) {
  DCHECK(!finished_);
  finished_ = true;

  // It's possible that the render frame was destroyed in the course of
  // injecting scripts. Don't respond if it was (the browser side watches for
  // frame deletions so nothing is left hanging).
  if (render_frame) {
    render_frame->Send(
        new ExtensionHostMsg_ExecuteCodeFinished(
            render_frame->GetRoutingID(), params_->request_id,
            error, url_, results_));
  }
}

}  // namespace extensions
