/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_EXECUTION_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_EXECUTION_CONTEXT_H_

#include <memory>

#include "base/location.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/access_control_status.h"
#include "third_party/blink/renderer/platform/loader/fetch/https_state.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/weborigin/referrer_policy.h"
#include "v8/include/v8.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace service_manager {
class InterfaceProvider;
}

namespace blink {

class ConsoleMessage;
class ContentSecurityPolicy;
class CoreProbeSink;
class DOMTimerCoordinator;
class ErrorEvent;
class EventTarget;
class FetchClientSettingsObjectSnapshot;
class FrameOrWorkerScheduler;
class InterfaceInvalidator;
class KURL;
class LocalDOMWindow;
class PausableObject;
class PublicURLManager;
class ResourceFetcher;
class SecurityContext;
class SecurityOrigin;
class ScriptState;

enum class TaskType : unsigned;

enum ReasonForCallingCanExecuteScripts {
  kAboutToExecuteScript,
  kNotAboutToExecuteScript
};

enum class SecureContextMode { kInsecureContext, kSecureContext };

// An environment in which script can execute. This class exposes the common
// properties of script execution environments on the web (i.e, common between
// script executing in a document and script executing in a worker), such as:
//
// - a base URL for the resolution of relative URLs
// - a security context that defines the privileges associated with the
//   environment (note, however, that specific isolated script contexts may
//   still enjoy elevated privileges)
// - affordances for the activity (including script and active DOM objects) to
//   be paused or terminated, e.g. because a frame has entered the background or
//   been closed permanently
// - a console logging facility for debugging
//
// Typically, the ExecutionContext is an instance of Document or of
// WorkerOrWorkletGlobalScope.
//
// Note that this is distinct from the notion of a ScriptState or v8::Context,
// which are associated with a single script context (with a single global
// object). For example, there are separate JavaScript globals for "main world"
// script written by a web author and an "isolated world" content script written
// by an extension developer, but these share an ExecutionContext (the document)
// in common.
class CORE_EXPORT ExecutionContext : public ContextLifecycleNotifier,
                                     public Supplementable<ExecutionContext> {
  MERGE_GARBAGE_COLLECTED_MIXINS();

 public:
  void Trace(blink::Visitor*) override;

  static ExecutionContext* From(const ScriptState*);

  // Returns the ExecutionContext of the current realm.
  static ExecutionContext* ForCurrentRealm(
      const v8::FunctionCallbackInfo<v8::Value>&);
  // Returns the ExecutionContext of the relevant realm for the receiver object.
  static ExecutionContext* ForRelevantRealm(
      const v8::FunctionCallbackInfo<v8::Value>&);

  virtual bool IsDocument() const { return false; }
  virtual bool IsWorkerOrWorkletGlobalScope() const { return false; }
  virtual bool IsWorkerGlobalScope() const { return false; }
  virtual bool IsWorkletGlobalScope() const { return false; }
  virtual bool IsMainThreadWorkletGlobalScope() const { return false; }
  virtual bool IsDedicatedWorkerGlobalScope() const { return false; }
  virtual bool IsSharedWorkerGlobalScope() const { return false; }
  virtual bool IsServiceWorkerGlobalScope() const { return false; }
  virtual bool IsAnimationWorkletGlobalScope() const { return false; }
  virtual bool IsAudioWorkletGlobalScope() const { return false; }
  virtual bool IsLayoutWorkletGlobalScope() const { return false; }
  virtual bool IsPaintWorkletGlobalScope() const { return false; }
  virtual bool IsThreadedWorkletGlobalScope() const { return false; }
  virtual bool IsJSExecutionForbidden() const { return false; }

  virtual bool IsContextThread() const { return true; }

  virtual bool ShouldInstallV8Extensions() const { return false; }

  const SecurityOrigin* GetSecurityOrigin();
  SecurityOrigin* GetMutableSecurityOrigin();

  ContentSecurityPolicy* GetContentSecurityPolicy();
  virtual const KURL& Url() const = 0;
  virtual const KURL& BaseURL() const = 0;
  virtual KURL CompleteURL(const String& url) const = 0;
  virtual void DisableEval(const String& error_message) = 0;
  virtual LocalDOMWindow* ExecutingWindow() const { return nullptr; }
  virtual String UserAgent() const = 0;

  virtual HttpsState GetHttpsState() const = 0;

  // Gets the DOMTimerCoordinator which maintains the "active timer
  // list" of tasks created by setTimeout and setInterval. The
  // DOMTimerCoordinator is owned by the ExecutionContext and should
  // not be used after the ExecutionContext is destroyed.
  virtual DOMTimerCoordinator* Timers() = 0;

  virtual ResourceFetcher* Fetcher() const = 0;

  virtual SecurityContext& GetSecurityContext() = 0;

  // https://tc39.github.io/ecma262/#sec-agent-clusters
  virtual const base::UnguessableToken& GetAgentClusterID() const = 0;

  bool IsSameAgentCluster(const base::UnguessableToken&) const;

  virtual bool CanExecuteScripts(ReasonForCallingCanExecuteScripts) {
    return false;
  }

  bool ShouldSanitizeScriptError(const String& source_url, AccessControlStatus);
  void DispatchErrorEvent(ErrorEvent*, AccessControlStatus);

  virtual void AddConsoleMessage(ConsoleMessage*) = 0;
  virtual void ExceptionThrown(ErrorEvent*) = 0;

  PublicURLManager& GetPublicURLManager();

  virtual void RemoveURLFromMemoryCache(const KURL&);

  void PausePausableObjects();
  void UnpausePausableObjects();
  void StopPausableObjects();
  void NotifyContextDestroyed() override;

  void PauseScheduledTasks();
  void UnpauseScheduledTasks();

  // TODO(haraken): Remove these methods by making the customers inherit from
  // PausableObject. PausableObject is a standard way to observe context
  // suspension/resumption.
  virtual bool TasksNeedPause() { return false; }
  virtual void TasksWerePaused() {}
  virtual void TasksWereUnpaused() {}

  bool IsContextPaused() const { return is_context_paused_; }
  bool IsContextDestroyed() const { return is_context_destroyed_; }

  // Called after the construction of an PausableObject to synchronize
  // pause state.
  void PausePausableObjectIfNeeded(PausableObject*);

  // Gets the next id in a circular sequence from 1 to 2^31-1.
  int CircularSequentialID();

  virtual EventTarget* ErrorEventTarget() = 0;

  // Methods related to window interaction. It should be used to manage window
  // focusing and window creation permission for an ExecutionContext.
  void AllowWindowInteraction();
  void ConsumeWindowInteraction();
  bool IsWindowInteractionAllowed() const;

  // Decides whether this context is privileged, as described in
  // https://w3c.github.io/webappsec-secure-contexts/#is-settings-object-contextually-secure.
  virtual bool IsSecureContext(String& error_message) const = 0;
  virtual bool IsSecureContext() const;

  SecureContextMode GetSecureContextMode() const {
    return IsSecureContext() ? SecureContextMode::kSecureContext
                             : SecureContextMode::kInsecureContext;
  }

  // Returns a referrer to be used in the "Determine request's Referrer"
  // algorithm defined in the Referrer Policy spec.
  // https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
  virtual String OutgoingReferrer() const;

  FetchClientSettingsObjectSnapshot* CreateFetchClientSettingsObjectSnapshot();

  // Parses a comma-separated list of referrer policy tokens, and sets
  // the context's referrer policy to the last one that is a valid
  // policy. Logs a message to the console if none of the policy
  // tokens are valid policies.
  //
  // If |supportLegacyKeywords| is true, then the legacy keywords
  // "never", "default", "always", and "origin-when-crossorigin" are
  // parsed as valid policies.
  void ParseAndSetReferrerPolicy(const String& policies,
                                 bool support_legacy_keywords = false);
  void SetReferrerPolicy(ReferrerPolicy);
  virtual ReferrerPolicy GetReferrerPolicy() const { return referrer_policy_; }

  virtual CoreProbeSink* GetProbeSink() { return nullptr; }

  virtual service_manager::InterfaceProvider* GetInterfaceProvider() {
    return nullptr;
  }

  virtual FrameOrWorkerScheduler* GetScheduler() = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      TaskType) = 0;

  InterfaceInvalidator* GetInterfaceInvalidator() { return invalidator_.get(); }

 protected:
  ExecutionContext();
  ~ExecutionContext() override;

 private:
  bool DispatchErrorEventInternal(ErrorEvent*, AccessControlStatus);

  unsigned circular_sequential_id_;

  bool in_dispatch_error_event_;
  HeapVector<Member<ErrorEvent>> pending_exceptions_;

  bool is_context_paused_;
  bool is_context_destroyed_;

  Member<PublicURLManager> public_url_manager_;

  // Counter that keeps track of how many window interaction calls are allowed
  // for this ExecutionContext. Callers are expected to call
  // |allowWindowInteraction()| and |consumeWindowInteraction()| in order to
  // increment and decrement the counter.
  int window_interaction_tokens_;

  ReferrerPolicy referrer_policy_;

  std::unique_ptr<InterfaceInvalidator> invalidator_;

  DISALLOW_COPY_AND_ASSIGN(ExecutionContext);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_EXECUTION_CONTEXT_H_
