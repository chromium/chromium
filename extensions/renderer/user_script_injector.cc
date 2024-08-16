// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/user_script_injector.h"

#include <tuple>
#include <vector>

#include "base/check.h"
#include "base/lazy_instance.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/grit/extensions_renderer_resources.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/injection_host.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/scripts_run_info.h"
#include "ipc/ipc_sync_channel.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/common/mojom/guest_view.mojom.h"
#endif

namespace extensions {

namespace {

#if BUILDFLAG(ENABLE_GUEST_VIEW)
struct RoutingInfoKey {
  blink::LocalFrameToken frame_token;
  std::string script_id;

  RoutingInfoKey(const blink::LocalFrameToken& frame_token,
                 std::string script_id)
      : frame_token(frame_token), script_id(std::move(script_id)) {}

  bool operator<(const RoutingInfoKey& other) const {
    return std::tie(frame_token, script_id) <
           std::tie(other.frame_token, other.script_id);
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

#endif

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
  std::string greasemonky_api_js(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_GREASEMONKEY_API_JS));
  source_ = blink::WebString::FromUTF8(greasemonky_api_js);
}

blink::WebScriptSource GreasemonkeyApiJsString::GetSource() const {
  return blink::WebScriptSource(source_);
}

base::LazyInstance<GreasemonkeyApiJsString>::Leaky g_greasemonkey_api =
    LAZY_INSTANCE_INITIALIZER;

bool ShouldInjectScripts(const UserScript::ContentList& script_contents,
                         const std::set<std::string>& injected_files) {
  for (const std::unique_ptr<UserScript::Content>& content : script_contents) {
    // Check if the script is already injected.
    if (injected_files.count(content->url().path()) == 0) {
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
      is_declarative_(is_declarative) {
  user_script_set_observation_.Observe(script_list);
}

UserScriptInjector::~UserScriptInjector() {
}

void UserScriptInjector::OnUserScriptsUpdated() {
  // When user scripts are updated, this means the host causing this injection
  // has changed. All old script pointers are invalidated and this injection
  // will be removed as there's no guarantee the backing script still exists.
  script_ = nullptr;
}

void UserScriptInjector::OnUserScriptSetDestroyed() {
  user_script_set_observation_.Reset();
  // Invalidate the script pointer as the UserScriptSet which this script
  // belongs to has been destroyed.
  script_ = nullptr;
}

mojom::InjectionType UserScriptInjector::script_type() const {
  return mojom::InjectionType::kContentScript;
}

blink::mojom::UserActivationOption UserScriptInjector::IsUserGesture() const {
  return blink::mojom::UserActivationOption::kDoNotActivate;
}

mojom::ExecutionWorld UserScriptInjector::GetExecutionWorld() const {
  return script_->execution_world();
}

const std::optional<std::string>& UserScriptInjector::GetExecutionWorldId()
    const {
  return script_->world_id();
}

blink::mojom::WantResultOption UserScriptInjector::ExpectsResults() const {
  return blink::mojom::WantResultOption::kNoResult;
}

blink::mojom::PromiseResultOption UserScriptInjector::ShouldWaitForPromise()
    const {
  return blink::mojom::PromiseResultOption::kDoNotWait;
}

mojom::CSSOrigin UserScriptInjector::GetCssOrigin() const {
  return mojom::CSSOrigin::kAuthor;
}

mojom::CSSInjection::Operation UserScriptInjector::GetCSSInjectionOperation()
    const {
  DCHECK(script_);
  DCHECK(!script_->css_scripts().empty());
  return mojom::CSSInjection::Operation::kAdd;
}

bool UserScriptInjector::ShouldInjectJs(
    mojom::RunLocation run_location,
    const std::set<std::string>& executing_scripts) const {
  return script_ && script_->run_location() == run_location &&
         !script_->js_scripts().empty() &&
         ShouldInjectScripts(script_->js_scripts(), executing_scripts);
}

bool UserScriptInjector::ShouldInjectOrRemoveCss(
    mojom::RunLocation run_location,
    const std::set<std::string>& injected_stylesheets) const {
  return script_ && run_location == mojom::RunLocation::kDocumentStart &&
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

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  if (script_->consumer_instance_type() ==
          UserScript::ConsumerInstanceType::WEBVIEW) {
    auto* render_frame = content::RenderFrame::FromWebFrame(web_frame);
    auto token = web_frame->GetLocalFrameToken();

    RoutingInfoKey key(token, script_->id());

    RoutingInfoMap& map = g_routing_info_map.Get();
    auto iter = map.find(key);

    bool allowed = false;
    if (iter != map.end()) {
      allowed = iter->second;
    } else {
      mojo::AssociatedRemote<mojom::GuestView> remote;
      render_frame->GetRemoteAssociatedInterfaces()->GetInterface(&remote);

      // Perform a sync mojo call to the browser to check if this is allowed.
      // This is not ideal, but is mitigated by the fact that this is only done
      // for webviews, and then only once per host.
      // TODO(hanxi): Find a more efficient way to do this.
      remote->CanExecuteContentScript(script_->id(), &allowed);
      map.insert(std::pair<RoutingInfoKey, bool>(key, allowed));
    }

    return allowed ? PermissionsData::PageAccess::kAllowed
                   : PermissionsData::PageAccess::kDenied;
  }
#endif

  GURL effective_document_url =
      ScriptContext::GetEffectiveDocumentURLForInjection(
          web_frame, web_frame->GetDocument().Url(),
          script_->match_origin_as_fallback());

  return injection_host->CanExecuteOnFrame(
      effective_document_url,
      content::RenderFrame::FromWebFrame(web_frame),
      tab_id,
      is_declarative_);
}

std::vector<blink::WebScriptSource> UserScriptInjector::GetJsSources(
    mojom::RunLocation run_location,
    std::set<std::string>* executing_scripts,
    size_t* num_injected_js_scripts) const {
  DCHECK(script_);
  std::vector<blink::WebScriptSource> sources;

  DCHECK_EQ(script_->run_location(), run_location);

  const UserScript::ContentList& js_scripts = script_->js_scripts();
  sources.reserve(js_scripts.size() +
                  (script_->emulate_greasemonkey() ? 1 : 0));
  // Emulate Greasemonkey API for scripts that were converted to extension
  // user scripts.
  if (script_->emulate_greasemonkey())
    sources.push_back(g_greasemonkey_api.Get().GetSource());
  for (const std::unique_ptr<UserScript::Content>& file : js_scripts) {
    const GURL& script_url = file->url();
    // Check if the script is already injected.
    if (executing_scripts->count(script_url.path()) != 0)
      continue;

    sources.push_back(blink::WebScriptSource(
        user_script_set_->GetJsSource(*file, script_->emulate_greasemonkey()),
        script_url));

    ++*num_injected_js_scripts;
    executing_scripts->insert(script_url.path());
  }

  return sources;
}

std::vector<ScriptInjector::CSSSource> UserScriptInjector::GetCssSources(
    mojom::RunLocation run_location,
    std::set<std::string>* injected_stylesheets,
    size_t* num_injected_stylesheets) const {
  DCHECK(script_);
  DCHECK_EQ(mojom::RunLocation::kDocumentStart, run_location);

  std::vector<CSSSource> sources;

  const UserScript::ContentList& css_scripts = script_->css_scripts();
  sources.reserve(css_scripts.size());
  for (const std::unique_ptr<UserScript::Content>& file :
       script_->css_scripts()) {
    const std::string& stylesheet_path = file->url().path();
    // Check if the stylesheet is already injected.
    if (injected_stylesheets->count(stylesheet_path) != 0)
      continue;

    sources.push_back(CSSSource{user_script_set_->GetCssSource(*file),
                                blink::WebStyleSheetKey()});
    injected_stylesheets->insert(stylesheet_path);
  }
  *num_injected_stylesheets += sources.size();
  return sources;
}

void UserScriptInjector::OnInjectionComplete(
    std::optional<base::Value> execution_result,
    mojom::RunLocation run_location) {}

void UserScriptInjector::OnWillNotInject(InjectFailureReason reason) {}

}  // namespace extensions
