// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/inner_html_agent.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/content_extraction/inner_html_builder.h"

namespace blink {

// static
const char InnerHtmlAgent::kSupplementName[] = "InnerHtmlAgent";

// static
InnerHtmlAgent* InnerHtmlAgent::From(Document& document) {
  return Supplement<Document>::From<InnerHtmlAgent>(document);
}

// static
void InnerHtmlAgent::BindReceiver(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::InnerHtmlAgent> receiver) {
  DCHECK(frame && frame->GetDocument());
  auto& document = *frame->GetDocument();
  auto* agent = InnerHtmlAgent::From(document);
  if (!agent) {
    agent = MakeGarbageCollected<InnerHtmlAgent>(
        base::PassKey<InnerHtmlAgent>(), *frame);
    Supplement<Document>::ProvideTo(document, agent);
  }
  agent->Bind(std::move(receiver));
}

InnerHtmlAgent::InnerHtmlAgent(base::PassKey<InnerHtmlAgent>, LocalFrame& frame)
    : Supplement<Document>(*frame.GetDocument()),
      receiver_set_(this, frame.DomWindow()) {}

InnerHtmlAgent::~InnerHtmlAgent() = default;

void InnerHtmlAgent::Bind(
    mojo::PendingReceiver<mojom::blink::InnerHtmlAgent> receiver) {
  // Use `kInternalUserAction` as this task generally results in generating
  // a response to the user.
  receiver_set_.Add(
      std::move(receiver),
      GetSupplementable()->GetTaskRunner(TaskType::kInternalUserInteraction));
}

void InnerHtmlAgent::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_set_);
  Supplement<Document>::Trace(visitor);
}

void InnerHtmlAgent::GetInnerHtml(GetInnerHtmlCallback callback) {
  LocalFrame* frame = GetSupplementable()->GetFrame();
  CHECK(frame);
  std::move(callback).Run(InnerHtmlBuilder::Build(*frame));
}

}  // namespace blink
