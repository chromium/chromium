// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/inner_text_agent.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/content_extraction/inner_text_builder.h"

namespace blink {

// static
const char InnerTextAgent::kSupplementName[] = "InnerTextAgent";

// static
InnerTextAgent* InnerTextAgent::From(Document& document) {
  return Supplement<Document>::From<InnerTextAgent>(document);
}

// static
void InnerTextAgent::BindReceiver(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::InnerTextAgent> receiver) {
  DCHECK(frame && frame->GetDocument());
  auto& document = *frame->GetDocument();
  auto* agent = InnerTextAgent::From(document);
  if (!agent) {
    agent = MakeGarbageCollected<InnerTextAgent>(
        base::PassKey<InnerTextAgent>(), *frame);
    Supplement<Document>::ProvideTo(document, agent);
  }
  agent->Bind(std::move(receiver));
}

InnerTextAgent::InnerTextAgent(base::PassKey<InnerTextAgent>, LocalFrame& frame)
    : Supplement<Document>(*frame.GetDocument()),
      receiver_set_(this, frame.DomWindow()) {}

InnerTextAgent::~InnerTextAgent() = default;

void InnerTextAgent::Bind(
    mojo::PendingReceiver<mojom::blink::InnerTextAgent> receiver) {
  // Use `kInternalUserAction` as this task generally results in generating
  // a response to the user.
  receiver_set_.Add(
      std::move(receiver),
      GetSupplementable()->GetTaskRunner(TaskType::kInternalUserInteraction));
}

void InnerTextAgent::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_set_);
  Supplement<Document>::Trace(visitor);
}

void InnerTextAgent::GetInnerText(mojom::blink::InnerTextParamsPtr params,
                                  GetInnerTextCallback callback) {
  LocalFrame* frame = GetSupplementable()->GetFrame();
  std::move(callback).Run(frame ? InnerTextBuilder::Build(*frame, *params)
                                : nullptr);
}

}  // namespace blink
