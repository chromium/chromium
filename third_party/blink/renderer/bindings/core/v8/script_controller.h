/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class DOMWrapperWorld;
class ExecutionContext;
class KURL;
class LocalDOMWindow;
class ScriptSourceCode;
class SecurityOrigin;

// This class exposes methods to run script in a frame (in the main world and
// in isolated worlds). An instance can be obtained by using
// LocalDOMWindow::GetScriptController().
class CORE_EXPORT ScriptController final
    : public GarbageCollected<ScriptController> {
 public:
  enum ExecuteScriptPolicy {
    kExecuteScriptWhenScriptsDisabled,
    kDoNotExecuteScriptWhenScriptsDisabled
  };

  ScriptController(LocalDOMWindow& window,
                   LocalWindowProxyManager& window_proxy_manager)
      : window_(&window), window_proxy_manager_(&window_proxy_manager) {}
  void Trace(Visitor*) const;

  // This returns an initialized window proxy. (If the window proxy is not
  // yet initialized, it's implicitly initialized at the first access.)
  LocalWindowProxy* WindowProxy(DOMWrapperWorld& world) {
    return window_proxy_manager_->WindowProxy(world);
  }

  v8::Local<v8::Value> ExecuteScriptAndReturnValue(
      v8::Local<v8::Context>,
      const ScriptSourceCode&,
      const KURL& base_url,
      SanitizeScriptErrors,
      const ScriptFetchOptions& = ScriptFetchOptions());

  // Evaluate JavaScript in the main world.
  v8::Local<v8::Value> EvaluateScriptInMainWorld(const ScriptSourceCode&,
                                                 const KURL& base_url,
                                                 SanitizeScriptErrors,
                                                 const ScriptFetchOptions&,
                                                 ExecuteScriptPolicy);

  // Executes JavaScript in an isolated world. The script gets its own global
  // scope, its own prototypes for intrinsic JavaScript objects (String, Array,
  // and so-on), and its own wrappers for all DOM nodes and DOM constructors.
  //
  // If an isolated world with the specified ID already exists, it is reused.
  // Otherwise, a new world is created.
  v8::Local<v8::Value> ExecuteScriptInIsolatedWorld(
      int32_t world_id,
      const ScriptSourceCode&,
      const KURL& base_url,
      SanitizeScriptErrors sanitize_script_errors);

  // Executes a javascript url in the main world. |world_for_csp| denotes the
  // javascript world in which this navigation initiated and which should be
  // used for CSP checks.
  void ExecuteJavaScriptURL(const KURL&,
                            network::mojom::CSPDisposition,
                            const DOMWrapperWorld* world_for_csp);

  // Creates a new isolated world for DevTools with the given human readable
  // |world_name| and returns it id or nullptr on failure.
  scoped_refptr<DOMWrapperWorld> CreateNewInspectorIsolatedWorld(
      const String& world_name);

  // Disables eval for the main world.
  void DisableEval(const String& error_message);

  // Disables eval for the given isolated |world_id|. This initializes the
  // window proxy for the isolated world, if it's not yet initialized.
  void DisableEvalForIsolatedWorld(int32_t world_id,
                                   const String& error_message);

  TextPosition EventHandlerPosition() const;

  void UpdateDocument();
  void UpdateSecurityOrigin(const SecurityOrigin*);

  // Registers a v8 extension to be available on webpages. Will only
  // affect v8 contexts initialized after this call.
  static void RegisterExtensionIfNeeded(std::unique_ptr<v8::Extension>);
  static v8::ExtensionConfiguration ExtensionsFor(const ExecutionContext*);

 private:
  v8::Isolate* GetIsolate() const {
    return window_proxy_manager_->GetIsolate();
  }
  void EnableEval();

  // Sets whether eval is enabled for the context corresponding to the given
  // |world|. |error_message| is used only when |allow_eval| is false.
  void SetEvalForWorld(DOMWrapperWorld& world,
                       bool allow_eval,
                       const String& error_message);

  const Member<LocalDOMWindow> window_;
  const Member<LocalWindowProxyManager> window_proxy_manager_;

  DISALLOW_COPY_AND_ASSIGN(ScriptController);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_CONTROLLER_H_
