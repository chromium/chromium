// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_annotation_agent.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/state_transitions.h"

namespace chrome_pdf {

PdfAnnotationAgent::PdfAnnotationAgent(
    Container* container,
    blink::mojom::AnnotationType type,
    blink::mojom::SelectorPtr selector,
    mojo::PendingRemote<blink::mojom::AnnotationAgentHost> host_remote,
    mojo::PendingReceiver<blink::mojom::AnnotationAgent> agent_receiver)
    : container_(container) {
  agent_host_.Bind(std::move(host_remote));
  receiver_.Bind(std::move(agent_receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &PdfAnnotationAgent::RemoveTextFragments, weak_factory_.GetWeakPtr()));

  auto attachment_result = blink::mojom::AttachmentResult::kSelectorNotMatched;
  if (type == blink::mojom::AnnotationType::kGlic &&
      selector->which() == blink::mojom::Selector::Tag::kSerializedSelector) {
    const std::string& serialized = selector->get_serialized_selector();
    if (!serialized.empty() && container_->FindAndHighlightTextFragments(
                                   base::span_from_ref(serialized))) {
      attachment_result = blink::mojom::AttachmentResult::kSuccess;
    }
  }
  agent_host_->DidFinishAttachment(gfx::Rect(), attachment_result);
  SetState(attachment_result == blink::mojom::AttachmentResult::kSuccess
               ? State::kActive
               : State::kFailure);
}

PdfAnnotationAgent::~PdfAnnotationAgent() {
  RemoveTextFragments();
}

void PdfAnnotationAgent::ScrollIntoView(bool applies_focus) {
  // The text fragment results can be invalidated between DidFinishAttachment()
  // and ScrollIntoView(). Do not attempt to scroll if the results are
  // invalidated.
  if (state_ == State::kHighlightDropped) {
    return;
  }
  // ScrollIntoView() is only valid after the construction, shouldn't be called
  // if the text fragment isn't found.
  CHECK_EQ(state_, State::kActive);
  container_->ScrollTextFragmentIntoView();
}

void PdfAnnotationAgent::RemoveTextFragments() {
  if (state_ == State::kFailure || state_ == State::kHighlightDropped) {
    return;
  }
  // Reset() shouldn't be called while in the kInitial state as the search
  // completes synchronously in the constructor.
  CHECK_EQ(state_, State::kActive);
  container_->RemoveTextFragments();
  SetState(State::kHighlightDropped);
}

void PdfAnnotationAgent::SetState(State new_state) {
  static const base::NoDestructor<base::StateTransitions<State>>
      allowed_transitions(base::StateTransitions<State>(
          {{State::kInitial, {State::kActive, State::kFailure}},
           {State::kActive, {State::kHighlightDropped}},
           {State::kHighlightDropped, {}},
           {State::kFailure, {}}}));
  CHECK_STATE_TRANSITION(allowed_transitions, state_, new_state);
  state_ = new_state;
}

}  // namespace chrome_pdf
