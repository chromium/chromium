// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_INSPECTOR_ACCESSIBILITY_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_INSPECTOR_ACCESSIBILITY_AGENT_H_

#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/accessibility.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"

namespace blink {

class AXObject;
class AXObjectCacheImpl;
class InspectorDOMAgent;
class InspectedFrames;
class LocalFrame;

using protocol::Accessibility::AXNode;
using protocol::Accessibility::AXNodeId;

class MODULES_EXPORT InspectorAccessibilityAgent
    : public InspectorBaseAgent<protocol::Accessibility::Metainfo> {
 public:
  InspectorAccessibilityAgent(InspectedFrames*, InspectorDOMAgent*);

  InspectorAccessibilityAgent(const InspectorAccessibilityAgent&) = delete;
  InspectorAccessibilityAgent& operator=(const InspectorAccessibilityAgent&) =
      delete;

  static void ProvideTo(LocalFrame* frame);

  // Base agent methods.
  void Trace(Visitor*) const override;
  void Restore() override;

  // Protocol methods.
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response getPartialAXTree(
      protocol::Maybe<int> dom_node_id,
      protocol::Maybe<int> backend_node_id,
      protocol::Maybe<String> object_id,
      protocol::Maybe<bool> fetch_relatives,
      std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*)
      override;
  protocol::Response getFullAXTree(
      protocol::Maybe<int> depth,
      protocol::Maybe<String> frame_id,
      std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*)
      override;
  protocol::Response getRootAXNode(
      protocol::Maybe<String> frame_id,
      std::unique_ptr<protocol::Accessibility::AXNode>* node) override;
  protocol::Response getAXNodeAndAncestors(
      protocol::Maybe<int> dom_node_id,
      protocol::Maybe<int> backend_node_id,
      protocol::Maybe<String> object_id,
      std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*
          out_nodes) override;
  protocol::Response getChildAXNodes(
      const String& in_id,
      protocol::Maybe<String> frame_id,
      std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*
          out_nodes) override;
  protocol::Response queryAXTree(
      protocol::Maybe<int> dom_node_id,
      protocol::Maybe<int> backend_node_id,
      protocol::Maybe<String> object_id,
      protocol::Maybe<String> accessibleName,
      protocol::Maybe<String> role,
      std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*)
      override;

  void AXEventFired(AXObject* object, ax::mojom::blink::Event event);
  void AXObjectModified(AXObject* object, bool subtree);
  void RefreshFrontendNodes(TimerBase*);

 private:
  bool MarkAXObjectDirty(AXObject* ax_object);
  // Unconditionally enables the agent, even if |enabled_.Get()==true|.
  // For idempotence, call enable().
  void EnableAndReset();
  std::unique_ptr<protocol::Array<AXNode>> WalkAXNodesToDepth(
      Document* document,
      int max_depth);
  std::unique_ptr<AXNode> BuildProtocolAXNodeForDOMNodeWithNoAXNode(
      int backend_node_id) const;
  std::unique_ptr<AXNode> BuildProtocolAXNodeForAXObject(
      AXObject&,
      bool force_name_and_role = false) const;
  std::unique_ptr<AXNode> BuildProtocolAXNodeForIgnoredAXObject(
      AXObject&,
      bool force_name_and_role) const;
  std::unique_ptr<AXNode> BuildProtocolAXNodeForUnignoredAXObject(
      AXObject&) const;
  void FillCoreProperties(AXObject&, AXNode*) const;
  void AddAncestors(AXObject& first_ancestor,
                    AXObject* inspected_ax_object,
                    std::unique_ptr<protocol::Array<AXNode>>& nodes,
                    AXObjectCacheImpl&) const;
  void AddChildren(AXObject& ax_object,
                   bool follow_ignored,
                   std::unique_ptr<protocol::Array<AXNode>>& nodes,
                   AXObjectCacheImpl&) const;
  LocalFrame* FrameFromIdOrRoot(const protocol::Maybe<String>& frame_id);
  void ScheduleAXChangeNotification();
  void RetainAXContextForDocument(Document* document);

  Member<InspectedFrames> inspected_frames_;
  Member<InspectorDOMAgent> dom_agent_;
  InspectorAgentState::Boolean enabled_;
  HashSet<AXID> nodes_requested_;
  HeapHashSet<WeakMember<AXObject>> dirty_nodes_;

  // The agent needs to keep AXContext because it enables caching of a11y nodes.
  HeapHashMap<WeakMember<Document>, std::unique_ptr<AXContext>>
      document_to_context_map_;
  HeapTaskRunnerTimer<InspectorAccessibilityAgent> timer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_INSPECTOR_ACCESSIBILITY_AGENT_H_
