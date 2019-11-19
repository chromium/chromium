// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/user_script_injector.h"

#include <tuple>
#include <vector>

#include "base/lazy_instance.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/guest_view/extensions_guest_view_messages.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/grit/extensions_renderer_resources.h"
#include "extensions/renderer/injection_host.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/scripts_run_info.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace extensions {

namespace {

struct RoutingInfoKey {
  int routing_id;
  int script_id;

  RoutingInfoKey(int routing_id, int script_id)
      : routing_id(routing_id), script_id(script_id) {}

  bool operator<(const RoutingInfoKey& other) const {
    return std::tie(routing_id, script_id) <
           std::tie(other.routing_id, other.script_id);
  }
};

using RoutingInfoMap = std::map<RoutingInfoKey, bool>;

// A map records whether a given |script_id| from a webview-added user script
// is allowed to inject on the render of given |routing_id|.
// Once a script is added, the decision of whether or not allowed to inject
// won't be changed.
// After removed by the webview, the user scipt will also be removed
// from the render. Therefore, there won't be any query from the same
// |script_id| and |routing_id| pair.
base::LazyInstance<RoutingInfoMap>::DestructorAtExit g_routing_info_map =
    LAZY_INSTANCE_INITIALIZER;

// Greasemonkey API source that is injected with the scripts.
struct GreasemonkeyApiJsString {
  GreasemonkeyApiJsString();
  blink::WebScriptSource GetSource() const;

 private:
  blink::WebString source_;
};

// The below constructor, monstrous as it is, just makes a WebScriptSource from
// the GreasemonkeyApiJs resource.
GreasemonkeyApiJsString::GreasemonkeyApiJsString() {
  base::StringPiece source_piece =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_GREASEMONKEY_API_JS);
  source_ =
      blink::WebString::FromUTF8(source_piece.data(), source_piece.length());
}

blink::WebScriptSource GreasemonkeyApiJsString::GetSource() const {
  return blink::WebScriptSource(source_);
}

base::LazyInstance<GreasemonkeyApiJsString>::Leaky g_greasemonkey_api =
    LAZY_INSTANCE_INITIALIZER;

bool ShouldInjectScripts(const UserScript::FileList& scripts,
                         const std::set<std::string>& injected_files) {
  for (const std::unique_ptr<UserScript::File>& file : scripts) {
    // Check if the script is already injected.
    if (injected_files.count(file->url().path()) == 0) {
      return true;
    }
  }
  return false;
}

}  // namespace

UserScriptInjector::UserScriptInjector(const UserScript* script,
                                       UserScriptSet* script_list,
                                       bool is_declarative)
    : script_(script),
      user_script_set_(script_list),
      script_id_(script_->id()),
      host_id_(script_->host_id()),
      is_declarative_(is_declarative),
      user_script_set_observer_(this) {
  user_script_set_observer_.Add(script_list);
}

UserScriptInjector::~UserScriptInjector() {
}

void UserScriptInjector::OnUserScriptsUpdated(
    const std::set<HostID>& changed_hosts,
    const UserScriptList& scripts) {
  // When user scripts are updated, all the old script pointers are invalidated.
  script_ = nullptr;
  // If the host causing this injection changed, then this injection
  // will be removed, and there's no guarantee the backing script still exists.
  if (changed_hosts.count(host_id_) > 0)
    return;

  for (const std::unique_ptr<UserScript>& script : scripts) {
    if (script->id() == script_id_) {
      script_ = script.get();
      break;
    }
  }
  // If |host_id_| wasn't in |changed_hosts|, then the script for this injection
  // should be guaranteed to exist.
  DCHECK(script_);
}

UserScript::InjectionType UserScriptInjector::script_type() const {
  return UserScript::CONTENT_SCRIPT;
}

bool UserScriptInjector::IsUserGesture() const {
  return false;
}

bool UserScriptInjector::ExpectsResults() const {
  return false;
}

base::Optional<CSSOrigin> UserScriptInjector::GetCssOrigin() const {
  return base::nullopt;
}

const base::Optional<std::string> UserScriptInjector::GetInjectionKey() const {
  return base::nullopt;
}

bool UserScriptInjector::ShouldInjectJs(
    UserScript::RunLocation run_location,
    const std::set<std::string>& executing_scripts) const {
  return script_ && script_->run_location() == run_location &&
         !script_->js_scripts().empty() &&
         ShouldInjectScripts(script_->js_scripts(), executing_scripts);
}

bool UserScriptInjector::ShouldInjectCss(
    UserScript::RunLocation run_location,
    const std::set<std::string>& injected_stylesheets) const {
  return script_ && run_location == UserScript::DOCUMENT_START &&
         !script_->css_scripts().empty() &&
         ShouldInjectScripts(script_->css_scripts(), injected_stylesheets);
}

PermissionsData::PageAccess UserScriptInjector::CanExecuteOnFrame(
    const InjectionHost* injection_host,
    blink::WebLocalFrame* web_frame,
    int tab_id) {
  // There is no harm in allowing the injection when the script is gone,
  // because there is nothing to inject.
  if (!script_)
    return PermissionsData::PageAccess::kAllowed;

  if (script_->consumer_instance_type() ==
          UserScript::ConsumerInstanceType::WEBVIEW) {
    int routing_id = content::RenderView::FromWebView(web_frame->Top()->View())
                         ->GetRoutingID();

    RoutingInfoKey key(routing_id, script_->id());

    RoutingInfoMap& map = g_routing_info_map.Get();
    auto iter = map.find(key);

    bool allowed = false;
    if (iter != map.end()) {
      allowed = iter->second;
    } else {
      // Send a SYNC IPC message to the browser to check if this is allowed.
      // This is not ideal, but is mitigated by the fact that this is only done
      // for webviews, and then only once per host.
      // TODO(hanxi): Find a more efficient way to do this.
      content::RenderThread::Get()->Send(
          new ExtensionsGuestViewHostMsg_CanExecuteContentScriptSync(
              routing_id, script_->id(), &allowed));
      map.insert(std::pair<RoutingInfoKey, bool>(key, allowed));
    }

    return allowed ? PermissionsData::PageAccess::kAllowed
                   : PermissionsData::PageAccess::kDenied;
  }

  GURL effective_document_url = ScriptContext::GetEffectiveDocumentURL(
      web_frame, web_frame->GetDocument().Url(), script_->match_about_blank());

  return injection_host->CanExecuteOnFrame(
      effective_document_url,
      content::RenderFrame::FromWebFrame(web_frame),
      tab_id,
      is_declarative_);
}

std::vector<blink::WebScriptSource> UserScriptInjector::GetJsSources(
    UserScript::RunLocation run_location,
    std::set<std::string>* executing_scripts,
    size_t* num_injected_js_scripts) const {
  DCHECK(script_);
  std::vector<blink::WebScriptSource> sources;

  DCHECK_EQ(script_->run_location(), run_location);

  const UserScript::FileList& js_scripts = script_->js_scripts();
  sources.reserve(js_scripts.size() +
                  (script_->emulate_greasemonkey() ? 1 : 0));
  // Emulate Greasemonkey API for scripts that were converted to extension
  // user scripts.
  if (script_->emulate_greasemonkey())
    sources.push_back(g_greasemonkey_api.Get().GetSource());
  for (const std::unique_ptr<UserScript::File>& file : js_scripts) {
    const GURL& script_url = file->url();
    // Check if the script is already injected.
    if (executing_scripts->count(script_url.path()) != 0)
      continue;

    sources.push_back(blink::WebScriptSource(
        user_script_set_->GetJsSource(*file, script_->emulate_greasemonkey()),
        script_url));

    (*num_injected_js_scripts) += 1;
    executing_scripts->insert(script_url.path());
  }

  return sources;
}

std::vector<blink::WebString> UserScriptInjector::GetCssSources(
    UserScript::RunLocation run_location,
    std::set<std::string>* injected_stylesheets,
    size_t* num_injected_stylesheets) const {
  DCHECK(script_);
  DCHECK_EQ(UserScript::DOCUMENT_START, run_location);

  std::vector<blink::WebString> sources;

  const UserScript::FileList& css_scripts = script_->css_scripts();
  sources.reserve(css_scripts.size());
  for (const std::unique_ptr<UserScript::File>& file : script_->css_scripts()) {
    const std::string& stylesheet_path = file->url().path();
    // Check if the stylesheet is already injected.
    if (injected_stylesheets->count(stylesheet_path) != 0)
      continue;

    sources.push_back(user_script_set_->GetCssSource(*file));
    (*num_injected_stylesheets) += 1;
    injected_stylesheets->insert(stylesheet_path);
  }
  return sources;
}

void UserScriptInjector::OnInjectionComplete(
    std::unique_ptr<base::Value> execution_result,
    UserScript::RunLocation run_location,
    content::RenderFrame* render_frame) {}

void UserScriptInjector::OnWillNotInject(InjectFailureReason reason,
                                         content::RenderFrame* render_frame) {
}

}  // namespace extensions
