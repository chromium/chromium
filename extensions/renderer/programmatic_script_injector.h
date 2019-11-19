// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_PROGRAMMATIC_SCRIPT_INJECTOR_H_
#define EXTENSIONS_RENDERER_PROGRAMMATIC_SCRIPT_INJECTOR_H_

#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "extensions/renderer/script_injection.h"
#include "url/gurl.h"

struct ExtensionMsg_ExecuteCode_Params;

namespace content {
class RenderFrame;
}

namespace extensions {

// A ScriptInjector to handle tabs.executeScript().
class ProgrammaticScriptInjector : public ScriptInjector {
 public:
  explicit ProgrammaticScriptInjector(
      const ExtensionMsg_ExecuteCode_Params& params);
  ~ProgrammaticScriptInjector() override;

 private:
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

  // Whether it is safe to include information about the URL in error messages.
  bool CanShowUrlInError() const;

  // Notify the browser that the script was injected (or never will be), and
  // send along any results or errors.
  void Finish(const std::string& error, content::RenderFrame* render_frame);

  // The parameters for injecting the script.
  std::unique_ptr<ExtensionMsg_ExecuteCode_Params> params_;

  // The url of the frame into which we are injecting.
  GURL url_;

  // The serialization of the frame's origin if the frame is an about:-URL. This
  // is used to provide user-friendly messages.
  std::string origin_for_about_error_;

  // The results of the script execution.
  base::ListValue results_;

  // Whether or not this script injection has finished.
  bool finished_;

  DISALLOW_COPY_AND_ASSIGN(ProgrammaticScriptInjector);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_PROGRAMMATIC_SCRIPT_INJECTOR_H_
