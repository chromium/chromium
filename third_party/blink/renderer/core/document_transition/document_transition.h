// Copyright 2020 The Chromium Authors
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
class LayoutObject;
class PseudoElement;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

// An interface class that allows us to store requests in some object that isn't
// tied to the lifetime of the transition.
class DocumentTransitionDirectiveStore {
 public:
  virtual void AddPendingRequest(
      std::unique_ptr<DocumentTransitionRequest>) = 0;
};

class CORE_EXPORT DocumentTransition
    : public ScriptWrappable,
      public ActiveScriptWrappable<DocumentTransition>,
      public ExecutionContextLifecycleObserver,
      public ChromeClient::CommitObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Creating a document transition also starts it.
  DocumentTransition(Document*,
                     ScriptState*,
                     V8DocumentTransitionCallback*,
                     DocumentTransitionDirectiveStore*);

  // IDL implementation. Refer to document_transition.idl for additional
  // comments.
  void skipTransition();
  ScriptPromise finished() const;
  ScriptPromise ready() const;
  ScriptPromise domUpdated() const;

  // GC functionality.
  void Trace(Visitor* visitor) const override;

  // ExecutionContextLifecycleObserver implementation.
  void ContextDestroyed() override;

  // ActiveScriptWrappable functionality.
  bool HasPendingActivity() const override;

  // Returns true if this object needs to create an EffectNode for the shared
  // element transition.
  bool NeedsSharedElementEffectNode(const LayoutObject& object) const;

  // Returns true if this object is painted via pseudo elements. Note that this
  // is different from NeedsSharedElementFromEffectNode() since the root may not
  // be a shared element, but require an effect node.
  bool IsRepresentedViaPseudoElements(const LayoutObject& object) const;

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

  // Dispatched during a lifecycle update after prepaint has finished its work.
  // This is only done if we're in the main lifecycle update that will produce
  // painted output.
  void RunDocumentTransitionStepsDuringMainFrame();

  // This returns true if this transition object needs to gather tags as the
  // next step in the process. This is used to force activatable
  // content-visibility locks.
  bool NeedsUpToDateTags() const;

  // Creates a pseudo element for the given |pseudo_id|.
  PseudoElement* CreatePseudoElement(
      Element* parent,
      PseudoId pseudo_id,
      const AtomicString& document_transition_tag);

  // Returns the UA style sheet for the pseudo element tree generated during a
  // transition.
  String UAStyleSheet() const;

  // CommitObserver overrides.
  void WillCommitCompositorFrame() override;

  // Return non-root transitioning elements.
  VectorOf<Element> GetTransitioningElements() const {
    return style_tracker_ ? style_tracker_->GetTransitioningElements()
                          : VectorOf<Element>{};
  }

  bool IsRootTransitioning() const {
    return style_tracker_ && style_tracker_->IsRootTransitioning();
  }

  // In physical pixels. See comments on equivalent methods in
  // DocumentTransitionStyleTracker for info.
  gfx::Rect GetSnapshotViewportRect() const;
  gfx::Vector2d GetRootSnapshotPaintOffset() const;

  bool IsDone() const { return IsTerminalState(state_); }

 private:
  friend class DocumentTransitionTest;
  friend class AXDocumentTransitionTest;

  // Note the states are possibly overly verbose, and several states can
  // transition in one function call, but it's useful to keep track of what is
  // happening. For the most part, the states transition in order, with the
  // exception of us being able to jump to some terminal states from other
  // states.
  enum class State {
    // Initial state.
    kInitial,

    // Capture states.
    kCaptureTagDiscovery,
    kCaptureRequestPending,
    kCapturing,
    kCaptured,

    // Callback states.
    kDOMCallbackRunning,
    kDOMCallbackFinished,

    // Animate states.
    kAnimateTagDiscovery,
    kAnimateRequestPending,
    kAnimating,

    // Terminal states.
    kFinished,
    kAborted,
    kTimedOut
  };

  // Advance to the new state. This returns true if the state should be
  // processed immediately.
  bool AdvanceTo(State state);

  bool CanAdvanceTo(State state) const;
  static bool StateRunsInDocumentTransitionStepsDuringMainFrame(State state);

  // Returns true if we're in a state that doesn't require explicit flow (i.e.
  // we don't need to post task or schedule a frame). We're waiting for some
  // external notifications, like capture is finished, or callback finished
  // running.
  static bool WaitsForNotification(State state);

  static bool IsTerminalState(State state);

  void ProcessCurrentState();

  // Invoked when DocumentTransitionCallback finishes running.
  class DOMChangeFinishedCallback : public ScriptFunction::Callable {
   public:
    explicit DOMChangeFinishedCallback(
        DocumentTransition* transition,
        ScriptPromiseResolver* dom_updated_resolver,
        bool success);
    ~DOMChangeFinishedCallback() override;

    ScriptValue Call(ScriptState*, ScriptValue) override;
    void Trace(Visitor* visitor) const override;

    void Cancel();

   private:
    WeakMember<DocumentTransition> transition_;
    Member<ScriptPromiseResolver> dom_updated_resolver_;
    const bool success_;
  };

  void NotifyCaptureFinished();

  // Dispatched when the DocumentTransitionCallback has finished executing and
  // start phase of the animation can be initiated.
  void NotifyDOMCallbackFinished(bool success);

  // Used to defer visual updates between transition prepare dispatching and
  // transition start to allow the page to set up the final scene
  // asynchronously.
  void PauseRendering();
  void OnRenderingPausedTimeout();
  void ResumeRendering();

  // Returns true if we invoked the callback and the result was not nothing,
  // and false otherwise. False would be returned if we failed to run the
  // callback (the result was "Nothing"). Note that this returns true if there
  // is no dom callback, since effectively we called "noop".
  bool InvokeDOMChangeCallback();

  Member<Document> document_;

  State state_ = State::kInitial;

  // The document tag identifies the document to which this transition belongs.
  // It's unique among other local documents.
  uint32_t document_tag_ = 0u;

  Member<ScriptState> script_state_;

  Member<V8DocumentTransitionCallback> update_dom_callback_;

  Member<DocumentTransitionStyleTracker> style_tracker_;

  Member<ScriptPromiseResolver> dom_updated_promise_resolver_;
  Member<ScriptPromiseResolver> ready_promise_resolver_;
  Member<ScriptPromiseResolver> finished_promise_resolver_;

  DocumentTransitionDirectiveStore* directive_store_;

  std::unique_ptr<cc::ScopedPauseRendering> rendering_paused_scope_;

  bool in_main_lifecycle_update_ = false;
  bool dom_callback_succeeded_ = false;
  bool first_animating_frame_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_H_
