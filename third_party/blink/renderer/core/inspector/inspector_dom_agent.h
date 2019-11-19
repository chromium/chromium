/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_DOM_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_DOM_AGENT_H_

#include <memory>
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_listener_map.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/DOM.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8-inspector.h"

namespace blink {

class CharacterData;
class Color;
class DOMEditor;
class Document;
class DocumentLoader;
class Element;
class ExceptionState;
class FloatQuad;
class HTMLFrameOwnerElement;
class HTMLPortalElement;
class HTMLSlotElement;
class V0InsertionPoint;
class InspectedFrames;
class InspectorHistory;
class Node;
class QualifiedName;
class PseudoElement;
class InspectorRevalidateDOMTask;
class ShadowRoot;

class CORE_EXPORT InspectorDOMAgent final
    : public InspectorBaseAgent<protocol::DOM::Metainfo> {
 public:
  struct CORE_EXPORT DOMListener : public GarbageCollectedMixin {
    virtual ~DOMListener() = default;
    virtual void DidAddDocument(Document*) = 0;
    virtual void DidRemoveDocument(Document*) = 0;
    virtual void DidRemoveDOMNode(Node*) = 0;
    virtual void DidModifyDOMAttr(Element*) = 0;
  };

  class CORE_EXPORT InspectorSourceLocation final
      : public GarbageCollected<InspectorSourceLocation> {
   public:
    InspectorSourceLocation(std::unique_ptr<SourceLocation> source_location)
        : source_location_(std::move(source_location)) {}

    SourceLocation& GetSourceLocation() { return *source_location_; }
    virtual void Trace(blink::Visitor* visitor) {}

   private:
    std::unique_ptr<SourceLocation> source_location_;
  };

  static protocol::Response ToResponse(ExceptionState&);
  static bool GetPseudoElementType(PseudoId, String*);
  static protocol::DOM::ShadowRootType GetShadowRootType(ShadowRoot*);
  static ShadowRoot* UserAgentShadowRoot(Node*);
  static Color ParseColor(protocol::DOM::RGBA*);

  InspectorDOMAgent(v8::Isolate*,
                    InspectedFrames*,
                    v8_inspector::V8InspectorSession*);
  ~InspectorDOMAgent() override;
  void Trace(blink::Visitor*) override;

  void Restore() override;

  HeapVector<Member<Document>> Documents();
  void Reset();

  // Methods called from the frontend for DOM nodes inspection.
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response getDocument(
      protocol::Maybe<int> depth,
      protocol::Maybe<bool> traverse_frames,
      std::unique_ptr<protocol::DOM::Node>* root) override;
  protocol::Response getFlattenedDocument(
      protocol::Maybe<int> depth,
      protocol::Maybe<bool> pierce,
      std::unique_ptr<protocol::Array<protocol::DOM::Node>>* nodes) override;
  protocol::Response collectClassNamesFromSubtree(
      int node_id,
      std::unique_ptr<protocol::Array<String>>* class_names) override;
  protocol::Response requestChildNodes(
      int node_id,
      protocol::Maybe<int> depth,
      protocol::Maybe<bool> traverse_frames) override;
  protocol::Response querySelector(int node_id,
                                   const String& selector,
                                   int* out_node_id) override;
  protocol::Response querySelectorAll(
      int node_id,
      const String& selector,
      std::unique_ptr<protocol::Array<int>>* node_ids) override;
  protocol::Response setNodeName(int node_id,
                                 const String& name,
                                 int* out_node_id) override;
  protocol::Response setNodeValue(int node_id, const String& value) override;
  protocol::Response removeNode(int node_id) override;
  protocol::Response setAttributeValue(int node_id,
                                       const String& name,
                                       const String& value) override;
  protocol::Response setAttributesAsText(int node_id,
                                         const String& text,
                                         protocol::Maybe<String> name) override;
  protocol::Response removeAttribute(int node_id, const String& name) override;
  protocol::Response getOuterHTML(protocol::Maybe<int> node_id,
                                  protocol::Maybe<int> backend_node_id,
                                  protocol::Maybe<String> object_id,
                                  String* outer_html) override;
  protocol::Response setOuterHTML(int node_id,
                                  const String& outer_html) override;
  protocol::Response performSearch(
      const String& query,
      protocol::Maybe<bool> include_user_agent_shadow_dom,
      String* search_id,
      int* result_count) override;
  protocol::Response getSearchResults(
      const String& search_id,
      int from_index,
      int to_index,
      std::unique_ptr<protocol::Array<int>>* node_ids) override;
  protocol::Response discardSearchResults(const String& search_id) override;
  protocol::Response requestNode(const String& object_id,
                                 int* out_node_id) override;
  protocol::Response pushNodeByPathToFrontend(const String& path,
                                              int* out_node_id) override;
  protocol::Response pushNodesByBackendIdsToFrontend(
      std::unique_ptr<protocol::Array<int>> backend_node_ids,
      std::unique_ptr<protocol::Array<int>>* node_ids) override;
  protocol::Response setInspectedNode(int node_id) override;
  protocol::Response resolveNode(
      protocol::Maybe<int> node_id,
      protocol::Maybe<int> backend_node_id,
      protocol::Maybe<String> object_group,
      protocol::Maybe<int> execution_context_id,
      std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>*)
      override;
  protocol::Response getAttributes(
      int node_id,
      std::unique_ptr<protocol::Array<String>>* attributes) override;
  protocol::Response copyTo(int node_id,
                            int target_node_id,
                            protocol::Maybe<int> insert_before_node_id,
                            int* out_node_id) override;
  protocol::Response moveTo(int node_id,
                            int target_node_id,
                            protocol::Maybe<int> insert_before_node_id,
                            int* out_node_id) override;
  protocol::Response undo() override;
  protocol::Response redo() override;
  protocol::Response markUndoableState() override;
  protocol::Response focus(protocol::Maybe<int> node_id,
                           protocol::Maybe<int> backend_node_id,
                           protocol::Maybe<String> object_id) override;
  protocol::Response setFileInputFiles(
      std::unique_ptr<protocol::Array<String>> files,
      protocol::Maybe<int> node_id,
      protocol::Maybe<int> backend_node_id,
      protocol::Maybe<String> object_id) override;
  protocol::Response setNodeStackTracesEnabled(bool enable) override;
  protocol::Response getNodeStackTraces(
      int node_id,
      protocol::Maybe<v8_inspector::protocol::Runtime::API::StackTrace>*
          creation) override;
  protocol::Response getBoxModel(
      protocol::Maybe<int> node_id,
      protocol::Maybe<int> backend_node_id,
      protocol::Maybe<String> object_id,
      std::unique_ptr<protocol::DOM::BoxModel>*) override;
  protocol::Response getContentQuads(
      protocol::Maybe<int> node_id,
      protocol::Maybe<int> backend_node_id,
      protocol::Maybe<String> object_id,
      std::unique_ptr<protocol::Array<protocol::Array<double>>>* quads)
      override;
  protocol::Response getNodeForLocation(
      int x,
      int y,
      protocol::Maybe<bool> include_user_agent_shadow_dom,
      protocol::Maybe<bool> ignore_pointer_events_none,
      int* backend_node_id,
      String* frame_id,
      protocol::Maybe<int>* node_id) override;
  protocol::Response getRelayoutBoundary(int node_id,
                                         int* out_node_id) override;
  protocol::Response describeNode(
      protocol::Maybe<int> node_id,
      protocol::Maybe<int> backend_node_id,
      protocol::Maybe<String> object_id,
      protocol::Maybe<int> depth,
      protocol::Maybe<bool> pierce,
      std::unique_ptr<protocol::DOM::Node>*) override;

  protocol::Response getFrameOwner(const String& frame_id,
                                   int* backend_node_id,
                                   protocol::Maybe<int>* node_id) override;

  protocol::Response getFileInfo(const String& object_id,
                                 String* path) override;

  bool Enabled() const;
  void ReleaseDanglingNodes();

  // Methods called from the InspectorInstrumentation.
  void DomContentLoadedEventFired(LocalFrame*);
  void DidCommitLoad(LocalFrame*, DocumentLoader*);
  void DidInsertDOMNode(Node*);
  void WillRemoveDOMNode(Node*);
  void WillModifyDOMAttr(Element*,
                         const AtomicString& old_value,
                         const AtomicString& new_value);
  void DidModifyDOMAttr(Element*,
                        const QualifiedName&,
                        const AtomicString& value);
  void DidRemoveDOMAttr(Element*, const QualifiedName&);
  void StyleAttributeInvalidated(const HeapVector<Member<Element>>& elements);
  void CharacterDataModified(CharacterData*);
  void DidInvalidateStyleAttr(Node*);
  void DidPushShadowRoot(Element* host, ShadowRoot*);
  void WillPopShadowRoot(Element* host, ShadowRoot*);
  void DidPerformElementShadowDistribution(Element*);
  void DidPerformSlotDistribution(HTMLSlotElement*);
  void FrameDocumentUpdated(LocalFrame*);
  void FrameOwnerContentUpdated(LocalFrame*, HTMLFrameOwnerElement*);
  void PseudoElementCreated(PseudoElement*);
  void PseudoElementDestroyed(PseudoElement*);
  void NodeCreated(Node* node);
  void PortalRemoteFrameCreated(HTMLPortalElement*);

  Node* NodeForId(int node_id);
  int BoundNodeId(Node*);
  void SetDOMListener(DOMListener*);
  int PushNodePathToFrontend(Node*);
  protocol::Response NodeForRemoteObjectId(const String& remote_object_id,
                                           Node*&);

  static String DocumentURLString(Document*);
  static String DocumentBaseURLString(Document*);

  InspectorHistory* History() { return history_.Get(); }

  // We represent embedded doms as a part of the same hierarchy. Hence we treat
  // children of frame owners differently.  We also skip whitespace text nodes
  // conditionally. Following methods encapsulate these specifics.
  static Node* InnerFirstChild(Node*);
  static Node* InnerNextSibling(Node*);
  static Node* InnerPreviousSibling(Node*);
  static unsigned InnerChildNodeCount(Node*);
  static Node* InnerParentNode(Node*);
  static bool IsWhitespace(Node*);
  static void CollectNodes(Node* root,
                           int depth,
                           bool pierce,
                           base::RepeatingCallback<bool(Node*)>,
                           HeapVector<Member<Node>>* result);

  protocol::Response AssertNode(int node_id, Node*&);
  protocol::Response AssertNode(const protocol::Maybe<int>& node_id,
                                const protocol::Maybe<int>& backend_node_id,
                                const protocol::Maybe<String>& object_id,
                                Node*&);
  protocol::Response AssertElement(int node_id, Element*&);
  Document* GetDocument() const { return document_.Get(); }

 private:
  void SetDocument(Document*);
  // Unconditionally enables the agent, even if |enabled_.Get()==true|.
  // For idempotence, call enable().
  void EnableAndReset();

  // Node-related methods.
  typedef HeapHashMap<Member<Node>, int> NodeToIdMap;
  int Bind(Node*, NodeToIdMap*);
  void Unbind(Node*, NodeToIdMap*);

  protocol::Response AssertEditableNode(int node_id, Node*&);
  protocol::Response AssertEditableChildNode(Element* parent_element,
                                             int node_id,
                                             Node*&);
  protocol::Response AssertEditableElement(int node_id, Element*&);

  int PushNodePathToFrontend(Node*, NodeToIdMap* node_map);
  void PushChildNodesToFrontend(int node_id,
                                int depth = 1,
                                bool traverse_frames = false);
  void DOMNodeRemoved(Node*);

  void InvalidateFrameOwnerElement(HTMLFrameOwnerElement*);

  std::unique_ptr<protocol::DOM::Node> BuildObjectForNode(
      Node*,
      int depth,
      bool traverse_frames,
      NodeToIdMap*,
      protocol::Array<protocol::DOM::Node>* flatten_result = nullptr);
  std::unique_ptr<protocol::Array<String>> BuildArrayForElementAttributes(
      Element*);
  std::unique_ptr<protocol::Array<protocol::DOM::Node>>
  BuildArrayForContainerChildren(
      Node* container,
      int depth,
      bool traverse_frames,
      NodeToIdMap* nodes_map,
      protocol::Array<protocol::DOM::Node>* flatten_result);
  std::unique_ptr<protocol::Array<protocol::DOM::Node>>
  BuildArrayForPseudoElements(Element*, NodeToIdMap* nodes_map);
  std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
  BuildArrayForDistributedNodes(V0InsertionPoint*);
  std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
  BuildDistributedNodesForSlot(HTMLSlotElement*);

  Node* NodeForPath(const String& path);

  void DiscardFrontendBindings();

  InspectorRevalidateDOMTask* RevalidateTask();

  v8::Isolate* isolate_;
  Member<InspectedFrames> inspected_frames_;
  v8_inspector::V8InspectorSession* v8_session_;
  Member<DOMListener> dom_listener_;
  Member<NodeToIdMap> document_node_to_id_map_;
  // Owns node mappings for dangling nodes.
  HeapVector<Member<NodeToIdMap>> dangling_node_to_id_maps_;
  HeapHashMap<int, Member<Node>> id_to_node_;
  HeapHashMap<int, Member<NodeToIdMap>> id_to_nodes_map_;
  HeapHashMap<WeakMember<Node>, Member<InspectorSourceLocation>>
      node_to_creation_source_location_map_;
  HashSet<int> children_requested_;
  HashSet<int> distributed_nodes_requested_;
  HashMap<int, int> cached_child_count_;
  int last_node_id_;
  Member<Document> document_;
  typedef HeapHashMap<String, HeapVector<Member<Node>>> SearchResults;
  SearchResults search_results_;
  Member<InspectorRevalidateDOMTask> revalidate_task_;
  Member<InspectorHistory> history_;
  Member<DOMEditor> dom_editor_;
  bool suppress_attribute_modified_event_;
  InspectorAgentState::Boolean enabled_;
  InspectorAgentState::Boolean capture_node_stack_traces_;
  DISALLOW_COPY_AND_ASSIGN(InspectorDOMAgent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_DOM_AGENT_H_
