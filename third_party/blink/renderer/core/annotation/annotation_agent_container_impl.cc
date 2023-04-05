// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/annotation/annotation_agent_container_impl.h"

#include "base/functional/callback.h"
#include "base/trace_event/typed_macros.h"
#include "components/shared_highlighting/core/common/disabled_sites.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_generator.h"
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

namespace {
const char* ToString(mojom::blink::AnnotationType type) {
  switch (type) {
    case mojom::blink::AnnotationType::kSharedHighlight:
      return "SharedHighlight";
    case mojom::blink::AnnotationType::kUserNote:
      return "UserNote";
    case mojom::blink::AnnotationType::kTextFinder:
      return "TextFinder";
  }
}
}  // namespace

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
      receivers_(this, document.GetExecutionContext()) {
  LocalFrame* frame = document.GetFrame();
  DCHECK(frame);

  annotation_agent_generator_ =
      MakeGarbageCollected<AnnotationAgentGenerator>(frame);
}

void AnnotationAgentContainerImpl::Bind(
    mojo::PendingReceiver<mojom::blink::AnnotationAgentContainer> receiver) {
  receivers_.Add(std::move(receiver), GetSupplementable()->GetTaskRunner(
                                          TaskType::kInternalDefault));
}

void AnnotationAgentContainerImpl::Trace(Visitor* visitor) const {
  visitor->Trace(receivers_);
  visitor->Trace(agents_);
  visitor->Trace(annotation_agent_generator_);
  Supplement<Document>::Trace(visitor);
}

void AnnotationAgentContainerImpl::FinishedParsing() {
  TRACE_EVENT("blink", "AnnotationAgentContainerImpl::FinishedParsing",
              "num_agents", agents_.size());
  for (auto& agent : agents_) {
    // TODO(crbug.com/1379741): Don't try attaching shared highlights like
    // this. Their lifetime is currently owned by TextFragmentAnchor which is
    // driven by the document lifecycle. Attach() itself may perform lifecycle
    // updates and has safeguards to prevent reentrancy so it's important to
    // not call Attach outside of that process. See also: comment in
    // Document::ApplyScrollRestorationLogic. Eventually we'd like to move the
    // lifecycle management of shared highlight annotations out of
    // TextFragmentAnchor. When that happens we can remove this exception.
    if (agent->GetType() == mojom::blink::AnnotationType::kSharedHighlight)
      continue;

    if (!agent->DidTryAttach())
      agent->Attach();
  }
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
  TRACE_EVENT("blink", "AnnotationAgentContainerImpl::CreateAgent", "type",
              ToString(type), "selector", serialized_selector);
  DCHECK(GetSupplementable());

  AnnotationSelector* selector =
      AnnotationSelector::Deserialize(serialized_selector);

  // If the selector was invalid, we should drop the bindings which the host
  // will see as a disconnect.
  // TODO(bokan): We could support more graceful fallback/error reporting by
  // calling an error method on the host.
  if (!selector) {
    TRACE_EVENT_INSTANT("blink", "Failed to deserialize selector");
    return;
  }

  auto* agent_impl = MakeGarbageCollected<AnnotationAgentImpl>(
      *this, type, *selector, PassKey());
  agents_.insert(agent_impl);
  agent_impl->Bind(std::move(host_remote), std::move(agent_receiver));

  Document& document = *GetSupplementable();

  // We may have received this message before the document finishes parsing.
  // Postpone attachment for now; these agents will try attaching when the
  // document finishes parsing.
  if (document.HasFinishedParsing()) {
    agent_impl->Attach();
  } else {
    TRACE_EVENT_INSTANT("blink", "Waiting on parse to attach");
  }
}

void AnnotationAgentContainerImpl::CreateAgentFromSelection(
    mojom::blink::AnnotationType type,
    CreateAgentFromSelectionCallback callback) {
  DCHECK(annotation_agent_generator_);
  annotation_agent_generator_->GetForCurrentSelection(
      type,
      WTF::BindOnce(&AnnotationAgentContainerImpl::DidFinishSelectorGeneration,
                    WrapWeakPersistent(this), std::move(callback)));
}

// TODO(cheickcisse@): Move shared highlighting enums, also used in user note to
// annotation.mojom.
void AnnotationAgentContainerImpl::DidFinishSelectorGeneration(
    CreateAgentFromSelectionCallback callback,
    mojom::blink::AnnotationType type,
    shared_highlighting::LinkGenerationReadyStatus ready_status,
    const String& selected_text,
    const TextFragmentSelector& selector,
    shared_highlighting::LinkGenerationError error) {
  if (error != shared_highlighting::LinkGenerationError::kNone) {
    std::move(callback).Run(/*SelectorCreationResult=*/nullptr, error,
                            ready_status);
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

  mojom::blink::SelectorCreationResultPtr selector_creation_result =
      mojom::blink::SelectorCreationResult::New();
  selector_creation_result->host_receiver =
      pending_host_remote.InitWithNewPipeAndPassReceiver();
  selector_creation_result->agent_remote =
      pending_agent_receiver.InitWithNewPipeAndPassRemote();
  selector_creation_result->serialized_selector =
      annotation_selector->Serialize();
  DCHECK(!selector_creation_result->serialized_selector.empty())
      << "User note creation received an empty selector for mojo binding "
         "result";
  selector_creation_result->selected_text = selected_text;
  DCHECK(!selector_creation_result->selected_text.empty())
      << "User note creation received an empty text for mojo binding result";

  std::move(callback).Run(std::move(selector_creation_result), error,
                          ready_status);

  AnnotationAgentImpl* agent_impl =
      CreateUnboundAgent(type, *annotation_selector);
  agent_impl->Bind(std::move(pending_host_remote),
                   std::move(pending_agent_receiver));

  agent_impl->Attach();
}

void AnnotationAgentContainerImpl::OpenedContextMenuOverSelection() {
  DCHECK(annotation_agent_generator_);
  if (!ShouldPreemptivelyGenerate())
    return;

  annotation_agent_generator_->PreemptivelyGenerateForCurrentSelection();
}

bool AnnotationAgentContainerImpl::ShouldPreemptivelyGenerate() {
  Document* document = GetSupplementable();
  DCHECK(document);

  LocalFrame* frame = document->GetFrame();
  DCHECK(frame);

  if (!shared_highlighting::ShouldOfferLinkToText(
          GURL(frame->GetDocument()->Url()))) {
    return false;
  }

  if (frame->Selection().SelectedText().empty())
    return false;

  if (frame->IsOutermostMainFrame())
    return true;

  // Only generate for iframe urls if they are supported
  return base::FeatureList::IsEnabled(
             shared_highlighting::kSharedHighlightingAmp) &&
         shared_highlighting::SupportsLinkGenerationInIframe(
             GURL(frame->GetDocument()->Url()));
}

}  // namespace blink
