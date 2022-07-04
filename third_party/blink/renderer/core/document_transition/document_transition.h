// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_callback.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_request.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_style_tracker.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/document_transition_shared_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class Document;
class Element;
class ExceptionState;
class LayoutObject;
class PseudoElement;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

class CORE_EXPORT DocumentTransition
    : public ScriptWrappable,
      public ActiveScriptWrappable<DocumentTransition>,
      public ExecutionContextLifecycleObserver,
      public LocalFrameView::LifecycleNotificationObserver,
      public ChromeClient::DeferredCommitObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit DocumentTransition(Document*);

  // GC functionality.
  void Trace(Visitor* visitor) const override;

  // ExecutionContextLifecycleObserver implementation.
  void ContextDestroyed() override;

  // ActiveScriptWrappable functionality.
  bool HasPendingActivity() const override;

  // Initiates a new transition via createDocumentTransition() API. Returns
  // false if a transition was already active which must finish before a new one
  // can be started.
  bool StartNewTransition();

  ScriptPromise start(ScriptState*, ExceptionState&);
  ScriptPromise start(ScriptState*,
                      V8DocumentTransitionCallback* callback,
                      ExceptionState&);
  void abandon(ScriptState*, ExceptionState&);

  // This uses std::move semantics to take the request from this object.
  std::unique_ptr<DocumentTransitionRequest> TakePendingRequest();

  // Returns true if this object needs to create an EffectNode for the shared
  // element transition.
  bool NeedsSharedElementEffectNode(const LayoutObject& object) const;

  // Updates an effect node. This effect populates the shared element id and the
  // shared element resource id. The return value is a result of updating the
  // effect node.
  PaintPropertyChangeType UpdateEffect(
      const LayoutObject& object,
      const EffectPaintPropertyNodeOrAlias& current_effect,
      const ClipPaintPropertyNodeOrAlias* current_clip,
      const TransformPaintPropertyNodeOrAlias* current_transform);

  // Returns the effect. One needs to first call UpdateEffect().
  EffectPaintPropertyNode* GetEffect(const LayoutObject& object) const;

  // We require shared elements to be contained. This check verifies that and
  // removes it from the shared list if it isn't. See
  // https://github.com/vmpstr/shared-element-transitions/issues/17
  void VerifySharedElements();

  // Dispatched during a lifecycle update after clean layout.
  void RunPostPrePaintSteps();

  // Creates a pseudo element for the given |pseudo_id|.
  PseudoElement* CreatePseudoElement(
      Element* parent,
      PseudoId pseudo_id,
      const AtomicString& document_transition_tag);

  // Returns the UA style sheet for the pseudo element tree generated during a
  // transition.
  const String& UAStyleSheet() const;

  // Used by web tests to retain the pseudo-element tree after a
  // DocumentTransition finishes. This is used to capture a static version of
  // the last rendered frame.
  void DisableEndTransition() { disable_end_transition_ = true; }

  // LifecycleNotificationObserver overrides.
  void WillStartLifecycleUpdate(const LocalFrameView&) override;

 private:
  friend class DocumentTransitionTest;

  enum class State { kIdle, kCapturing, kCaptured, kStarted };

  // Invoked when the async callback dispatched after capture finishes
  // executing.
  class PostCaptureResolved : public ScriptFunction::Callable {
   public:
    explicit PostCaptureResolved(DocumentTransition* transition, bool success);
    ~PostCaptureResolved() override;

    ScriptValue Call(ScriptState*, ScriptValue) override;
    void Trace(Visitor* visitor) const override;

    void Cancel();

   private:
    Member<DocumentTransition> transition_;
    const bool success_;
  };

  void NotifyHasChangesToCommit();

  void NotifyCaptureFinished(uint32_t sequence_id);
  void NotifyStartFinished(uint32_t sequence_id);

  // Dispatched when the DocumentTransitionCallback has finished executing and
  // start phase of the animation can be initiated.
  void NotifyPostCaptureCallbackResolved(bool success);

  // Used to defer visual updates between transition prepare finishing and
  // transition start to allow the page to set up the final scene
  // asynchronously.
  void StartDeferringCommits();
  void StopDeferringCommits();
  void WillStopDeferringCommits(cc::PaintHoldingCommitTrigger) final;

  // Allow canceling a transition until it reaches start().
  void CancelPendingTransition(const char* abort_message);

  // Resets internal state, called in both abort situations and transition
  // finished situations.
  void ResetTransitionState(bool abort_style_tracker = true);
  void ResetScriptState(const char* abort_message);

  Member<Document> document_;

  State state_ = State::kIdle;

  // Script callback passed to the start() API. Dispatched when capturing
  // snapshots from the old DOM finishes.
  Member<V8DocumentTransitionCallback> capture_resolved_callback_;

  // Script state cached from the start() API.
  Member<ScriptState> start_script_state_;

  // The following callables are used to track when the async
  // |capture_resolved_callback_| finishes executing.
  Member<PostCaptureResolved> post_capture_success_callable_;
  Member<PostCaptureResolved> post_capture_reject_callable_;

  // The following promise is provided to script and resolved when all
  // animations from the start phase finish.
  Member<ScriptPromiseResolver> start_promise_resolver_;

  // Created conditionally if renderer based SharedElementTransitions is
  // enabled.
  Member<DocumentTransitionStyleTracker> style_tracker_;

  std::unique_ptr<DocumentTransitionRequest> pending_request_;

  uint32_t last_prepare_sequence_id_ = 0u;
  uint32_t last_start_sequence_id_ = 0u;

  // Use a common counter for sequence ids to avoid confusion. Prepare
  // and start sequence ids are technically in a different namespace, but this
  // avoids any confusion while debugging.
  uint32_t next_sequence_id_ = 1u;

  // The document tag identifies the document to which this transition belongs.
  // It's unique among other local documents.
  uint32_t document_tag_ = 0u;

  bool deferring_commits_ = false;

  // Set only for tests.
  bool disable_end_transition_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_H_
