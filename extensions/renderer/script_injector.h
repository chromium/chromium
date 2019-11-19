// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_INJECTOR_H_
#define EXTENSIONS_RENDERER_SCRIPT_INJECTOR_H_

#include <memory>
#include <vector>

#include "extensions/common/constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"
#include "third_party/blink/public/web/web_script_source.h"

class InjectionHost;

namespace blink {
class WebLocalFrame;
}

namespace extensions {

// The pseudo-delegate class for a ScriptInjection that provides all necessary
// information about how to inject the script, including what code to inject and
// when (run location), but without any injection logic.
class ScriptInjector {
 public:
  // The possible reasons for not injecting the script.
  enum InjectFailureReason {
    EXTENSION_REMOVED,  // The extension was removed before injection.
    NOT_ALLOWED,        // The script is not allowed to inject.
    WONT_INJECT         // The injection won't inject because the user rejected
                        // (or just did not accept) the injection.
  };

  virtual ~ScriptInjector() {}

  // Returns the script type of this particular injection.
  virtual UserScript::InjectionType script_type() const = 0;

  // Returns true if the script is running inside a user gesture.
  virtual bool IsUserGesture() const = 0;

  // Returns the CSS origin of this injection.
  virtual base::Optional<CSSOrigin> GetCssOrigin() const = 0;

  // Returns the key for this injection, if it's a CSS injection.
  virtual const base::Optional<std::string> GetInjectionKey() const = 0;

  // Returns true if the script expects results.
  virtual bool ExpectsResults() const = 0;

  // Returns true if the script should inject JS source at the given
  // |run_location|.
  virtual bool ShouldInjectJs(
      UserScript::RunLocation run_location,
      const std::set<std::string>& executing_scripts) const = 0;

  // Returns true if the script should inject CSS at the given |run_location|.
  virtual bool ShouldInjectCss(
      UserScript::RunLocation run_location,
      const std::set<std::string>& injected_stylesheets) const = 0;

  // Returns true if the script should execute on the given |frame|.
  virtual PermissionsData::PageAccess CanExecuteOnFrame(
      const InjectionHost* injection_host,
      blink::WebLocalFrame* web_frame,
      int tab_id) = 0;

  // Returns the javascript sources to inject at the given |run_location|.
  // Only called if ShouldInjectJs() is true.
  virtual std::vector<blink::WebScriptSource> GetJsSources(
      UserScript::RunLocation run_location,
      std::set<std::string>* executing_scripts,
      size_t* num_injected_js_scripts) const = 0;

  // Returns the css to inject at the given |run_location|.
  // Only called if ShouldInjectCss() is true.
  virtual std::vector<blink::WebString> GetCssSources(
      UserScript::RunLocation run_location,
      std::set<std::string>* injected_stylesheets,
      size_t* num_injected_stylesheets) const = 0;

  // Notifies the script that injection has completed, with a possibly-populated
  // list of results (depending on whether or not ExpectsResults() was true).
  // |render_frame| contains the render frame, or null if the frame was
  // invalidated.
  virtual void OnInjectionComplete(
      std::unique_ptr<base::Value> execution_result,
      UserScript::RunLocation run_location,
      content::RenderFrame* render_frame) = 0;

  // Notifies the script that injection will never occur.
  // |render_frame| contains the render frame, or null if the frame was
  // invalidated.
  virtual void OnWillNotInject(InjectFailureReason reason,
                               content::RenderFrame* render_frame) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPT_INJECTOR_H_
