// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_USER_SCRIPT_INJECTOR_H_
#define EXTENSIONS_RENDERER_USER_SCRIPT_INJECTOR_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "extensions/common/mojom/css_origin.mojom-shared.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/injection_type.mojom-shared.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
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

  UserScriptInjector(const UserScriptInjector&) = delete;
  UserScriptInjector& operator=(const UserScriptInjector&) = delete;

  ~UserScriptInjector() override;

 private:
  // UserScriptSet::Observer implementation.
  void OnUserScriptsUpdated() override;
  void OnUserScriptSetDestroyed() override;

  // ScriptInjector implementation.
  mojom::InjectionType script_type() const override;
  blink::mojom::UserActivationOption IsUserGesture() const override;
  mojom::ExecutionWorld GetExecutionWorld() const override;
  const std::optional<std::string>& GetExecutionWorldId() const override;
  mojom::CSSOrigin GetCssOrigin() const override;
  mojom::CSSInjection::Operation GetCSSInjectionOperation() const override;
  blink::mojom::WantResultOption ExpectsResults() const override;
  blink::mojom::PromiseResultOption ShouldWaitForPromise() const override;
  bool ShouldInjectJs(
      mojom::RunLocation run_location,
      const std::set<std::string>& executing_scripts) const override;
  bool ShouldInjectOrRemoveCss(
      mojom::RunLocation run_location,
      const std::set<std::string>& injected_stylesheets) const override;
  PermissionsData::PageAccess CanExecuteOnFrame(
      const InjectionHost* injection_host,
      blink::WebLocalFrame* web_frame,
      int tab_id) override;
  std::vector<blink::WebScriptSource> GetJsSources(
      mojom::RunLocation run_location,
      std::set<std::string>* executing_scripts,
      size_t* num_injected_js_scripts) const override;
  std::vector<CSSSource> GetCssSources(
      mojom::RunLocation run_location,
      std::set<std::string>* injected_stylesheets,
      size_t* num_injected_stylesheets) const override;
  void OnInjectionComplete(std::optional<base::Value> execution_result,
                           mojom::RunLocation run_location) override;
  void OnWillNotInject(InjectFailureReason reason) override;

  // The associated user script. Owned by the UserScriptSet that created this
  // object.
  raw_ptr<const UserScript, DanglingUntriaged> script_;

  // The UserScriptSet that eventually owns the UserScript this
  // UserScriptInjector points to. Outlives `this` unless the UserScriptSet may
  // be destroyed first, and `this` will be destroyed immediately after.
  const raw_ptr<UserScriptSet, DanglingUntriaged> user_script_set_;

  // The id of the associated user script. We cache this because when we update
  // the |script_| associated with this injection, the old reference may be
  // deleted.
  std::string script_id_;

  // The associated host id, preserved for the same reason as |script_id|.
  mojom::HostID host_id_;

  // Indicates whether or not this script is declarative. This influences which
  // script permissions are checked before injection.
  bool is_declarative_;

  base::ScopedObservation<UserScriptSet, UserScriptSet::Observer>
      user_script_set_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_USER_SCRIPT_INJECTOR_H_
