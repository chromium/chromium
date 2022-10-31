// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANNOTATION_ANNOTATION_AGENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANNOTATION_ANNOTATION_AGENT_IMPL_H_

#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class AnnotationAgentContainerImpl;
class AnnotationAgentImplTest;
class AnnotationSelector;
class Document;
class RangeInFlatTree;

// This class represents an instantiation of an annotation in a Document. It is
// always owned and stored in an AnnotationAgentContainerImpl. Removing it from
// the container will remove any visible effects on content and disconnect mojo
// bindings. Once removed, an agent cannot be reused.
//
// AnnotationAgentImpl allows its client to provide a selector that it uses to
// attach to a particular Range in the Document. Once attached, the annotation
// adds a visible marker to the content and enables the client to scroll the
// attached content into view. An agent is considered attached if it has found
// a Range and that Range is valid and uncollapsed. The range may become
// invalid in response to changes in a Document's DOM.
// TODO(bokan): Changes in the DOM affecting an annotation should be signaled
// via the AnnotationAgentHost interface.
//
// This class is the renderer end of an annotation. It can be instantiated
// directly from Blink as well (TODO(bokan): this will soon be the case when
// TextFragmentAnchor is refactored to use this class) in which case it need not
// be bound to a host on the browser side. If bound to a corresponding
// AnnotationAgentHost in the browser, it will notify the host of relevant
// events (e.g. attachment, marker clicked, etc.) and self-remove itself from
// the container if the mojo bindings become disconnected (i.e. if the browser
// closes the connection, the AnnotationAgentImpl will be removed)
class CORE_EXPORT AnnotationAgentImpl final
    : public GarbageCollected<AnnotationAgentImpl>,
      public mojom::blink::AnnotationAgent {
 public:
  using PassKey = base::PassKey<AnnotationAgentImpl>;

  // Agents can only be created via AnnotationAgentContainerImpl.
  AnnotationAgentImpl(AnnotationAgentContainerImpl& owning_container,
                      mojom::blink::AnnotationType annotation_type,
                      AnnotationSelector& selector,
                      base::PassKey<AnnotationAgentContainerImpl>);
  ~AnnotationAgentImpl() override = default;

  // Non-copyable or moveable
  AnnotationAgentImpl(const AnnotationAgentImpl&) = delete;
  AnnotationAgentImpl& operator=(const AnnotationAgentImpl&) = delete;

  void Trace(Visitor* visitor) const;

  // Binds this agent to a host.
  void Bind(
      mojo::PendingRemote<mojom::blink::AnnotationAgentHost> host_remote,
      mojo::PendingReceiver<mojom::blink::AnnotationAgent> agent_receiver);

  // Attempts to find a Range of DOM matching the search criteria of the
  // AnnotationSelector passed in the constructor. The search is performed
  // synchronously.
  // TODO(bokan): This is synchronous for the TextFragmentAnchor use case but
  // we'll likely want an async version for typical usage and/or eventually
  // convert TextFragmentAnchor to use an async search.
  void Attach();

  // Returns whether Attach() has been called at least once.
  bool DidTryAttach() const { return did_try_attach_; }

  // Returns true if the agent has performed attachment and resulted in a valid
  // DOM Range. Note that Range is relocated, meaning that it will update in
  // response to changes in DOM. Hence, an AnnotationAgent that IsAttached may
  // become detached due to changes in the Document.
  bool IsAttached() const;

  // Returns true if this agent is bound to a host.
  bool IsBoundForTesting() const;

  // Removes the agent from its container, clearing all its state, mojo
  // bindings, and any visual indications in the document.
  void Remove();

  // mojom::blink::AnnotationAgent
  void ScrollIntoView() override {
    const_cast<const AnnotationAgentImpl*>(this)->ScrollIntoView();
  }
  void ScrollIntoView() const;

  const RangeInFlatTree& GetAttachedRange() const {
    DCHECK(attached_range_.Get());
    return *attached_range_.Get();
  }

  const AnnotationSelector* GetSelector() const { return selector_.Get(); }

  mojom::blink::AnnotationType GetType() const { return type_; }

 private:
  friend AnnotationAgentImplTest;

  // Callback for when AnnotationSelector::FindRange finishes.
  void DidFinishAttach(const RangeInFlatTree* range);

  bool IsRemoved() const;

  // Mojo bindings to the remote host and this' remote. These are always
  // connected as a pair and disconnecting one will cause the other to be
  // disconnected as well.
  HeapMojoRemote<mojom::blink::AnnotationAgentHost> agent_host_;
  HeapMojoReceiver<mojom::blink::AnnotationAgent, AnnotationAgentImpl>
      receiver_;

  // These will be cleared when the agent is removed from its container.
  Member<AnnotationAgentContainerImpl> owning_container_;
  Member<AnnotationSelector> selector_;

  // The attached_range_ is null until the agent performs a successful
  // Attach().
  // TODO(bokan): This doesn't need to be const but is due to the
  // TextFragmentFinder::Client interface.
  Member<const RangeInFlatTree> attached_range_;

  // TODO(bokan): Once we have more of this implemented we'll use the type to
  // determine styling and context menu behavior.
  mojom::blink::AnnotationType type_;

  bool did_try_attach_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANNOTATION_ANNOTATION_AGENT_IMPL_H_
