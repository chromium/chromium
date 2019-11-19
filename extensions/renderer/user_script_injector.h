// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_USER_SCRIPT_INJECTOR_H_
#define EXTENSIONS_RENDERER_USER_SCRIPT_INJECTOR_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "extensions/common/user_script.h"
#include "extensions/renderer/script_injection.h"
#include "extensions/renderer/user_script_set.h"

class InjectionHost;

namespace blink {
class WebLocalFrame;
}

namespace extensions {

// A ScriptInjector for UserScripts.
class UserScriptInjector : public ScriptInjector,
                           public UserScriptSet::Observer {
 public:
  UserScriptInjector(const UserScript* user_script,
                     UserScriptSet* user_script_set,
                     bool is_declarative);
  ~UserScriptInjector() override;

 private:
  // UserScriptSet::Observer implementation.
  void OnUserScriptsUpdated(const std::set<HostID>& changed_hosts,
                            const UserScriptList& scripts) override;

  // ScriptInjector implementation.
  UserScript::InjectionType script_type() const override;
  bool IsUserGesture() const override;
  base::Optional<CSSOrigin> GetCssOrigin() const override;
  const base::Optional<std::string> GetInjectionKey() const override;
  bool ExpectsResults() const override;
  bool ShouldInjectJs(
      UserScript::RunLocation run_location,
      const std::set<std::string>& executing_scripts) const override;
  bool ShouldInjectCss(
      UserScript::RunLocation run_location,
      const std::set<std::string>& injected_stylesheets) const override;
  PermissionsData::PageAccess CanExecuteOnFrame(
      const InjectionHost* injection_host,
      blink::WebLocalFrame* web_frame,
      int tab_id) override;
  std::vector<blink::WebScriptSource> GetJsSources(
      UserScript::RunLocation run_location,
      std::set<std::string>* executing_scripts,
      size_t* num_injected_js_scripts) const override;
  std::vector<blink::WebString> GetCssSources(
      UserScript::RunLocation run_location,
      std::set<std::string>* injected_stylesheets,
      size_t* num_injected_stylesheets) const override;
  void OnInjectionComplete(std::unique_ptr<base::Value> execution_result,
                           UserScript::RunLocation run_location,
                           content::RenderFrame* render_frame) override;
  void OnWillNotInject(InjectFailureReason reason,
                       content::RenderFrame* render_frame) override;

  // The associated user script. Owned by the UserScriptInjector that created
  // this object.
  const UserScript* script_;

  // The UserScriptSet that eventually owns the UserScript this
  // UserScriptInjector points to.
  // Outlives |this|.
  UserScriptSet* const user_script_set_;

  // The id of the associated user script. We cache this because when we update
  // the |script_| associated with this injection, the old referance may be
  // deleted.
  int script_id_;

  // The associated host id, preserved for the same reason as |script_id|.
  HostID host_id_;

  // Indicates whether or not this script is declarative. This influences which
  // script permissions are checked before injection.
  bool is_declarative_;

  ScopedObserver<UserScriptSet, UserScriptSet::Observer>
      user_script_set_observer_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptInjector);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_USER_SCRIPT_INJECTOR_H_
