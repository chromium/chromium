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

#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/shared_persistent.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/access_control_status.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class DOMWrapperWorld;
class Element;
class ExecutionContext;
class KURL;
class LocalFrame;
class ScriptSourceCode;
class SecurityOrigin;

// This class exposes methods to run script in a frame (in the main world and
// in isolated worlds). An instance can be obtained by using
// LocalFrame::GetScriptController().
class CORE_EXPORT ScriptController final
    : public GarbageCollected<ScriptController> {
  WTF_MAKE_NONCOPYABLE(ScriptController);

 public:
  enum ExecuteScriptPolicy {
    kExecuteScriptWhenScriptsDisabled,
    kDoNotExecuteScriptWhenScriptsDisabled
  };

  static ScriptController* Create(
      LocalFrame& frame,
      LocalWindowProxyManager& window_proxy_manager) {
    return new ScriptController(frame, window_proxy_manager);
  }

  void Trace(blink::Visitor*);

  // This returns an initialized window proxy. (If the window proxy is not
  // yet initialized, it's implicitly initialized at the first access.)
  LocalWindowProxy* WindowProxy(DOMWrapperWorld& world) {
    return window_proxy_manager_->WindowProxy(world);
  }

  // Evaluate JavaScript in the main world.
  void ExecuteScriptInMainWorld(
      const String& script,
      ScriptSourceLocationType = ScriptSourceLocationType::kUnknown,
      ExecuteScriptPolicy = kDoNotExecuteScriptWhenScriptsDisabled);
  void ExecuteScriptInMainWorld(
      const ScriptSourceCode&,
      const KURL& base_url,
      AccessControlStatus,
      const ScriptFetchOptions& = ScriptFetchOptions());
  v8::Local<v8::Value> ExecuteScriptInMainWorldAndReturnValue(
      const ScriptSourceCode&,
      const KURL& base_url,
      AccessControlStatus,
      const ScriptFetchOptions& = ScriptFetchOptions(),
      ExecuteScriptPolicy = kDoNotExecuteScriptWhenScriptsDisabled);
  v8::Local<v8::Value> ExecuteScriptAndReturnValue(
      v8::Local<v8::Context>,
      const ScriptSourceCode&,
      const KURL& base_url,
      AccessControlStatus,
      const ScriptFetchOptions& = ScriptFetchOptions());

  // Executes JavaScript in an isolated world. The script gets its own global
  // scope, its own prototypes for intrinsic JavaScript objects (String, Array,
  // and so-on), and its own wrappers for all DOM nodes and DOM constructors.
  //
  // If an isolated world with the specified ID already exists, it is reused.
  // Otherwise, a new world is created.
  v8::Local<v8::Value> ExecuteScriptInIsolatedWorld(
      int world_id,
      const ScriptSourceCode&,
      const KURL& base_url,
      AccessControlStatus access_control_status);

  // Returns true if argument is a JavaScript URL.
  bool ExecuteScriptIfJavaScriptURL(const KURL&, Element*);

  // Creates a new isolated world for DevTools with the given human readable
  // |world_name| and returns it id or nullptr on failure.
  scoped_refptr<DOMWrapperWorld> CreateNewInspectorIsolatedWorld(
      const String& world_name);

  // Returns true if the current world is isolated, and has its own Content
  // Security Policy. In this case, the policy of the main world should be
  // ignored when evaluating resources injected into the DOM.
  bool ShouldBypassMainWorldCSP();

  void DisableEval(const String& error_message);

  TextPosition EventHandlerPosition() const;

  void ClearWindowProxy();
  void UpdateDocument();

  void UpdateSecurityOrigin(const SecurityOrigin*);

  void ClearForClose();

  // Registers a v8 extension to be available on webpages. Will only
  // affect v8 contexts initialized after this call. Takes ownership of
  // the v8::Extension object passed.
  static void RegisterExtensionIfNeeded(v8::Extension*);
  static v8::ExtensionConfiguration ExtensionsFor(const ExecutionContext*);

 private:
  ScriptController(LocalFrame& frame,
                   LocalWindowProxyManager& window_proxy_manager)
      : frame_(&frame), window_proxy_manager_(&window_proxy_manager) {}

  LocalFrame* GetFrame() const { return frame_; }
  v8::Isolate* GetIsolate() const {
    return window_proxy_manager_->GetIsolate();
  }
  void EnableEval();

  v8::Local<v8::Value> EvaluateScriptInMainWorld(const ScriptSourceCode&,
                                                 const KURL& base_url,
                                                 AccessControlStatus,
                                                 const ScriptFetchOptions&,
                                                 ExecuteScriptPolicy);

  const Member<LocalFrame> frame_;
  const Member<LocalWindowProxyManager> window_proxy_manager_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_CONTROLLER_H_
