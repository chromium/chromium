/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/inspector/dom_patch_support.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/context_features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/inspector/dom_editor.h"
#include "third_party/blink/renderer/core/inspector/inspector_history.h"
#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

DOMPatchSupport::DOMPatchSupport(DOMEditor* dom_editor, Document& document)
    : dom_editor_(dom_editor), document_(document) {}

void DOMPatchSupport::PatchDocument(const String& markup) {
  Document* new_document = nullptr;
  DocumentInit init = DocumentInit::Create();
  if (GetDocument().IsHTMLDocument())
    new_document = MakeGarbageCollected<HTMLDocument>(init);
  else if (GetDocument().IsSVGDocument())
    new_document = XMLDocument::CreateSVG(init);
  else if (GetDocument().IsXHTMLDocument())
    new_document = XMLDocument::CreateXHTML(init);
  else if (GetDocument().IsXMLDocument())
    new_document = MakeGarbageCollected<XMLDocument>(init);

  DCHECK(new_document);
  new_document->SetContextFeatures(GetDocument().GetContextFeatures());
  if (!GetDocument().IsHTMLDocument()) {
    DocumentParser* parser =
        MakeGarbageCollected<XMLDocumentParser>(*new_document, nullptr);
    parser->Append(markup);
    parser->Finish();
    parser->Detach();

    // Avoid breakage on non-well-formed documents.
    if (!static_cast<XMLDocumentParser*>(parser)->WellFormed())
      return;
  }
  new_document->SetContent(markup);
  Digest* old_info = CreateDigest(GetDocument().documentElement(), nullptr);
  Digest* new_info =
      CreateDigest(new_document->documentElement(), &unused_nodes_map_);

  if (!InnerPatchNode(old_info, new_info, IGNORE_EXCEPTION_FOR_TESTING)) {
    // Fall back to rewrite.
    GetDocument().write(markup);
    GetDocument().close();
  }
}

Node* DOMPatchSupport::PatchNode(Node* node,
                                 const String& markup,
                                 ExceptionState& exception_state) {
  // Don't parse <html> as a fragment.
  if (node->IsDocumentNode() ||
      (node->parentNode() && node->parentNode()->IsDocumentNode())) {
    PatchDocument(markup);
    return nullptr;
  }

  Node* previous_sibling = node->previousSibling();
  DocumentFragment* fragment = DocumentFragment::Create(GetDocument());
  Node* target_node = node->ParentElementOrShadowRoot()
                          ? node->ParentElementOrShadowRoot()
                          : GetDocument().documentElement();

  // Use the document BODY as the context element when editing immediate shadow
  // root children, as it provides an equivalent parsing context.
  if (target_node->IsShadowRoot())
    target_node = GetDocument().body();
  auto* target_element = To<Element>(target_node);

  // FIXME: This code should use one of createFragment* in Serialization.h
  if (GetDocument().IsHTMLDocument())
    fragment->ParseHTML(markup, target_element);
  else
    fragment->ParseXML(markup, target_element);

  // Compose the old list.
  ContainerNode* parent_node = node->parentNode();
  HeapVector<Member<Digest>> old_list;
  for (Node* child = parent_node->firstChild(); child;
       child = child->nextSibling())
    old_list.push_back(CreateDigest(child, nullptr));

  // Compose the new list.
  String markup_copy = markup.DeprecatedLower();
  HeapVector<Member<Digest>> new_list;
  for (Node* child = parent_node->firstChild(); child != node;
       child = child->nextSibling())
    new_list.push_back(CreateDigest(child, nullptr));
  for (Node* child = fragment->firstChild(); child;
       child = child->nextSibling()) {
    if (IsA<HTMLHeadElement>(*child) && !child->hasChildren() &&
        markup_copy.Find("</head>") == kNotFound) {
      // HTML5 parser inserts empty <head> tag whenever it parses <body>
      continue;
    }
    if (IsA<HTMLBodyElement>(*child) && !child->hasChildren() &&
        markup_copy.Find("</body>") == kNotFound) {
      // HTML5 parser inserts empty <body> tag whenever it parses </head>
      continue;
    }
    new_list.push_back(CreateDigest(child, &unused_nodes_map_));
  }
  for (Node* child = node->nextSibling(); child; child = child->nextSibling())
    new_list.push_back(CreateDigest(child, nullptr));

  if (!InnerPatchChildren(parent_node, old_list, new_list, exception_state)) {
    // Fall back to total replace.
    if (!dom_editor_->ReplaceChild(parent_node, fragment, node,
                                   exception_state))
      return nullptr;
  }
  return previous_sibling ? previous_sibling->nextSibling()
                          : parent_node->firstChild();
}

bool DOMPatchSupport::InnerPatchNode(Digest* old_digest,
                                     Digest* new_digest,
                                     ExceptionState& exception_state) {
  if (old_digest->sha1_ == new_digest->sha1_)
    return true;

  Node* old_node = old_digest->node_;
  Node* new_node = new_digest->node_;

  if (new_node->getNodeType() != old_node->getNodeType() ||
      new_node->nodeName() != old_node->nodeName())
    return dom_editor_->ReplaceChild(old_node->parentNode(), new_node, old_node,
                                     exception_state);

  if (old_node->nodeValue() != new_node->nodeValue()) {
    if (!dom_editor_->SetNodeValue(old_node, new_node->nodeValue(),
                                   exception_state))
      return false;
  }

  auto* old_element = DynamicTo<Element>(old_node);
  if (!old_element)
    return true;

  // Patch attributes
  auto* new_element = To<Element>(new_node);
  if (old_digest->attrs_sha1_ != new_digest->attrs_sha1_) {
    // FIXME: Create a function in Element for removing all properties. Take in
    // account whether did/willModifyAttribute are important.
    while (old_element->AttributesWithoutUpdate().size()) {
      const Attribute& attribute = old_element->AttributesWithoutUpdate().at(0);
      if (!dom_editor_->RemoveAttribute(
              old_element, attribute.GetName().ToString(), exception_state))
        return false;
    }

    // FIXME: Create a function in Element for copying properties.
    // cloneDataFromElement() is close but not enough for this case.
    for (auto& attribute : new_element->AttributesWithoutUpdate()) {
      if (!dom_editor_->SetAttribute(old_element,
                                     attribute.GetName().ToString(),
                                     attribute.Value(), exception_state))
        return false;
    }
  }

  bool result = InnerPatchChildren(old_element, old_digest->children_,
                                   new_digest->children_, exception_state);
  unused_nodes_map_.erase(new_digest->sha1_);
  return result;
}

std::pair<DOMPatchSupport::ResultMap, DOMPatchSupport::ResultMap>
DOMPatchSupport::Diff(const HeapVector<Member<Digest>>& old_list,
                      const HeapVector<Member<Digest>>& new_list) {
  ResultMap new_map(new_list.size());
  ResultMap old_map(old_list.size());

  for (wtf_size_t i = 0; i < old_map.size(); ++i) {
    old_map[i].first = nullptr;
    old_map[i].second = 0;
  }

  for (wtf_size_t i = 0; i < new_map.size(); ++i) {
    new_map[i].first = nullptr;
    new_map[i].second = 0;
  }

  // Trim head and tail.
  for (wtf_size_t i = 0; i < old_list.size() && i < new_list.size() &&
                         old_list[i]->sha1_ == new_list[i]->sha1_;
       ++i) {
    old_map[i].first = old_list[i].Get();
    old_map[i].second = i;
    new_map[i].first = new_list[i].Get();
    new_map[i].second = i;
  }
  for (wtf_size_t i = 0; i < old_list.size() && i < new_list.size() &&
                         old_list[old_list.size() - i - 1]->sha1_ ==
                             new_list[new_list.size() - i - 1]->sha1_;
       ++i) {
    wtf_size_t old_index = old_list.size() - i - 1;
    wtf_size_t new_index = new_list.size() - i - 1;
    old_map[old_index].first = old_list[old_index].Get();
    old_map[old_index].second = new_index;
    new_map[new_index].first = new_list[new_index].Get();
    new_map[new_index].second = old_index;
  }

  typedef HashMap<String, Vector<wtf_size_t>> DiffTable;
  DiffTable new_table;
  DiffTable old_table;

  for (wtf_size_t i = 0; i < new_list.size(); ++i) {
    new_table.insert(new_list[i]->sha1_, Vector<wtf_size_t>())
        .stored_value->value.push_back(i);
  }

  for (wtf_size_t i = 0; i < old_list.size(); ++i) {
    old_table.insert(old_list[i]->sha1_, Vector<wtf_size_t>())
        .stored_value->value.push_back(i);
  }

  for (auto& new_it : new_table) {
    if (new_it.value.size() != 1)
      continue;

    DiffTable::iterator old_it = old_table.find(new_it.key);
    if (old_it == old_table.end() || old_it->value.size() != 1)
      continue;

    new_map[new_it.value[0]] =
        std::make_pair(new_list[new_it.value[0]].Get(), old_it->value[0]);
    old_map[old_it->value[0]] =
        std::make_pair(old_list[old_it->value[0]].Get(), new_it.value[0]);
  }

  for (wtf_size_t i = 0; new_list.size() > 0 && i < new_list.size() - 1; ++i) {
    if (!new_map[i].first || new_map[i + 1].first)
      continue;

    wtf_size_t j = new_map[i].second + 1;
    if (j < old_map.size() && !old_map[j].first &&
        new_list[i + 1]->sha1_ == old_list[j]->sha1_) {
      new_map[i + 1] = std::make_pair(new_list[i + 1].Get(), j);
      old_map[j] = std::make_pair(old_list[j].Get(), i + 1);
    }
  }

  for (wtf_size_t i = new_list.size() - 1; new_list.size() > 0 && i > 0; --i) {
    if (!new_map[i].first || new_map[i - 1].first || new_map[i].second <= 0)
      continue;

    wtf_size_t j = new_map[i].second - 1;
    if (!old_map[j].first && new_list[i - 1]->sha1_ == old_list[j]->sha1_) {
      new_map[i - 1] = std::make_pair(new_list[i - 1].Get(), j);
      old_map[j] = std::make_pair(old_list[j].Get(), i - 1);
    }
  }

  return std::make_pair(old_map, new_map);
}

bool DOMPatchSupport::InnerPatchChildren(
    ContainerNode* parent_node,
    const HeapVector<Member<Digest>>& old_list,
    const HeapVector<Member<Digest>>& new_list,
    ExceptionState& exception_state) {
  std::pair<ResultMap, ResultMap> result_maps = Diff(old_list, new_list);
  ResultMap& old_map = result_maps.first;
  ResultMap& new_map = result_maps.second;

  Digest* old_head = nullptr;
  Digest* old_body = nullptr;

  // 1. First strip everything except for the nodes that retain. Collect pending
  // merges.
  HeapHashMap<Member<Digest>, Member<Digest>> merges;
  HashSet<wtf_size_t, WTF::IntHash<wtf_size_t>,
          WTF::UnsignedWithZeroKeyHashTraits<wtf_size_t>>
      used_new_ordinals;
  for (wtf_size_t i = 0; i < old_list.size(); ++i) {
    if (old_map[i].first) {
      if (used_new_ordinals.insert(old_map[i].second).is_new_entry)
        continue;
      old_map[i].first = nullptr;
      old_map[i].second = 0;
    }

    // Always match <head> and <body> tags with each other - we can't remove
    // them from the DOM upon patching.
    if (IsA<HTMLHeadElement>(*old_list[i]->node_)) {
      old_head = old_list[i].Get();
      continue;
    }
    if (IsA<HTMLBodyElement>(*old_list[i]->node_)) {
      old_body = old_list[i].Get();
      continue;
    }

    // Check if this change is between stable nodes. If it is, consider it as
    // "modified".
    if (!unused_nodes_map_.Contains(old_list[i]->sha1_) &&
        (!i || old_map[i - 1].first) &&
        (i == old_map.size() - 1 || old_map[i + 1].first)) {
      wtf_size_t anchor_candidate = i ? old_map[i - 1].second + 1 : 0;
      wtf_size_t anchor_after = (i == old_map.size() - 1)
                                    ? anchor_candidate + 1
                                    : old_map[i + 1].second;
      if (anchor_after - anchor_candidate == 1 &&
          anchor_candidate < new_list.size())
        merges.Set(new_list[anchor_candidate].Get(), old_list[i].Get());
      else {
        if (!RemoveChildAndMoveToNew(old_list[i].Get(), exception_state))
          return false;
      }
    } else {
      if (!RemoveChildAndMoveToNew(old_list[i].Get(), exception_state))
        return false;
    }
  }

  // Mark retained nodes as used, do not reuse node more than once.
  HashSet<wtf_size_t, WTF::IntHash<wtf_size_t>,
          WTF::UnsignedWithZeroKeyHashTraits<wtf_size_t>>
      used_old_ordinals;
  for (wtf_size_t i = 0; i < new_list.size(); ++i) {
    if (!new_map[i].first)
      continue;
    wtf_size_t old_ordinal = new_map[i].second;
    if (used_old_ordinals.Contains(old_ordinal)) {
      // Do not map node more than once
      new_map[i].first = nullptr;
      new_map[i].second = 0;
      continue;
    }
    used_old_ordinals.insert(old_ordinal);
    MarkNodeAsUsed(new_map[i].first);
  }

  // Mark <head> and <body> nodes for merge.
  if (old_head || old_body) {
    for (wtf_size_t i = 0; i < new_list.size(); ++i) {
      if (old_head && IsA<HTMLHeadElement>(*new_list[i]->node_))
        merges.Set(new_list[i].Get(), old_head);
      if (old_body && IsA<HTMLBodyElement>(*new_list[i]->node_))
        merges.Set(new_list[i].Get(), old_body);
    }
  }

  // 2. Patch nodes marked for merge.
  for (auto& merge : merges) {
    if (!InnerPatchNode(merge.value, merge.key, exception_state))
      return false;
  }

  // 3. Insert missing nodes.
  for (wtf_size_t i = 0; i < new_map.size(); ++i) {
    if (new_map[i].first || merges.Contains(new_list[i].Get()))
      continue;
    if (!InsertBeforeAndMarkAsUsed(parent_node, new_list[i].Get(),
                                   NodeTraversal::ChildAt(*parent_node, i),
                                   exception_state))
      return false;
  }

  // 4. Then put all nodes that retained into their slots (sort by new index).
  for (wtf_size_t i = 0; i < old_map.size(); ++i) {
    if (!old_map[i].first)
      continue;
    Node* node = old_map[i].first->node_;
    Node* anchor_node = NodeTraversal::ChildAt(*parent_node, old_map[i].second);
    if (node == anchor_node)
      continue;
    if (IsA<HTMLBodyElement>(*node) || IsA<HTMLHeadElement>(*node)) {
      // Never move head or body, move the rest of the nodes around them.
      continue;
    }

    if (!dom_editor_->InsertBefore(parent_node, node, anchor_node,
                                   exception_state))
      return false;
  }
  return true;
}

DOMPatchSupport::Digest* DOMPatchSupport::CreateDigest(
    Node* node,
    UnusedNodesMap* unused_nodes_map) {
  Digest* digest = MakeGarbageCollected<Digest>(node);
  Digestor digestor(kHashAlgorithmSha1);
  DigestValue digest_result;

  Node::NodeType node_type = node->getNodeType();
  digestor.Update(
      {reinterpret_cast<const uint8_t*>(&node_type), sizeof(node_type)});
  digestor.UpdateUtf8(node->nodeName());
  digestor.UpdateUtf8(node->nodeValue());

  if (auto* element = DynamicTo<Element>(node)) {
    Node* child = element->firstChild();
    while (child) {
      Digest* child_info = CreateDigest(child, unused_nodes_map);
      digestor.UpdateUtf8(child_info->sha1_);
      child = child->nextSibling();
      digest->children_.push_back(child_info);
    }

    AttributeCollection attributes = element->AttributesWithoutUpdate();
    if (!attributes.IsEmpty()) {
      Digestor attrs_digestor(kHashAlgorithmSha1);
      for (auto& attribute : attributes) {
        attrs_digestor.UpdateUtf8(attribute.GetName().ToString());
        attrs_digestor.UpdateUtf8(attribute.Value().GetString());
      }

      attrs_digestor.Finish(digest_result);
      DCHECK(!attrs_digestor.has_failed());
      digest->attrs_sha1_ =
          Base64Encode(base::make_span(digest_result).first<10>());
      digestor.UpdateUtf8(digest->attrs_sha1_);
    }
  }

  digestor.Finish(digest_result);
  DCHECK(!digestor.has_failed());
  digest->sha1_ = Base64Encode(base::make_span(digest_result).first<10>());

  if (unused_nodes_map)
    unused_nodes_map->insert(digest->sha1_, digest);
  return digest;
}

bool DOMPatchSupport::InsertBeforeAndMarkAsUsed(
    ContainerNode* parent_node,
    Digest* digest,
    Node* anchor,
    ExceptionState& exception_state) {
  bool result = dom_editor_->InsertBefore(parent_node, digest->node_, anchor,
                                          exception_state);
  MarkNodeAsUsed(digest);
  return result;
}

bool DOMPatchSupport::RemoveChildAndMoveToNew(Digest* old_digest,
                                              ExceptionState& exception_state) {
  Node* old_node = old_digest->node_;
  if (!dom_editor_->RemoveChild(old_node->parentNode(), old_node,
                                exception_state))
    return false;

  // Diff works within levels. In order not to lose the node identity when user
  // prepends their HTML with "<div>" (i.e. all nodes are shifted to the next
  // nested level), prior to dropping the original node on the floor, check
  // whether new DOM has a digest with matching sha1. If it does, replace it
  // with the original DOM chunk.  Chances are high that it will get merged back
  // into the original DOM during the further patching.
  UnusedNodesMap::iterator it = unused_nodes_map_.find(old_digest->sha1_);
  if (it != unused_nodes_map_.end()) {
    Digest* new_digest = it->value;
    Node* new_node = new_digest->node_;
    if (!dom_editor_->ReplaceChild(new_node->parentNode(), old_node, new_node,
                                   exception_state))
      return false;
    new_digest->node_ = old_node;
    MarkNodeAsUsed(new_digest);
    return true;
  }

  for (wtf_size_t i = 0; i < old_digest->children_.size(); ++i) {
    if (!RemoveChildAndMoveToNew(old_digest->children_[i].Get(),
                                 exception_state))
      return false;
  }
  return true;
}

void DOMPatchSupport::MarkNodeAsUsed(Digest* digest) {
  HeapDeque<Member<Digest>> queue;
  queue.push_back(digest);
  while (!queue.IsEmpty()) {
    Digest* first = queue.TakeFirst();
    unused_nodes_map_.erase(first->sha1_);
    for (wtf_size_t i = 0; i < first->children_.size(); ++i)
      queue.push_back(first->children_[i].Get());
  }
}

void DOMPatchSupport::Digest::Trace(blink::Visitor* visitor) {
  visitor->Trace(node_);
  visitor->Trace(children_);
}

}  // namespace blink
