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

// TextFragmentHandler is responsible for handling text fragment operations
// on a LocalFrame. Generating text fragment selectors for a selection is
// delegated to TextFragmentSelectorGenerator.
class CORE_EXPORT TextFragmentHandler final
    : public GarbageCollected<TextFragmentHandler>,
      public blink::mojom::blink::TextFragmentReceiver {
 public:
  explicit TextFragmentHandler(LocalFrame* main_frame);

  void BindTextFragmentReceiver(
      mojo::PendingReceiver<mojom::blink::TextFragmentReceiver> producer);

  // Cancel any pending selector requests.
  void Cancel() override;

  // Requests selector for current selection.
  void RequestSelector(RequestSelectorCallback callback) override;

  // Remove all text fragments from the current frame.
  void RemoveFragments() override;

  // Determine if |result| represents a click on an existing highlight.
  static bool IsOverTextFragment(HitTestResult result);

  void Trace(Visitor*) const;

  TextFragmentSelectorGenerator* GetTextFragmentSelectorGenerator();

 private:
  // Class responsible for generating text fragment selectors for the current
  // selection.
  Member<TextFragmentSelectorGenerator> text_fragment_selector_generator_;

  // Used for communication between |TextFragmentHandler| in renderer
  // and |TextFragmentSelectorClientImpl| in browser.
  HeapMojoReceiver<blink::mojom::blink::TextFragmentReceiver,
                   TextFragmentHandler>
      selector_producer_{this, nullptr};

  DISALLOW_COPY_AND_ASSIGN(TextFragmentHandler);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_HANDLER_H_
