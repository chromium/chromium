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

#include <bitset>
#include <memory>

#include "base/location.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser_delegate.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/https_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "v8/include/v8.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace service_manager {
class InterfaceProvider;
}

namespace blink {

class Agent;
class BrowserInterfaceBrokerProxy;
class ConsoleMessage;
class ContentSecurityPolicy;
class ContentSecurityPolicyDelegate;
class CoreProbeSink;
class DOMTimerCoordinator;
class ErrorEvent;
class EventTarget;
class FrameOrWorkerScheduler;
class KURL;
class LocalDOMWindow;
class OriginTrialContext;
class PublicURLManager;
class ResourceFetcher;
class SecurityContext;
class SecurityOrigin;
class ScriptState;
class TrustedTypePolicyFactory;

enum class TaskType : unsigned char;

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
                                     public Supplementable<ExecutionContext>,
                                     public ConsoleLogger,
                                     public UseCounter,
                                     public FeaturePolicyParserDelegate {
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

  // Returns the content security policy to be used based on the current
  // JavaScript world we are in.
  // Note: As part of crbug.com/896041, existing usages of
  // ContentSecurityPolicy::ShouldBypassMainWorld should eventually be replaced
  // by GetContentSecurityPolicyForWorld. However this is under active
  // development, hence new callers should still use
  // ContentSecurityPolicy::ShouldBypassMainWorld for now.
  virtual ContentSecurityPolicy* GetContentSecurityPolicyForWorld();

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
  virtual const SecurityContext& GetSecurityContext() const = 0;

  // https://tc39.github.io/ecma262/#sec-agent-clusters
  // TODO(dtapuska): Remove this virtual once all execution_contexts
  // always have an agent. Worklets currently override this because
  // they don't have agents.
  virtual const base::UnguessableToken& GetAgentClusterID() const;

  bool IsSameAgentCluster(const base::UnguessableToken&) const;

  virtual bool CanExecuteScripts(ReasonForCallingCanExecuteScripts) {
    return false;
  }

  void DispatchErrorEvent(ErrorEvent*, SanitizeScriptErrors);

  virtual void ExceptionThrown(ErrorEvent*) = 0;

  PublicURLManager& GetPublicURLManager();

  ContentSecurityPolicyDelegate& GetContentSecurityPolicyDelegate();

  virtual void RemoveURLFromMemoryCache(const KURL&);

  void SetLifecycleState(mojom::FrameLifecycleState);
  void NotifyContextDestroyed() override;

  using ConsoleLogger::AddConsoleMessage;

  void AddConsoleMessage(ConsoleMessage* message,
                         bool discard_duplicates = false) {
    AddConsoleMessageImpl(message, discard_duplicates);
  }

  // TODO(haraken): Remove these methods by making the customers inherit from
  // ContextLifecycleObserver. ContextLifecycleObserver is a standard way to
  // observe context suspension/resumption.
  virtual bool TasksNeedPause() { return false; }
  virtual void TasksWerePaused() {}
  virtual void TasksWereUnpaused() {}

  bool IsContextPaused() const;
  bool IsContextDestroyed() const { return is_context_destroyed_; }
  mojom::FrameLifecycleState ContextPauseState() const {
    return lifecycle_state_;
  }

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
  void SetReferrerPolicy(network::mojom::ReferrerPolicy);
  virtual network::mojom::ReferrerPolicy GetReferrerPolicy() const {
    return referrer_policy_;
  }

  virtual CoreProbeSink* GetProbeSink() { return nullptr; }

  virtual service_manager::InterfaceProvider* GetInterfaceProvider() {
    return nullptr;
  }

  virtual BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker() = 0;

  virtual FrameOrWorkerScheduler* GetScheduler() = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      TaskType) = 0;

  v8::Isolate* GetIsolate() const { return isolate_; }
  Agent* GetAgent() const { return agent_; }

  v8::MicrotaskQueue* GetMicrotaskQueue() const;

  OriginTrialContext* GetOriginTrialContext() const {
    return origin_trial_context_;
  }

  virtual TrustedTypePolicyFactory* GetTrustedTypes() const { return nullptr; }
  virtual bool RequireTrustedTypes() const;

  // FeaturePolicyParserDelegate override
  bool FeatureEnabled(OriginTrialFeature) const override;
  void CountFeaturePolicyUsage(mojom::WebFeature feature) override;
  bool FeaturePolicyFeatureObserved(
      mojom::FeaturePolicyFeature feature) override;


 protected:
  ExecutionContext(v8::Isolate* isolate,
                   Agent* agent,
                   OriginTrialContext* origin_trial_context);
  ~ExecutionContext() override;

 private:
  // ConsoleLogger implementation.
  void AddConsoleMessageImpl(mojom::ConsoleMessageSource,
                             mojom::ConsoleMessageLevel,
                             const String& message,
                             bool discard_duplicates) final;

  virtual void AddConsoleMessageImpl(ConsoleMessage*,
                                     bool discard_duplicates) = 0;

  v8::Isolate* const isolate_;

  bool DispatchErrorEventInternal(ErrorEvent*, SanitizeScriptErrors);

  unsigned circular_sequential_id_;

  bool in_dispatch_error_event_;
  HeapVector<Member<ErrorEvent>> pending_exceptions_;

  mojom::FrameLifecycleState lifecycle_state_;
  bool is_context_destroyed_;

  Member<PublicURLManager> public_url_manager_;

  const Member<ContentSecurityPolicyDelegate> csp_delegate_;

  Member<Agent> agent_;

  Member<OriginTrialContext> origin_trial_context_;

  // Counter that keeps track of how many window interaction calls are allowed
  // for this ExecutionContext. Callers are expected to call
  // |allowWindowInteraction()| and |consumeWindowInteraction()| in order to
  // increment and decrement the counter.
  int window_interaction_tokens_;

  network::mojom::ReferrerPolicy referrer_policy_;

  // Tracks which feature policies have already been parsed, so as not to count
  // them multiple times.
  // The size of this vector is 0 until FeaturePolicyFeatureObserved is called.
  Vector<bool> parsed_feature_policies_;

  DISALLOW_COPY_AND_ASSIGN(ExecutionContext);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_EXECUTION_CONTEXT_H_
