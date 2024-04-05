// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_INJECTOR_H_
#define EXTENSIONS_RENDERER_SCRIPT_INJECTOR_H_

#include <memory>
#include <vector>

#include "extensions/common/constants.h"
#include "extensions/common/mojom/code_injection.mojom.h"
#include "extensions/common/mojom/css_origin.mojom-shared.h"
#include "extensions/common/mojom/execution_world.mojom-shared.h"
#include "extensions/common/mojom/injection_type.mojom-shared.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"
#include "third_party/blink/public/web/web_document.h"
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

  struct CSSSource {
    blink::WebString code;
    blink::WebStyleSheetKey key;
  };

  virtual ~ScriptInjector() {}

  // Returns the script type of this particular injection.
  virtual mojom::InjectionType script_type() const = 0;

  // Returns the associated `UserActivationOption` for script evaluation.
  virtual blink::mojom::UserActivationOption IsUserGesture() const = 0;

  // Returns the world in which to execute the javascript code.
  virtual mojom::ExecutionWorld GetExecutionWorld() const = 0;

  // Returns the ID of the world into which to inject.
  virtual const std::optional<std::string>& GetExecutionWorldId() const = 0;

  // Returns the CSS origin of this injection.
  virtual mojom::CSSOrigin GetCssOrigin() const = 0;

  // Returns the type of CSS operation (addition or removal) that should be
  // performed.
  virtual mojom::CSSInjection::Operation GetCSSInjectionOperation() const = 0;

  // Returns the associated `WantResultOption` for script evaluation.
  virtual blink::mojom::WantResultOption ExpectsResults() const = 0;

  // Returns the associated `PromiseResultOption` for script evaluation.
  virtual blink::mojom::PromiseResultOption ShouldWaitForPromise() const = 0;

  // Returns true if the script should inject JS source at the given
  // |run_location|.
  virtual bool ShouldInjectJs(
      mojom::RunLocation run_location,
      const std::set<std::string>& executing_scripts) const = 0;

  // Returns true if the script should inject or remove CSS at the given
  // |run_location|.
  virtual bool ShouldInjectOrRemoveCss(
      mojom::RunLocation run_location,
      const std::set<std::string>& injected_stylesheets) const = 0;

  // Returns true if the script should execute on the given |frame|.
  virtual PermissionsData::PageAccess CanExecuteOnFrame(
      const InjectionHost* injection_host,
      blink::WebLocalFrame* web_frame,
      int tab_id) = 0;

  // Returns the javascript sources to inject at the given |run_location|.
  // Only called if ShouldInjectJs() is true.
  virtual std::vector<blink::WebScriptSource> GetJsSources(
      mojom::RunLocation run_location,
      std::set<std::string>* executing_scripts,
      size_t* num_injected_js_scripts) const = 0;

  // Returns the css to inject at the given |run_location|.
  // Only called if ShouldInjectOrRemoveCss() is true.
  virtual std::vector<CSSSource> GetCssSources(
      mojom::RunLocation run_location,
      std::set<std::string>* injected_stylesheets,
      size_t* num_injected_stylesheets) const = 0;

  // Notifies the script that injection has completed, with a possibly-populated
  // list of results (depending on whether or not ExpectsResults() was true).
  virtual void OnInjectionComplete(std::optional<base::Value> execution_result,
                                   mojom::RunLocation run_location) = 0;

  // Notifies the script that injection will never occur.
  virtual void OnWillNotInject(InjectFailureReason reason) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPT_INJECTOR_H_
