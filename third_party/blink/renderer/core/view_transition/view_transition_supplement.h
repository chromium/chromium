// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_SUPPLEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_SUPPLEMENT_H_

#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_transition_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_transition_options.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_request.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
class DOMViewTransition;

class CORE_EXPORT ViewTransitionSupplement
    : public GarbageCollected<ViewTransitionSupplement>,
      public Supplement<Document>,
      public ViewTransition::Delegate {
 public:
  static const char kSupplementName[];

  // Supplement functionality.
  static ViewTransitionSupplement* From(Document&);
  static ViewTransitionSupplement* FromIfExists(const Document&);

  // Creates and starts a same-document ViewTransition initiated using the
  // script API.
  // With callback:
  static DOMViewTransition* startViewTransition(
      ScriptState*,
      Document&,
      V8ViewTransitionCallback* callback,
      ExceptionState&);
  // With options
  static DOMViewTransition* startViewTransition(ScriptState*,
                                                Document&,
                                                ViewTransitionOptions* options,
                                                ExceptionState&);
  // Without callback or options:
  static DOMViewTransition* startViewTransition(ScriptState*,
                                                Document&,
                                                ExceptionState&);

  // Creates a ViewTransition to cache the state of a Document before a
  // navigation. The cached state is provided to the caller using the
  // |ViewTransitionStateCallback|.
  static void SnapshotDocumentForNavigation(
      Document&,
      ViewTransition::ViewTransitionStateCallback);

  // Creates a ViewTransition using cached state from the previous Document
  // of a navigation. This ViewTransition is responsible for running
  // animations on the new Document using the cached state.
  static void CreateFromSnapshotForNavigation(
      Document&,
      ViewTransitionState transition_state);

  // Abort any ongoing transitions in the document.
  static void AbortTransition(Document&);

  ViewTransition* GetTransition();

  explicit ViewTransitionSupplement(Document&);
  ~ViewTransitionSupplement() override;

  // GC functionality.
  void Trace(Visitor* visitor) const override;

  // ViewTransition::Delegate implementation.
  void AddPendingRequest(std::unique_ptr<ViewTransitionRequest>) override;
  VectorOf<std::unique_ptr<ViewTransitionRequest>> TakePendingRequests();
  void OnTransitionFinished(ViewTransition* transition) override;

  // Notifies when the "view-transition" meta tag associated with this Document
  // has changed.
  void OnMetaTagChanged(const AtomicString& content_value);

  // TODO(https://crbug.com/1422251): Expand this to receive a the full set of
  // @view-transition options.
  void OnViewTransitionsStyleUpdated(bool cross_document_enabled);

  // Notifies that the `body` element has been parsed and will be added to the
  // Document.
  void WillInsertBody();

  // In the new page of a cross-document transition, this resolves the
  // @view-transition rule to use, sets types, and returns the ViewTransition.
  // It is the 'resolve cross-document view-transition` steps in the spec:
  // https://drafts.csswg.org/css-view-transitions-2/#document-resolve-cross-document-view-transition
  DOMViewTransition* ResolveCrossDocumentViewTransition();

 private:
  static DOMViewTransition* StartViewTransitionInternal(
      ScriptState*,
      Document&,
      V8ViewTransitionCallback* callback,
      const absl::optional<Vector<String>>& types,
      ExceptionState&);

  DOMViewTransition* StartTransition(
      Document& document,
      V8ViewTransitionCallback* callback,
      const absl::optional<Vector<String>>& types,
      ExceptionState& exception_state);
  void StartTransition(Document& document,
                       ViewTransition::ViewTransitionStateCallback callback);
  void StartTransition(Document& document,
                       ViewTransitionState transition_state);

  void SetCrossDocumentOptIn(mojom::blink::ViewTransitionSameOriginOptIn);

  Member<ViewTransition> transition_;

  VectorOf<std::unique_ptr<ViewTransitionRequest>> pending_requests_;

  mojom::blink::ViewTransitionSameOriginOptIn cross_document_opt_in_ =
      mojom::blink::ViewTransitionSameOriginOptIn::kDisabled;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_SUPPLEMENT_H_
