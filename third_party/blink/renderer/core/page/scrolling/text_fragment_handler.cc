// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_handler.h"

#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

TextFragmentHandler::TextFragmentHandler(LocalFrame* main_frame)
    : text_fragment_selector_generator_(
          MakeGarbageCollected<TextFragmentSelectorGenerator>(main_frame)) {}

void TextFragmentHandler::BindTextFragmentReceiver(
    mojo::PendingReceiver<mojom::blink::TextFragmentReceiver> producer) {
  selector_producer_.reset();
  selector_producer_.Bind(
      std::move(producer),
      text_fragment_selector_generator_->GetFrame()->GetTaskRunner(
          blink::TaskType::kInternalDefault));
}

TextFragmentSelectorGenerator*
TextFragmentHandler::GetTextFragmentSelectorGenerator() {
  return text_fragment_selector_generator_;
}

void TextFragmentHandler::Cancel() {
  GetTextFragmentSelectorGenerator()->Cancel();
}

void TextFragmentHandler::RequestSelector(RequestSelectorCallback callback) {
  GetTextFragmentSelectorGenerator()->RequestSelector(std::move(callback));
}

void TextFragmentHandler::RemoveFragments() {
  DCHECK(
      base::FeatureList::IsEnabled(shared_highlighting::kSharedHighlightingV2));

  GetTextFragmentSelectorGenerator()
      ->GetFrame()
      ->View()
      ->DismissFragmentAnchor();
}

void TextFragmentHandler::Trace(Visitor* visitor) const {
  visitor->Trace(text_fragment_selector_generator_);
  visitor->Trace(selector_producer_);
}

}  // namespace blink
