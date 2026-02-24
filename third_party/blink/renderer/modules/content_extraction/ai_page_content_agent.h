// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_AGENT_H_

#include "base/functional/callback.h"
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
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

namespace blink {
class Document;
class LayoutIFrame;
class LayoutObject;
class LocalFrame;
class Node;
#if DCHECK_IS_ON()
class AutoBuildHelper;
#endif

// AIPageContent is responsible for handling requests for inner-text. It calls
// to InnerTextBuilder to handle building of the text.
class MODULES_EXPORT AIPageContentAgent final
    : public GarbageCollected<AIPageContentAgent>,
      public mojom::blink::AIPageContentAgent,
      public Supplement<Document>,
      public LocalFrameView::LifecycleNotificationObserver {
 public:
  enum class CustomPasswordSource {
    // Non-standard CSS masking via `-webkit-text-security`.
    kCSS,
    // JS sets a value that is mostly mask characters.
    kJavaScript,
  };

  static const char kSupplementName[];
  static AIPageContentAgent* From(Document&);
  static void BindReceiver(
      LocalFrame* frame,
      mojo::PendingReceiver<mojom::blink::AIPageContentAgent> receiver);

  static AIPageContentAgent* GetOrCreateForTesting(Document&);
#if DCHECK_IS_ON()
  // If enabled, the ContentNode tree will be automatically built on page load.
  static void EnableAutomaticActionableExtractionOnPageLoadForTesting(
      LocalFrame&);
#endif

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

  String DumpContentNodeTreeForTest();
  String DumpContentNodeForTest(Node* node);

#if DCHECK_IS_ON()
  // Called by the DOMContentLoaded listener to kick off auto-build; this is
  // only used by tests to ensure we run cleanly without triggering crashes or
  // checks/dchecks.
  void RunAutoBuildAfterDOMContentLoadedForTesting();
#endif

 private:
  void GetAIPageContentSync(mojom::blink::AIPageContentOptionsPtr options,
                            GetAIPageContentCallback callback,
                            base::TimeTicks start_time) const;

  std::optional<CustomPasswordSource> ExistingCustomPasswordReason(
      const LayoutObject& object) const;

  // Synchronously services a single request.
  class ContentBuilder {
    STACK_ALLOCATED();

   public:
    ContentBuilder(const mojom::blink::AIPageContentOptions& options,
                   const AIPageContentAgent& agent);
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
      DOMNodeId accessibility_focused_node_id = kInvalidDOMNodeId;
    };

    bool actionable_mode() const {
      return options_->mode ==
             mojom::blink::AIPageContentMode::kActionableElements;
    }
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
    void AddFrameData(LocalFrame& frame,
                      mojom::blink::AIPageContentFrameData& frame_data);
    void AddFrameInteractionInfo(
        const LocalFrame& frame,
        mojom::blink::AIPageContentFrameInteractionInfo&
            frame_interaction_info);
    void MaybeAddPopupData(LocalFrame& frame,
                           mojom::blink::AIPageContentFrameData& frame_data);
    void AddNodeInteractionInfo(
        const LayoutObject& object,
        mojom::blink::AIPageContentAttributes& attributes,
        bool is_aria_disabled) const;
    void AddInteractionInfoForHitTesting(
        const Node* node,
        mojom::blink::AIPageContentNodeInteractionInfo& interaction_info) const;
    void AddMetaData(
        const LocalFrame& frame,
        Vector<mojom::blink::AIPageContentMetaPtr>& meta_data) const;
    void AddNodeGeometry(const LayoutObject& object,
                         mojom::blink::AIPageContentAttributes& attributes,
                         DOMNodeId accessibility_focused_node_id);
    void AddAnnotatedRoles(const LayoutObject& object,
                           Vector<mojom::blink::AIPageContentAnnotatedRole>&
                               annotated_roles) const;
    void AddLabel(const LayoutObject& object,
                  mojom::blink::AIPageContentAttributes& attributes) const;
    // Adds the control node id if this is a label associated with a form
    // control. This includes both explicit association using for, or
    // implicit association when the input node is a descendant of the label
    // node.
    void PopulateLabelForDomNodeId(
        const LayoutObject& object,
        mojom::blink::AIPageContentAttributes& attributes);
    // Returns whether `attributes.dom_node_id` should be emitted in APC output.
    //
    // If `node_id_allowlist` is unset, emit all ids.
    //
    // Otherwise, emit ids for:
    // 1. Actionable targets (`node_interaction_info` in actionable mode).
    // 2. Metadata-linked nodes tracked in `interactive_dom_node_ids_` (focused
    //    element, accessibility focus, selection endpoints, label-for targets,
    //    popup openers).
    // 3. Attribute types listed in `node_id_allowlist`.
    // This method must not allocate a new DomNodeIds entry for nodes that are
    // ultimately suppressed by policy. Callers should only mint new ids after
    // this returns true.
    bool ShouldEmitNodeIdForOutput(
        const LayoutObject& object,
        const mojom::blink::AIPageContentAttributes& attributes) const;
    // Returns true when the caller policy allowlists this `attribute_type` for
    // id emission.
    bool IsNodeIdAttributeTypeAllowlisted(
        mojom::blink::AIPageContentAttributeType attribute_type) const;
    bool IsGenericContainer(
        const LayoutObject& object,
        const mojom::blink::AIPageContentAttributes& attributes) const;

    DOMNodeId AddInteractiveNode(Node& node);
    void AddInteractiveNode(DOMNodeId dom_node_id);
    void ComputeHitTestableNodesInViewport(const LocalFrame& frame);

    void UpdateLifecycle(Document& document);

    void TrackPasswordRedactionIfNeeded(
        const LayoutObject& object,
        mojom::blink::AIPageContentAttributes& attributes,
        std::optional<gfx::Rect> visible_bounding_box = std::nullopt);

    // Applies custom password-like heuristics for text fields. This covers
    // author-defined password controls which do not use <input type=password>.
    void ApplyCustomPasswordRedactionHeuristicsIfNeeded(
        const LayoutObject& object,
        mojom::blink::AIPageContentAttributes& attributes) const;

    bool ShouldAddNodeGeometry(
        const LayoutObject& object,
        const mojom::blink::AIPageContentAttributes& attributes,
        DOMNodeId accessibility_focused_node_id) const;

    Vector<gfx::Rect> visible_bounding_box_for_passwords_;

    // The set of node ids that must always be emitted in APC output for
    // round-trippable metadata and interaction flows.
    HashSet<DOMNodeId, IntWithZeroKeyHashTraits<DOMNodeId>>
        interactive_dom_node_ids_;

    const raw_ref<const mojom::blink::AIPageContentOptions> options_;
    const AIPageContentAgent& agent_;

    // Keyed by Node identity within a single extraction pass.
    HeapHashMap<Member<const Node>, int32_t> dom_node_to_z_order_;

    // Whether the stack depth has exceeded the max tree depth.
    bool stack_depth_exceeded_ = false;

    // List of nodes marked as isAccessibleForFree=false.
    PaidContent paid_content_;
  };

  void Bind(mojo::PendingReceiver<mojom::blink::AIPageContentAgent> receiver);
  void EnsureLifecycleObserverRegistered();

  HeapMojoReceiverSet<mojom::blink::AIPageContentAgent, AIPageContentAgent>
      receiver_set_;
  // Already registered for lifetime notifications.
  bool is_lifecycle_observer_registered_ = false;
  // Tasks to run when post lifecycle.
  Vector<base::OnceClosure> async_extraction_tasks_;

#if DCHECK_IS_ON()
  void ListenForDOMContentLoadedForAutoBuild();
  AutoBuildHelper* GetOrCreateAutoBuildHelper();
  Member<AutoBuildHelper> auto_build_helper_;
  friend class AutoBuildHelper;
#endif

  // Persistent set of DOM node IDs determined to be password-like via heuristic
  // redaction (e.g. `-webkit-text-security` or JS masking patterns).
  //
  // This is mutable so it can be updated by const extraction calls.
  mutable HashMap<DOMNodeId,
                  CustomPasswordSource,
                  IntWithZeroKeyHashTraits<DOMNodeId>>
      custom_password_decision_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_AGENT_H_
