/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/xml/xpath_step.h"

#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/xml/xpath_parser.h"
#include "third_party/blink/renderer/core/xml/xpath_util.h"
#include "third_party/blink/renderer/core/xmlns_names.h"

namespace blink {
namespace xpath {

Step::Step(Axis axis, const NodeTest& node_test)
    : axis_(axis), node_test_(MakeGarbageCollected<NodeTest>(node_test)) {}

Step::Step(Axis axis,
           const NodeTest& node_test,
           HeapVector<Member<Predicate>>& predicates)
    : axis_(axis), node_test_(MakeGarbageCollected<NodeTest>(node_test)) {
  predicates_.swap(predicates);
}

Step::~Step() = default;

void Step::Trace(blink::Visitor* visitor) {
  visitor->Trace(node_test_);
  visitor->Trace(predicates_);
  ParseNode::Trace(visitor);
}

void Step::Optimize() {
  // Evaluate predicates as part of node test if possible to avoid building
  // unnecessary NodeSets.
  // E.g., there is no need to build a set of all "foo" nodes to evaluate
  // "foo[@bar]", we can check the predicate while enumerating.
  // This optimization can be applied to predicates that are not context node
  // list sensitive, or to first predicate that is only context position
  // sensitive, e.g. foo[position() mod 2 = 0].
  HeapVector<Member<Predicate>> remaining_predicates;
  for (const auto& predicate : predicates_) {
    if ((!predicate->IsContextPositionSensitive() ||
         GetNodeTest().MergedPredicates().IsEmpty()) &&
        !predicate->IsContextSizeSensitive() &&
        remaining_predicates.IsEmpty()) {
      GetNodeTest().MergedPredicates().push_back(predicate);
    } else {
      remaining_predicates.push_back(predicate);
    }
  }
  swap(remaining_predicates, predicates_);
}

bool OptimizeStepPair(Step* first, Step* second) {
  if (first->axis_ == Step::kDescendantOrSelfAxis &&
      first->GetNodeTest().GetKind() == Step::NodeTest::kAnyNodeTest &&
      !first->predicates_.size() &&
      !first->GetNodeTest().MergedPredicates().size()) {
    DCHECK(first->GetNodeTest().Data().IsEmpty());
    DCHECK(first->GetNodeTest().NamespaceURI().IsEmpty());

    // Optimize the common case of "//" AKA
    // /descendant-or-self::node()/child::NodeTest to /descendant::NodeTest.
    if (second->axis_ == Step::kChildAxis &&
        second->PredicatesAreContextListInsensitive()) {
      first->axis_ = Step::kDescendantAxis;
      first->GetNodeTest() = Step::NodeTest(
          second->GetNodeTest().GetKind(), second->GetNodeTest().Data(),
          second->GetNodeTest().NamespaceURI());
      swap(second->GetNodeTest().MergedPredicates(),
           first->GetNodeTest().MergedPredicates());
      swap(second->predicates_, first->predicates_);
      first->Optimize();
      return true;
    }
  }
  return false;
}

bool Step::PredicatesAreContextListInsensitive() const {
  for (const auto& predicate : predicates_) {
    if (predicate->IsContextPositionSensitive() ||
        predicate->IsContextSizeSensitive())
      return false;
  }

  for (const auto& predicate : GetNodeTest().MergedPredicates()) {
    if (predicate->IsContextPositionSensitive() ||
        predicate->IsContextSizeSensitive())
      return false;
  }

  return true;
}

void Step::Evaluate(EvaluationContext& evaluation_context,
                    Node* context,
                    NodeSet& nodes) const {
  evaluation_context.position = 0;

  NodesInAxis(evaluation_context, context, nodes);

  // Check predicates that couldn't be merged into node test.
  for (const auto& predicate : predicates_) {
    NodeSet* new_nodes = NodeSet::Create();
    if (!nodes.IsSorted())
      new_nodes->MarkSorted(false);

    for (unsigned j = 0; j < nodes.size(); j++) {
      Node* node = nodes[j];

      evaluation_context.node = node;
      evaluation_context.size = nodes.size();
      evaluation_context.position = j + 1;
      if (predicate->Evaluate(evaluation_context))
        new_nodes->Append(node);
    }

    nodes.Swap(*new_nodes);
  }
}

#if DCHECK_IS_ON()
static inline Node::NodeType PrimaryNodeType(Step::Axis axis) {
  switch (axis) {
    case Step::kAttributeAxis:
      return Node::kAttributeNode;
    default:
      return Node::kElementNode;
  }
}
#endif

// Evaluate NodeTest without considering merged predicates.
static inline bool NodeMatchesBasicTest(Node* node,
                                        Step::Axis axis,
                                        const Step::NodeTest& node_test) {
  switch (node_test.GetKind()) {
    case Step::NodeTest::kTextNodeTest: {
      Node::NodeType type = node->getNodeType();
      return type == Node::kTextNode || type == Node::kCdataSectionNode;
    }
    case Step::NodeTest::kCommentNodeTest:
      return node->getNodeType() == Node::kCommentNode;
    case Step::NodeTest::kProcessingInstructionNodeTest: {
      const AtomicString& name = node_test.Data();
      return node->getNodeType() == Node::kProcessingInstructionNode &&
             (name.IsEmpty() || node->nodeName() == name);
    }
    case Step::NodeTest::kAnyNodeTest:
      return true;
    case Step::NodeTest::kNameTest: {
      const AtomicString& name = node_test.Data();
      const AtomicString& namespace_uri = node_test.NamespaceURI();

      if (axis == Step::kAttributeAxis) {
        auto* attr = To<Attr>(node);

        // In XPath land, namespace nodes are not accessible on the
        // attribute axis.
        if (attr->namespaceURI() == xmlns_names::kNamespaceURI)
          return false;

        if (name == g_star_atom)
          return namespace_uri.IsEmpty() ||
                 attr->namespaceURI() == namespace_uri;

        return attr->localName() == name &&
               attr->namespaceURI() == namespace_uri;
      }

      // Node test on the namespace axis is not implemented yet, the caller
      // has a check for it.
      DCHECK_NE(Step::kNamespaceAxis, axis);

// For other axes, the principal node type is element.
#if DCHECK_IS_ON()
      DCHECK_EQ(Node::kElementNode, PrimaryNodeType(axis));
#endif
      auto* element = DynamicTo<Element>(node);
      if (!element)
        return false;

      if (name == g_star_atom) {
        return namespace_uri.IsEmpty() ||
               namespace_uri == element->namespaceURI();
      }

      if (element->GetDocument().IsHTMLDocument()) {
        if (element->IsHTMLElement()) {
          // Paths without namespaces should match HTML elements in HTML
          // documents despite those having an XHTML namespace. Names are
          // compared case-insensitively.
          return EqualIgnoringASCIICase(element->localName(), name) &&
                 (namespace_uri.IsNull() ||
                  namespace_uri == element->namespaceURI());
        }
        // An expression without any prefix shouldn't match no-namespace
        // nodes (because HTML5 says so).
        return element->HasLocalName(name) &&
               namespace_uri == element->namespaceURI() &&
               !namespace_uri.IsNull();
      }
      return element->HasLocalName(name) &&
             namespace_uri == element->namespaceURI();
    }
  }
  NOTREACHED();
  return false;
}

static inline bool NodeMatches(EvaluationContext& evaluation_context,
                               Node* node,
                               Step::Axis axis,
                               const Step::NodeTest& node_test) {
  if (!NodeMatchesBasicTest(node, axis, node_test))
    return false;

  // Only the first merged predicate may depend on position.
  ++evaluation_context.position;

  for (const auto& predicate : node_test.MergedPredicates()) {
    evaluation_context.node = node;
    // No need to set context size - we only get here when evaluating
    // predicates that do not depend on it.
    if (!predicate->Evaluate(evaluation_context))
      return false;
  }

  return true;
}

// Result nodes are ordered in axis order. Node test (including merged
// predicates) is applied.
void Step::NodesInAxis(EvaluationContext& evaluation_context,
                       Node* context,
                       NodeSet& nodes) const {
  DCHECK(nodes.IsEmpty());
  switch (axis_) {
    case kChildAxis:
      // In XPath model, attribute nodes do not have children.
      if (context->IsAttributeNode())
        return;

      for (Node* n = context->firstChild(); n; n = n->nextSibling()) {
        if (NodeMatches(evaluation_context, n, kChildAxis, GetNodeTest()))
          nodes.Append(n);
      }
      return;

    case kDescendantAxis:
      // In XPath model, attribute nodes do not have children.
      if (context->IsAttributeNode())
        return;

      for (Node& n : NodeTraversal::DescendantsOf(*context)) {
        if (NodeMatches(evaluation_context, &n, kDescendantAxis, GetNodeTest()))
          nodes.Append(&n);
      }
      return;

    case kParentAxis:
      if (auto* attr = DynamicTo<Attr>(context)) {
        Element* n = attr->ownerElement();
        if (NodeMatches(evaluation_context, n, kParentAxis, GetNodeTest()))
          nodes.Append(n);
      } else {
        ContainerNode* n = context->parentNode();
        if (n && NodeMatches(evaluation_context, n, kParentAxis, GetNodeTest()))
          nodes.Append(n);
      }
      return;

    case kAncestorAxis: {
      Node* n = context;
      if (auto* attr = DynamicTo<Attr>(context)) {
        n = attr->ownerElement();
        if (NodeMatches(evaluation_context, n, kAncestorAxis, GetNodeTest()))
          nodes.Append(n);
      }
      for (n = n->parentNode(); n; n = n->parentNode()) {
        if (NodeMatches(evaluation_context, n, kAncestorAxis, GetNodeTest()))
          nodes.Append(n);
      }
      nodes.MarkSorted(false);
      return;
    }

    case kFollowingSiblingAxis:
      if (context->getNodeType() == Node::kAttributeNode)
        return;

      for (Node* n = context->nextSibling(); n; n = n->nextSibling()) {
        if (NodeMatches(evaluation_context, n, kFollowingSiblingAxis,
                        GetNodeTest()))
          nodes.Append(n);
      }
      return;

    case kPrecedingSiblingAxis:
      if (context->getNodeType() == Node::kAttributeNode)
        return;

      for (Node* n = context->previousSibling(); n; n = n->previousSibling()) {
        if (NodeMatches(evaluation_context, n, kPrecedingSiblingAxis,
                        GetNodeTest()))
          nodes.Append(n);
      }
      nodes.MarkSorted(false);
      return;

    case kFollowingAxis:
      if (auto* attr = DynamicTo<Attr>(context)) {
        for (Node& p : NodeTraversal::StartsAfter(*attr->ownerElement())) {
          if (NodeMatches(evaluation_context, &p, kFollowingAxis,
                          GetNodeTest()))
            nodes.Append(&p);
        }
      } else {
        for (Node* p = context; !IsRootDomNode(p); p = p->parentNode()) {
          for (Node* n = p->nextSibling(); n; n = n->nextSibling()) {
            if (NodeMatches(evaluation_context, n, kFollowingAxis,
                            GetNodeTest()))
              nodes.Append(n);
            for (Node& c : NodeTraversal::DescendantsOf(*n)) {
              if (NodeMatches(evaluation_context, &c, kFollowingAxis,
                              GetNodeTest()))
                nodes.Append(&c);
            }
          }
        }
      }
      return;

    case kPrecedingAxis: {
      if (auto* attr = DynamicTo<Attr>(context))
        context = attr->ownerElement();

      Node* n = context;
      while (ContainerNode* parent = n->parentNode()) {
        for (n = NodeTraversal::Previous(*n); n != parent;
             n = NodeTraversal::Previous(*n)) {
          if (NodeMatches(evaluation_context, n, kPrecedingAxis, GetNodeTest()))
            nodes.Append(n);
        }
        n = parent;
      }
      nodes.MarkSorted(false);
      return;
    }

    case kAttributeAxis: {
      auto* context_element = DynamicTo<Element>(context);
      if (!context_element)
        return;

      // Avoid lazily creating attribute nodes for attributes that we do not
      // need anyway.
      if (GetNodeTest().GetKind() == NodeTest::kNameTest &&
          GetNodeTest().Data() != g_star_atom) {
        Attr* attr = context_element->getAttributeNodeNS(
            GetNodeTest().NamespaceURI(), GetNodeTest().Data());
        // In XPath land, namespace nodes are not accessible on the attribute
        // axis.
        if (attr && attr->namespaceURI() != xmlns_names::kNamespaceURI) {
          // Still need to check merged predicates.
          if (NodeMatches(evaluation_context, attr, kAttributeAxis,
                          GetNodeTest()))
            nodes.Append(attr);
        }
        return;
      }

      AttributeCollection attributes = context_element->Attributes();
      for (auto& attribute : attributes) {
        Attr* attr = context_element->EnsureAttr(attribute.GetName());
        if (NodeMatches(evaluation_context, attr, kAttributeAxis,
                        GetNodeTest()))
          nodes.Append(attr);
      }
      return;
    }

    case kNamespaceAxis:
      // XPath namespace nodes are not implemented.
      return;

    case kSelfAxis:
      if (NodeMatches(evaluation_context, context, kSelfAxis, GetNodeTest()))
        nodes.Append(context);
      return;

    case kDescendantOrSelfAxis:
      if (NodeMatches(evaluation_context, context, kDescendantOrSelfAxis,
                      GetNodeTest()))
        nodes.Append(context);
      // In XPath model, attribute nodes do not have children.
      if (context->IsAttributeNode())
        return;

      for (Node& n : NodeTraversal::DescendantsOf(*context)) {
        if (NodeMatches(evaluation_context, &n, kDescendantOrSelfAxis,
                        GetNodeTest()))
          nodes.Append(&n);
      }
      return;

    case kAncestorOrSelfAxis: {
      if (NodeMatches(evaluation_context, context, kAncestorOrSelfAxis,
                      GetNodeTest()))
        nodes.Append(context);
      Node* n = context;
      if (auto* attr = DynamicTo<Attr>(context)) {
        n = attr->ownerElement();
        if (NodeMatches(evaluation_context, n, kAncestorOrSelfAxis,
                        GetNodeTest()))
          nodes.Append(n);
      }
      for (n = n->parentNode(); n; n = n->parentNode()) {
        if (NodeMatches(evaluation_context, n, kAncestorOrSelfAxis,
                        GetNodeTest()))
          nodes.Append(n);
      }
      nodes.MarkSorted(false);
      return;
    }
  }
  NOTREACHED();
}

}  // namespace xpath

}  // namespace blink
