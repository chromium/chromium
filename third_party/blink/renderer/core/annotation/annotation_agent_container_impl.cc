// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/annotation/annotation_agent_container_impl.h"

#include <algorithm>

#include "base/functional/callback.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/pass_key.h"
#include "components/shared_highlighting/core/common/disabled_sites.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_generator.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_impl.h"
#include "third_party/blink/renderer/core/annotation/annotation_selector.h"
#include "third_party/blink/renderer/core/annotation/node_annotation_selector.h"
#include "third_party/blink/renderer/core/annotation/text_annotation_selector.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_handler.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_selector.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

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
    case mojom::blink::AnnotationType::kGlic:
      return "Glic";
  }
}

String ToString(const mojom::blink::Selector& selector) {
  switch (selector.which()) {
    case mojom::blink::Selector::Tag::kSerializedSelector:
      return selector.get_serialized_selector();
    case mojom::blink::Selector::Tag::kNodeId:
      return String::Number(selector.get_node_id());
  }
}
}  // namespace

// static
const char AnnotationAgentContainerImpl::kSupplementName[] =
    "AnnotationAgentContainerImpl";

void AnnotationAgentContainerImpl::AddObserver(Observer* observer) {
  observers_.insert(observer);
}

void AnnotationAgentContainerImpl::RemoveObserver(Observer* observer) {
  observers_.erase(observer);
}

// static
AnnotationAgentContainerImpl* AnnotationAgentContainerImpl::CreateIfNeeded(
    Document& document) {
  if (!document.IsActive()) {
    return nullptr;
  }

  AnnotationAgentContainerImpl* container = FromIfExists(document);
  if (!container) {
    container =
        MakeGarbageCollected<AnnotationAgentContainerImpl>(document, PassKey());
    Supplement<Document>::ProvideTo(document, container);
  }

  return container;
}

// static
AnnotationAgentContainerImpl* AnnotationAgentContainerImpl::FromIfExists(
    Document& document) {
  return Supplement<Document>::From<AnnotationAgentContainerImpl>(document);
}

// static
void AnnotationAgentContainerImpl::BindReceiver(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::AnnotationAgentContainer> receiver) {
  DCHECK(frame);
  DCHECK(frame->GetDocument());

  // If the current frame embeds a plugin, and that plugin supports annotation,
  // allow the plugin to bind the receiver.
  // TODO(crbug.com/427455182): Support embedded plugins.
  if (frame->View()->Plugins().size() == 1u) {
    WebPluginContainerImpl* web_plugin_container =
        *frame->View()->Plugins().begin();
    CHECK(web_plugin_container);
    WebPlugin* plugin = web_plugin_container->Plugin();
    CHECK(plugin);
    if (plugin->SupportsAnnotation()) {
      plugin->BindAnnotationAgentContainer(std::move(receiver));
      return;
    }
  }

  Document& document = *frame->GetDocument();

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(document);
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
  receivers_.Add(std::move(receiver),
                 GetDocument().GetTaskRunner(TaskType::kInternalDefault));
}

void AnnotationAgentContainerImpl::Trace(Visitor* visitor) const {
  visitor->Trace(receivers_);
  visitor->Trace(agents_);
  visitor->Trace(annotation_agent_generator_);
  visitor->Trace(observers_);
  Supplement<Document>::Trace(visitor);
}

void AnnotationAgentContainerImpl::PerformInitialAttachments() {
  TRACE_EVENT("blink",
              "AnnotationAgentContainerImpl::PerformInitialAttachments",
              "num_agents", agents_.size());
  CHECK(IsLifecycleCleanForAttachment());

  if (GetFrame().GetPage()->IsPageVisible()) {
    page_has_been_visible_ = true;
  }

  for (Observer* observer : observers_) {
    observer->WillPerformAttach();
  }

  for (auto& agent : agents_) {
    if (agent->NeedsAttachment()) {
      // SharedHighlights must wait until the page has been made visible at
      // least once before searching. See:
      // https://wicg.github.io/scroll-to-text-fragment/#search-timing:~:text=If%20a%20UA,in%20background%20documents.
      if (agent->GetType() == mojom::blink::AnnotationType::kSharedHighlight &&
          !page_has_been_visible_) {
        continue;
      }

      agent->Attach(PassKey());
    }
  }
}

AnnotationAgentImpl* AnnotationAgentContainerImpl::CreateUnboundAgent(
    mojom::blink::AnnotationType type,
    AnnotationSelector& selector,
    std::optional<DOMNodeId> search_range_start_node_id) {
  auto* agent_impl = MakeGarbageCollected<AnnotationAgentImpl>(
      *this, type, selector, search_range_start_node_id, PassKey());
  agents_.push_back(agent_impl);

  // Attachment will happen as part of the document lifecycle in a new frame.
  ScheduleBeginMainFrame();

  return agent_impl;
}

void AnnotationAgentContainerImpl::RemoveAgent(AnnotationAgentImpl& agent) {
  agent.Reset(PassKey());
  DCHECK(!agent.IsAttached());
  wtf_size_t index = agents_.Find(&agent);
  DCHECK_NE(index, kNotFound);
  agents_.EraseAt(index);
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
    mojom::blink::SelectorPtr selector,
    std::optional<DOMNodeId> search_range_start_node_id) {
  TRACE_EVENT("blink", "AnnotationAgentContainerImpl::CreateAgent", "type",
              ToString(type), "selector", ToString(*selector));
  DCHECK(GetSupplementable());

  AnnotationSelector* annotation_selector;
  switch (selector->which()) {
    case mojom::blink::Selector::Tag::kSerializedSelector:
      annotation_selector =
          AnnotationSelector::Deserialize(selector->get_serialized_selector());
      // If the selector was invalid, we should drop the bindings which the host
      // will see as a disconnect.
      // TODO(bokan): We could support more graceful fallback/error reporting by
      // calling an error method on the host.
      if (!annotation_selector) {
        TRACE_EVENT_INSTANT("blink", "Failed to deserialize selector");
        return;
      }
      break;
    case mojom::blink::Selector::Tag::kNodeId:
      annotation_selector =
          MakeGarbageCollected<NodeAnnotationSelector>(selector->get_node_id());
      break;
  }

  auto* agent_impl = CreateUnboundAgent(type, *annotation_selector,
                                        search_range_start_node_id);
  agent_impl->Bind(std::move(host_remote), std::move(agent_receiver));
}

void AnnotationAgentContainerImpl::CreateAgentFromSelection(
    mojom::blink::AnnotationType type,
    CreateAgentFromSelectionCallback callback) {
  TRACE_EVENT("blink", "AnnotationAgentContainerImpl::CreateAgentFromSelection",
              "type", ToString(type));
  DCHECK(annotation_agent_generator_);
  annotation_agent_generator_->GetForCurrentSelection(
      type, blink::BindOnce(
                &AnnotationAgentContainerImpl::DidFinishSelectorGeneration,
                WrapWeakPersistent(this), std::move(callback)));
}

void AnnotationAgentContainerImpl::RemoveAgentsOfType(
    mojom::blink::AnnotationType type) {
  TRACE_EVENT("blink", "AnnotationAgentContainerImpl::RemoveAgentsOfType",
              "type", ToString(type));
  HeapVector<Member<AnnotationAgentImpl>> agents_to_reset;
  EraseIf(agents_,[type, &agents_to_reset](AnnotationAgentImpl* agent) {
    if (agent->GetType() == type) {
      agents_to_reset.push_back(agent);
      return true;
    }
    return false;
  });

  // Note: We cannot call Reset() directly inside the EraseIf predicate above
  // because Reset() can result in PerformInitialAttachments() being called,
  // which will iterate over `agents_` in the middle of the EraseIf operation
  // (which is not safe).
  for (AnnotationAgentImpl* agent : agents_to_reset) {
    agent->Reset(PassKey());
  }
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
  TRACE_EVENT("blink",
              "AnnotationAgentContainerImpl::DidFinishSelectorGeneration",
              "type", ToString(type));

  if (error != shared_highlighting::LinkGenerationError::kNone) {
    std::move(callback).Run(/*SelectorCreationResult=*/nullptr, error,
                            ready_status);
    return;
  }

  // If the document was detached then selector generation must have returned
  // an error.
  CHECK(GetSupplementable());

  // TODO(bokan): Why doesn't this clear selection?
  GetFrame().Selection().Clear();

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

  AnnotationAgentImpl* agent_impl = CreateUnboundAgent(
      type, *annotation_selector, /*search_range_start_node_id=*/std::nullopt);
  agent_impl->Bind(std::move(pending_host_remote),
                   std::move(pending_agent_receiver));
}

void AnnotationAgentContainerImpl::OpenedContextMenuOverSelection() {
  DCHECK(annotation_agent_generator_);
  if (!ShouldPreemptivelyGenerate())
    return;

  annotation_agent_generator_->PreemptivelyGenerateForCurrentSelection();
}

bool AnnotationAgentContainerImpl::IsLifecycleCleanForAttachment() const {
  return GetDocument().HasFinishedParsing() &&
         !GetDocument().NeedsLayoutTreeUpdate() &&
         !GetFrame().View()->NeedsLayout();
}

bool AnnotationAgentContainerImpl::ShouldPreemptivelyGenerate() {
  if (!shared_highlighting::ShouldOfferLinkToText(GURL(GetDocument().Url()))) {
    return false;
  }

  if (RuntimeEnabledFeatures::
          NonEmptyVisibleTextSelectionForTextFragmentEnabled()) {
    if (!GetFrame().Selection().HasVisibleText()) {
      return false;
    }
  } else {
    if (GetFrame().Selection().SelectedText().empty()) {
      return false;
    }
  }

  if (GetFrame().IsOutermostMainFrame()) {
    return true;
  }

  // Only generate for iframe urls if they are supported
  return shared_highlighting::SupportsLinkGenerationInIframe(
      GURL(GetFrame().GetDocument()->Url()));
}

void AnnotationAgentContainerImpl::ScheduleBeginMainFrame() {
  GetFrame().GetPage()->GetChromeClient().ScheduleAnimation(GetFrame().View());
}

Document& AnnotationAgentContainerImpl::GetDocument() const {
  Document* document = GetSupplementable();
  CHECK(document);
  return *document;
}

LocalFrame& AnnotationAgentContainerImpl::GetFrame() const {
  LocalFrame* frame = GetDocument().GetFrame();
  CHECK(frame);
  return *frame;
}

}  // namespace blink
