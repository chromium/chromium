// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_AGENT_H_

#include "base/functional/callback_forward.h"
#include "base/memory/stack_allocated.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/lib/validation_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/modules/content_extraction/paid_content.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {
class Document;
class LayoutIFrame;
class LayoutObject;
class LocalFrame;

// AIPageContent is responsible for handling requests for inner-text. It calls
// to InnerTextBuilder to handle building of the text.
class MODULES_EXPORT AIPageContentAgent final
    : public GarbageCollected<AIPageContentAgent>,
      public mojom::blink::AIPageContentAgent,
      public Supplement<Document>,
      public LocalFrameView::LifecycleNotificationObserver {
 public:
  static const char kSupplementName[];
  static AIPageContentAgent* From(Document&);
  static void BindReceiver(
      LocalFrame* frame,
      mojo::PendingReceiver<mojom::blink::AIPageContentAgent> receiver);

  static AIPageContentAgent* GetOrCreateForTesting(Document&);

  AIPageContentAgent(base::PassKey<AIPageContentAgent>, LocalFrame&);
  AIPageContentAgent(const AIPageContentAgent&) = delete;
  AIPageContentAgent& operator=(const AIPageContentAgent&) = delete;
  ~AIPageContentAgent() override;

  void Trace(Visitor* visitor) const override;

  // mojom::blink::AIPageContentAgent overrides.
  void GetAIPageContent(mojom::blink::AIPageContentOptionsPtr options,
                        GetAIPageContentCallback callback) override;

  // public for testing.
  mojom::blink::AIPageContentPtr GetAIPageContentInternal(
      const mojom::blink::AIPageContentOptions& options) const;
  // LocalFrameView::LifecycleNotificationObserver overrides.
  void DidFinishPostLifecycleSteps(const LocalFrameView&) override;

 private:
  void GetAIPageContentSync(mojom::blink::AIPageContentOptionsPtr options,
                            GetAIPageContentCallback callback,
                            base::TimeTicks start_time) const;

  // Synchronously services a single request.
  class ContentBuilder {
    STACK_ALLOCATED();

   public:
    explicit ContentBuilder(const mojom::blink::AIPageContentOptions& options);
    ~ContentBuilder();

    mojom::blink::AIPageContentPtr Build(LocalFrame& frame);

   private:
    class RecursionData {
      STACK_ALLOCATED();

     public:
      RecursionData(const ComputedStyle& document_style);

      bool is_aria_disabled = false;
      const ComputedStyle& document_style;
      int stack_depth = 0;
    };

    // Returns true if any descendant of `object` has a computed value of
    // visible for `visibility`.
    bool WalkChildren(const LayoutObject& object,
                      mojom::blink::AIPageContentNode& content_node,
                      const RecursionData& recursion_data);
    void ProcessIframe(const LayoutIFrame& object,
                       mojom::blink::AIPageContentNode& content_node,
                       const RecursionData& recursion_data);
    mojom::blink::AIPageContentNodePtr MaybeGenerateContentNode(
        const LayoutObject& object,
        const RecursionData& recursion_data);
    void AddPageInteractionInfo(const Document& document,
                                mojom::blink::AIPageContent& page_content);
    void AddFrameData(const LocalFrame& frame,
                      mojom::blink::AIPageContentFrameData& frame_data);
    void AddFrameInteractionInfo(
        const LocalFrame& frame,
        mojom::blink::AIPageContentFrameInteractionInfo&
            frame_interaction_info);
    void AddAriaRole(const LayoutObject& object,
                     mojom::blink::AIPageContentAttributes& attributes);
    void AddNodeInteractionInfo(
        const LayoutObject& object,
        mojom::blink::AIPageContentAttributes& attributes,
        bool is_aria_disabled) const;
    void AddInteractionInfoForHitTesting(
        const Node* node,
        mojom::blink::AIPageContentNodeInteractionInfo& interaction_info) const;
    void AddMetaData(
        const LocalFrame& frame,
        WTF::Vector<mojom::blink::AIPageContentMetaPtr>& meta_data) const;
    void AddNodeGeometry(
        const LayoutObject& object,
        mojom::blink::AIPageContentAttributes& attributes) const;
    void AddAnnotatedRoles(const LayoutObject& object,
                           Vector<mojom::blink::AIPageContentAnnotatedRole>&
                               annotated_roles) const;
    void AddLabel(const LayoutObject& object,
                  mojom::blink::AIPageContentAttributes& attributes) const;
    // Adds the control node id if this is a label associated with a form
    // control. This includes both explicit association using for, or
    // implicit association when the input node is a descendant of the label
    // node.
    void AddForDomNodeId(
        const LayoutObject& object,
        mojom::blink::AIPageContentAttributes& attributes) const;
    bool IsGenericContainer(
        const LayoutObject& object,
        const mojom::blink::AIPageContentAttributes& attributes) const;

    void AddInteractiveNode(DOMNodeId dom_node_id);
    void ComputeHitTestableNodesInViewport(
        const LocalFrame& frame,
        mojom::blink::AIPageContentFrameData& frame_data);

    // The set of nodes which are involved in a user interaction and must
    // produce a ContentNode.
    base::flat_set<DOMNodeId> interactive_dom_node_ids_;

    const raw_ref<const mojom::blink::AIPageContentOptions> options_;

    base::flat_map<DOMNodeId, int32_t> dom_node_to_z_order_;

    // Whether the stack depth has exceeded the max tree depth.
    bool stack_depth_exceeded_ = false;

    // List of nodes marked as isAccessibleForFree=false.
    PaidContent paid_content_;
  };

  void Bind(mojo::PendingReceiver<mojom::blink::AIPageContentAgent> receiver);

  HeapMojoReceiverSet<mojom::blink::AIPageContentAgent, AIPageContentAgent>
      receiver_set_;
  // Already registered for lifetime notifications.
  bool is_registered_ = false;
  // Tasks to run when post lifecycle.
  WTF::Vector<base::OnceClosure> async_extraction_tasks_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_AGENT_H_
