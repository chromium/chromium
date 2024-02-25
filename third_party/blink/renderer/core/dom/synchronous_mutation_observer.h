// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SYNCHRONOUS_MUTATION_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SYNCHRONOUS_MUTATION_OBSERVER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CharacterData;
class ContainerNode;
class Document;
class Element;
class Node;
class NodeWithIndex;
class Text;

// This class is a base class for classes which observe DOM tree mutation
// synchronously. If you want to observe DOM tree mutation asynchronously see
// MutationObserver Web API.
// Note: if you only need to observe Document shutdown,
// ExecutionContextLifecycleObserver::ContextDestroyed provides this same
// functionality more efficiently (since it doesn't observe the other events).
//
// TODO(yosin): Following classes should be derived from this class to
// simplify Document class.
//  - DragCaret
//  - DocumentMarkerController
//  - EventHandler
//  - FrameCaret
//  - InputMethodController
//  - SelectionController
//  - Range set
//  - NodeIterator set
class CORE_EXPORT SynchronousMutationObserver : public GarbageCollectedMixin {
 public:
  // TODO(yosin): We will have following member functions:
  //  - dataWillBeChanged(const CharacterData&);
  //  - didMoveTreeToNewDocument(const Node& root);
  //  - didInsertText(Node*, unsigned offset, unsigned length);
  //  - didRemoveText(Node*, unsigned offset, unsigned length);

  // Called after child nodes changed.
  virtual void DidChangeChildren(const ContainerNode&,
                                 const ContainerNode::ChildrenChange&) {}

  // Called after attribute changes.
  virtual void AttributeChanged(const Element& element,
                                const QualifiedName& name,
                                const AtomicString& old_value,
                                const AtomicString& new_value) {}

  // Called after characters in |nodeToBeRemoved| is appended into |mergedNode|.
  // |oldLength| holds length of |mergedNode| before merge.
  virtual void DidMergeTextNodes(
      const Text& merged_node,
      const NodeWithIndex& node_to_be_removed_with_index,
      unsigned old_length) {}

  // Called just after node tree |root| is moved to new document.
  virtual void DidMoveTreeToNewDocument(const Node& root) {}

  // Called when |Text| node is split, next sibling |oldNode| contains
  // characters after split point.
  virtual void DidSplitTextNode(const Text& old_node) {}

  // Called when |CharacterData| is updated at |offset|, |oldLength| is a
  // number of deleted character and |newLength| is a number of added
  // characters.
  virtual void DidUpdateCharacterData(CharacterData*,
                                      unsigned offset,
                                      unsigned old_length,
                                      unsigned new_length) {}

  // Called before removing container node.
  virtual void NodeChildrenWillBeRemoved(ContainerNode&) {}

  // Called before removing node.
  virtual void NodeWillBeRemoved(Node&) {}

  // Called when detaching document.
  virtual void ContextDestroyed() {}

  // Call before clearing an observer list.
  void ObserverSetWillBeCleared();

  Document* GetDocument() const { return document_.Get(); }
  void SetDocument(Document*);

  void Trace(Visitor*) const override;

 protected:
  SynchronousMutationObserver() = default;

 private:
  WeakMember<Document> document_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SYNCHRONOUS_MUTATION_OBSERVER_H_
