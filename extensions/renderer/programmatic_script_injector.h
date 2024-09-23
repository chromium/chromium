// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_PROGRAMMATIC_SCRIPT_INJECTOR_H_
#define EXTENSIONS_RENDERER_PROGRAMMATIC_SCRIPT_INJECTOR_H_

#include <memory>
#include <optional>

#include "base/values.h"
#include "extensions/common/mojom/css_origin.mojom-shared.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/mojom/injection_type.mojom-shared.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/renderer/script_injection.h"
#include "url/gurl.h"

namespace extensions {

// A ScriptInjector to handle tabs.executeScript().
class ProgrammaticScriptInjector : public ScriptInjector {
 public:
  explicit ProgrammaticScriptInjector(
      mojom::ExecuteCodeParamsPtr params,
      mojom::LocalFrame::ExecuteCodeCallback callback);

  ProgrammaticScriptInjector(const ProgrammaticScriptInjector&) = delete;
  ProgrammaticScriptInjector& operator=(const ProgrammaticScriptInjector&) =
      delete;

  ~ProgrammaticScriptInjector() override;

 private:
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

  // Whether it is safe to include information about the URL in error messages.
  bool CanShowUrlInError() const;

  // Notify the browser that the script was injected (or never will be), and
  // send along any results or errors.
  void Finish(const std::string& error);

  // The parameters for injecting the script.
  mojom::ExecuteCodeParamsPtr params_;

  // The callback to notify that the script has been executed.
  mojom::LocalFrame::ExecuteCodeCallback callback_;

  // The url of the frame into which we are injecting.
  GURL url_;

  // The serialization of the frame's origin if the frame is an about:-URL. This
  // is used to provide user-friendly messages.
  std::string origin_for_about_error_;

  // The result of the script execution.
  std::optional<base::Value> result_;

  // Whether or not this script injection has finished.
  bool finished_ = false;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_PROGRAMMATIC_SCRIPT_INJECTOR_H_
