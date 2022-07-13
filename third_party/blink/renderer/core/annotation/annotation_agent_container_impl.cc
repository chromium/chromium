// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/annotation/annotation_agent_container_impl.h"

#include "base/callback.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_impl.h"
#include "third_party/blink/renderer/core/annotation/annotation_selector.h"
#include "third_party/blink/renderer/core/annotation/text_annotation_selector.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_handler.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_selector.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

// static
const char AnnotationAgentContainerImpl::kSupplementName[] =
    "AnnotationAgentContainerImpl";

// static
AnnotationAgentContainerImpl* AnnotationAgentContainerImpl::From(
    Document& document) {
  if (!document.IsActive())
    return nullptr;

  AnnotationAgentContainerImpl* container =
      Supplement<Document>::From<AnnotationAgentContainerImpl>(document);
  if (!container) {
    container =
        MakeGarbageCollected<AnnotationAgentContainerImpl>(document, PassKey());
    Supplement<Document>::ProvideTo(document, container);
  }

  return container;
}

// static
void AnnotationAgentContainerImpl::BindReceiver(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::AnnotationAgentContainer> receiver) {
  DCHECK(frame);
  DCHECK(frame->GetDocument());
  Document& document = *frame->GetDocument();

  auto* container = AnnotationAgentContainerImpl::From(document);
  if (!container)
    return;

  container->Bind(std::move(receiver));
}

AnnotationAgentContainerImpl::AnnotationAgentContainerImpl(Document& document,
                                                           PassKey)
    : Supplement<Document>(document),
      receivers_(this, document.GetExecutionContext()) {}

void AnnotationAgentContainerImpl::Bind(
    mojo::PendingReceiver<mojom::blink::AnnotationAgentContainer> receiver) {
  receivers_.Add(std::move(receiver), GetSupplementable()->GetTaskRunner(
                                          TaskType::kInternalDefault));
}

void AnnotationAgentContainerImpl::Trace(Visitor* visitor) const {
  visitor->Trace(receivers_);
  visitor->Trace(agents_);
  Supplement<Document>::Trace(visitor);
}

AnnotationAgentImpl* AnnotationAgentContainerImpl::CreateUnboundAgent(
    mojom::blink::AnnotationType type,
    AnnotationSelector& selector) {
  auto* agent_impl = MakeGarbageCollected<AnnotationAgentImpl>(
      *this, type, selector, PassKey());
  agents_.insert(agent_impl);

  // TODO(bokan): This is a stepping stone in refactoring the
  // TextFragmentHandler. When we replace it with a browser-side manager it may
  // make for a better API to have components register a handler for an
  // annotation type with AnnotationAgentContainer.
  // https://crbug.com/1303887.
  if (type == mojom::blink::AnnotationType::kSharedHighlight) {
    TextFragmentHandler::DidCreateTextFragment(*agent_impl,
                                               *GetSupplementable());
  }

  return agent_impl;
}

void AnnotationAgentContainerImpl::RemoveAgent(AnnotationAgentImpl& agent,
                                               AnnotationAgentImpl::PassKey) {
  DCHECK(!agent.IsAttached());
  auto itr = agents_.find(&agent);
  DCHECK_NE(itr, agents_.end());
  agents_.erase(itr);
}

HeapHashSet<Member<AnnotationAgentImpl>>
AnnotationAgentContainerImpl::GetAgentsOfType(
    mojom::blink::AnnotationType type) {
  HeapHashSet<Member<AnnotationAgentImpl>> agents_of_type;
  for (auto& agent : agents_) {
    if (agent->GetType() == type)
      agents_of_type.insert(agent);
  }

  return agents_of_type;
}

void AnnotationAgentContainerImpl::CreateAgent(
    mojo::PendingRemote<mojom::blink::AnnotationAgentHost> host_remote,
    mojo::PendingReceiver<mojom::blink::AnnotationAgent> agent_receiver,
    mojom::blink::AnnotationType type,
    const String& serialized_selector) {
  DCHECK(GetSupplementable());

  AnnotationSelector* selector =
      AnnotationSelector::Deserialize(serialized_selector);

  // If the selector was invalid, we should drop the bindings which the host
  // will see as a disconnect.
  // TODO(bokan): We could support more graceful fallback/error reporting by
  // calling an error method on the host.
  if (!selector)
    return;

  auto* agent_impl = MakeGarbageCollected<AnnotationAgentImpl>(
      *this, type, *selector, PassKey());
  agents_.insert(agent_impl);
  agent_impl->Bind(std::move(host_remote), std::move(agent_receiver));
  agent_impl->Attach();
}

void AnnotationAgentContainerImpl::CreateAgentFromSelection(
    mojom::blink::AnnotationType type,
    CreateAgentFromSelectionCallback callback) {
  // Both Document and LocalFrame must be non-null since the mojo connections
  // are closed when the Document shuts down its execution context.
  Document* document = GetSupplementable();
  DCHECK(document);

  LocalFrame* frame = document->GetFrame();
  DCHECK(frame);

  VisibleSelectionInFlatTree selection =
      frame->Selection().ComputeVisibleSelectionInFlatTree();
  if (selection.IsNone() || !selection.IsRange()) {
    std::move(callback).Run(mojo::NullReceiver(), mojo::NullRemote(),
                            /*serialized_selector=*/"", /*selected_text=*/"");
    return;
  }

  EphemeralRangeInFlatTree selection_range(selection.Start(), selection.End());

  if (selection_range.IsNull() || selection_range.IsCollapsed()) {
    std::move(callback).Run(mojo::NullReceiver(), mojo::NullRemote(),
                            /*serialized_selector=*/"", /*selected_text=*/"");
    return;
  }

  RangeInFlatTree* current_selection_range =
      MakeGarbageCollected<RangeInFlatTree>(selection_range.StartPosition(),
                                            selection_range.EndPosition());

  // TODO(crbug.com/1313967): We may be able to reduce the latency of adding a
  // new note by starting the generator when the context menu is opened so that
  // by the time the user selects "add a note" the selector is already
  // generated. We already do this for shared-highlighting so we could just
  // generalize that code, see
  // TextFragmentHandler::OpenedContextMenuOverSelection.
  auto* generator = MakeGarbageCollected<TextFragmentSelectorGenerator>(frame);

  // The generator is kept alive by the callback.
  generator->Generate(
      *current_selection_range,
      WTF::Bind(&AnnotationAgentContainerImpl::DidFinishSelectorGeneration,
                WrapWeakPersistent(this), WrapPersistent(generator), type,
                std::move(callback)));
}

void AnnotationAgentContainerImpl::DidFinishSelectorGeneration(
    TextFragmentSelectorGenerator* generator,
    mojom::blink::AnnotationType type,
    CreateAgentFromSelectionCallback callback,
    const TextFragmentSelector& selector,
    shared_highlighting::LinkGenerationError error) {
  if (error != shared_highlighting::LinkGenerationError::kNone) {
    std::move(callback).Run(mojo::NullReceiver(), mojo::NullRemote(),
                            /*serialized_selector=*/"", /*selected_text=*/"");
    return;
  }

  // TODO(bokan): Should we clear the frame selection?
  {
    // If the document were detached selector generation will return above with
    // an error.
    Document* document = GetSupplementable();
    DCHECK(document);

    LocalFrame* frame = document->GetFrame();
    DCHECK(frame);

    frame->Selection().Clear();
  }

  mojo::PendingRemote<mojom::blink::AnnotationAgentHost> pending_host_remote;
  mojo::PendingReceiver<mojom::blink::AnnotationAgent> pending_agent_receiver;

  // TODO(bokan): This replies with the selector before performing attachment
  // (i.e. before the highlight is shown). If we'd prefer to guarantee the
  // highlight is showing before the creation flow begins we can swap these.
  auto* annotation_selector =
      MakeGarbageCollected<TextAnnotationSelector>(selector);
  std::move(callback).Run(pending_host_remote.InitWithNewPipeAndPassReceiver(),
                          pending_agent_receiver.InitWithNewPipeAndPassRemote(),
                          annotation_selector->Serialize(),
                          generator->GetSelectorTargetText());

  AnnotationAgentImpl* agent_impl =
      CreateUnboundAgent(type, *annotation_selector);
  agent_impl->Bind(std::move(pending_host_remote),
                   std::move(pending_agent_receiver));

  agent_impl->Attach();
}

}  // namespace blink
