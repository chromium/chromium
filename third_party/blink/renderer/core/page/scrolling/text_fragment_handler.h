// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_HANDLER_H_

#include "third_party/blink/public/mojom/link_to_text/link_to_text.mojom-blink.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector_generator.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

class LocalFrame;
class TextFragmentAnchor;

// TextFragmentHandler is responsible for handling requests from the
// browser-side link-to-text/shared-highlighting feature. It is responsible for
// generating a text fragment URL based on the current selection as well as
// collecting information about and modifying text fragments on the current
// page. This class is registered on and owned by the frame that interacts with
// the link-to-text/shared-highlighting feature.
class CORE_EXPORT TextFragmentHandler final
    : public GarbageCollected<TextFragmentHandler>,
      public blink::mojom::blink::TextFragmentReceiver {
 public:
  explicit TextFragmentHandler(LocalFrame* main_frame);
  TextFragmentHandler(const TextFragmentHandler&) = delete;
  TextFragmentHandler& operator=(const TextFragmentHandler&) = delete;

  // Determine if |result| represents a click on an existing highlight.
  static bool IsOverTextFragment(HitTestResult result);

  // mojom::blink::TextFragmentReceiver interface
  void Cancel() override;
  void RequestSelector(RequestSelectorCallback callback) override;
  void GetExistingSelectors(GetExistingSelectorsCallback callback) override;
  void RemoveFragments() override;
  void ExtractTextFragmentsMatches(
      ExtractTextFragmentsMatchesCallback callback) override;
  void ExtractFirstFragmentRect(
      ExtractFirstFragmentRectCallback callback) override;

  // This starts the preemptive generation on the current selection if it is not
  // empty.
  void StartPreemptiveGenerationIfNeeded();

  void BindTextFragmentReceiver(
      mojo::PendingReceiver<mojom::blink::TextFragmentReceiver> producer);

  void Trace(Visitor*) const;

  TextFragmentSelectorGenerator* GetTextFragmentSelectorGenerator() {
    return text_fragment_selector_generator_;
  }

  void DidDetachDocumentOrFrame();

 private:
  // The callback passed to TextFragmentSelectorGenerator that will receive the
  // result.
  void DidFinishSelectorGeneration(const TextFragmentSelector& selector);

  // This starts running the generator over the current selection.
  // The result will be returned by invoking DidFinishSelectorGeneration().
  void StartGeneratingForCurrentSelection();

  void RecordPreemptiveGenerationMetrics(const TextFragmentSelector& selector);

  // Called to reply to the client's RequestSelector call with the result.
  void InvokeReplyCallback(const TextFragmentSelector& selector);

  TextFragmentAnchor* GetTextFragmentAnchor();

  LocalFrame* GetFrame() {
    return GetTextFragmentSelectorGenerator()->GetFrame();
  }

  // Class responsible for generating text fragment selectors for the current
  // selection.
  Member<TextFragmentSelectorGenerator> text_fragment_selector_generator_;

  // The result of preemptively generating on selection changes will be stored
  // in this member when completed. Used only in preemptive link generation
  // mode.
  absl::optional<TextFragmentSelector> preemptive_generation_result_;

  // Reports whether |RequestSelector| was called before or after selector was
  // ready. Used only in preemptive link generation mode.
  absl::optional<bool> selector_requested_before_ready_;

  // This will hold the reply callback to the RequestSelector mojo call. This
  // will be invoked in InvokeReplyCallback to send the reply back to the
  // browser.
  RequestSelectorCallback response_callback_;

  // Used for communication between |TextFragmentHandler| in renderer
  // and |TextFragmentSelectorClientImpl| in browser.
  HeapMojoReceiver<blink::mojom::blink::TextFragmentReceiver,
                   TextFragmentHandler>
      selector_producer_{this, nullptr};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_HANDLER_H_
