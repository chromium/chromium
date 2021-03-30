// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_DOM_SNAPSHOT_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_DOM_SNAPSHOT_AGENT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/DOMSnapshot.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CharacterData;
class ComputedStyle;
class Document;
class Element;
class InspectedFrames;
class Node;
class PaintLayer;

class CORE_EXPORT InspectorDOMSnapshotAgent final
    : public InspectorBaseAgent<protocol::DOMSnapshot::Metainfo> {
 public:
  InspectorDOMSnapshotAgent(InspectedFrames*, InspectorDOMDebuggerAgent*);
  ~InspectorDOMSnapshotAgent() override;
  void Trace(Visitor*) const override;

  void Restore() override;

  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response getSnapshot(
      std::unique_ptr<protocol::Array<String>> style_filter,
      protocol::Maybe<bool> include_event_listeners,
      protocol::Maybe<bool> include_paint_order,
      protocol::Maybe<bool> include_user_agent_shadow_tree,
      std::unique_ptr<protocol::Array<protocol::DOMSnapshot::DOMNode>>*
          dom_nodes,
      std::unique_ptr<protocol::Array<protocol::DOMSnapshot::LayoutTreeNode>>*
          layout_tree_nodes,
      std::unique_ptr<protocol::Array<protocol::DOMSnapshot::ComputedStyle>>*
          computed_styles) override;
  protocol::Response captureSnapshot(
      std::unique_ptr<protocol::Array<String>> computed_styles,
      protocol::Maybe<bool> include_paint_order,
      protocol::Maybe<bool> include_dom_rects,
      std::unique_ptr<protocol::Array<protocol::DOMSnapshot::DocumentSnapshot>>*
          documents,
      std::unique_ptr<protocol::Array<String>>* strings) override;

  // InspectorInstrumentation API.
  void CharacterDataModified(CharacterData*);
  void DidInsertDOMNode(Node*);

  // Helpers for rects
  static PhysicalRect RectInDocument(const LayoutObject* layout_object);
  static PhysicalRect TextFragmentRectInDocument(
      const LayoutObject* layout_object,
      const LayoutText::TextBoxInfo& text_box);

  using PaintOrderMap = WTF::HashMap<PaintLayer*, int>;
  static std::unique_ptr<PaintOrderMap> BuildPaintLayerTree(Document*);

 private:
  // Unconditionally enables the agent, even if |enabled_.Get()==true|.
  // For idempotence, call enable().
  void EnableAndReset();

  int AddString(const String& string);
  void SetRare(protocol::DOMSnapshot::RareIntegerData* data,
               int index,
               int value);
  void SetRare(protocol::DOMSnapshot::RareStringData* data,
               int index,
               const String& value);
  void SetRare(protocol::DOMSnapshot::RareBooleanData* data, int index);
  void VisitDocument(Document*);

  void VisitNode(Node*, int parent_index);
  void VisitContainerChildren(Node* container, int parent_index);
  void VisitPseudoElements(Element* parent, int parent_index);
  std::unique_ptr<protocol::Array<int>> BuildArrayForElementAttributes(Node*);
  int BuildLayoutTreeNode(LayoutObject*, Node*, int node_index);
  std::unique_ptr<protocol::Array<int>> BuildStylesForNode(Node*);

  static void TraversePaintLayerTree(Document*, PaintOrderMap* paint_order_map);
  static void VisitPaintLayer(PaintLayer*, PaintOrderMap* paint_order_map);

  using CSSPropertyFilter = Vector<const CSSProperty*>;
  using OriginUrlMap = WTF::HashMap<DOMNodeId, String>;

  // State of current snapshot.
  std::unique_ptr<protocol::Array<protocol::DOMSnapshot::DOMNode>> dom_nodes_;
  std::unique_ptr<protocol::Array<protocol::DOMSnapshot::LayoutTreeNode>>
      layout_tree_nodes_;
  std::unique_ptr<protocol::Array<protocol::DOMSnapshot::ComputedStyle>>
      computed_styles_;

  std::unique_ptr<protocol::Array<String>> strings_;
  WTF::HashMap<String, int> string_table_;

  HeapHashMap<Member<const CSSValue>, int> css_value_cache_;
  HashMap<scoped_refptr<const ComputedStyle>, protocol::Array<int>*>
      style_cache_;

  std::unique_ptr<protocol::Array<protocol::DOMSnapshot::DocumentSnapshot>>
      documents_;
  std::unique_ptr<protocol::DOMSnapshot::DocumentSnapshot> document_;

  bool include_snapshot_dom_rects_ = false;
  std::unique_ptr<CSSPropertyFilter> css_property_filter_;
  // Maps a PaintLayer to its paint order index.
  std::unique_ptr<PaintOrderMap> paint_order_map_;
  // Maps a backend node id to the url of the script (if any) that generates
  // the corresponding node.
  std::unique_ptr<OriginUrlMap> origin_url_map_;
  using DocumentOrderMap = HeapHashMap<Member<Document>, int>;
  DocumentOrderMap document_order_map_;
  Member<InspectedFrames> inspected_frames_;
  Member<InspectorDOMDebuggerAgent> dom_debugger_agent_;
  InspectorAgentState::Boolean enabled_;

  DISALLOW_COPY_AND_ASSIGN(InspectorDOMSnapshotAgent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_DOM_SNAPSHOT_AGENT_H_
