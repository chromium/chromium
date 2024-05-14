// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/testing/selection_sample.h"

#include <algorithm>

#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root_init.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

namespace {

void ConvertTemplatesToShadowRoots(HTMLElement& element) {
  // |element| and descendant elements can have TEMPLATE element with
  // |data-mode="open"|, which is required. Each elemnt can have only one
  // TEMPLATE element.
  HTMLCollection* const templates =
      element.getElementsByTagName(AtomicString("template"));
  HeapVector<Member<Element>> template_vector;
  for (Element* template_element : *templates)
    template_vector.push_back(template_element);
  for (Element* template_element : template_vector) {
    const AtomicString& data_mode =
        template_element->getAttribute(AtomicString("data-mode"));
    DCHECK_EQ(data_mode, "open");

    Element* const parent = template_element->parentElement();
    parent->removeChild(template_element);

    Document* const document = element.ownerDocument();
    ShadowRoot& shadow_root =
        parent->AttachShadowRootForTesting(ShadowRootMode::kOpen);
    Node* const fragment = document->importNode(
        To<HTMLTemplateElement>(template_element)->content(), true,
        ASSERT_NO_EXCEPTION);
    shadow_root.AppendChild(fragment);
  }
}

// Parse selection text notation into Selection object.
class Parser final {
  STACK_ALLOCATED();

 public:
  Parser() = default;
  ~Parser() = default;

  // Set |selection_text| as inner HTML of |element| and returns
  // |SelectionInDOMTree| marked up within |selection_text|.
  SelectionInDOMTree SetSelectionText(HTMLElement* element,
                                      const std::string& selection_text) {
    element->setInnerHTML(String::FromUTF8(selection_text.c_str()));
    element->GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    ConvertTemplatesToShadowRoots(*element);
    Traverse(element);
    if (anchor_node_ && focus_node_) {
      return typename SelectionInDOMTree::Builder()
          .Collapse(Position(anchor_node_, anchor_offset_))
          .Extend(Position(focus_node_, focus_offset_))
          .Build();
    }
    DCHECK(focus_node_) << "Need just '|', or '^' and '|'";
    return typename SelectionInDOMTree::Builder()
        .Collapse(Position(focus_node_, focus_offset_))
        .Build();
  }

 private:
  // Removes selection markers from |node| and records selection markers as
  // |Node| and |offset|. The |node| is removed from container when |node|
  // contains only selection markers.
  void HandleCharacterData(CharacterData* node) {
    int anchor_offset = -1;
    int focus_offset = -1;
    StringBuilder builder;
    for (unsigned i = 0; i < node->length(); ++i) {
      const UChar char_code = node->data()[i];
      if (char_code == '^') {
        DCHECK_EQ(anchor_offset, -1) << node->data();
        anchor_offset = static_cast<int>(builder.length());
        continue;
      }
      if (char_code == '|') {
        DCHECK_EQ(focus_offset, -1) << node->data();
        focus_offset = static_cast<int>(builder.length());
        continue;
      }
      builder.Append(char_code);
    }
    if (anchor_offset == -1 && focus_offset == -1)
      return;
    node->setData(builder.ToString());
    if (node->length() == 0) {
      // Remove |node| if it contains only selection markers.
      ContainerNode* const parent_node = node->parentNode();
      DCHECK(parent_node) << node;
      const int offset_in_parent = node->NodeIndex();
      if (anchor_offset >= 0)
        RecordSelectionAnchor(parent_node, offset_in_parent);
      if (focus_offset >= 0)
        RecordSelectionFocus(parent_node, offset_in_parent);
      parent_node->removeChild(node);
      return;
    }
    if (anchor_offset >= 0)
      RecordSelectionAnchor(node, anchor_offset);
    if (focus_offset >= 0)
      RecordSelectionFocus(node, focus_offset);
  }

  void HandleElementNode(Element* element) {
    if (ShadowRoot* shadow_root = element->GetShadowRoot())
      HandleChildren(shadow_root);
    HandleChildren(element);
  }

  void HandleChildren(ContainerNode* node) {
    Node* runner = node->firstChild();
    while (runner) {
      Node* const next_sibling = runner->nextSibling();
      // |Traverse()| may remove |runner|.
      Traverse(runner);
      runner = next_sibling;
    }
  }

  void RecordSelectionAnchor(Node* node, int offset) {
    DCHECK(!anchor_node_) << "Found more than one '^' in " << *anchor_node_
                          << " and " << *node;
    anchor_node_ = node;
    anchor_offset_ = offset;
  }

  void RecordSelectionFocus(Node* node, int offset) {
    DCHECK(!focus_node_) << "Found more than one '|' in " << *focus_node_
                         << " and " << *node;
    focus_node_ = node;
    focus_offset_ = offset;
  }

  // Traverses descendants of |node|. The |node| may be removed when it is
  // |CharacterData| node contains only selection markers.
  void Traverse(Node* node) {
    if (auto* element = DynamicTo<Element>(node)) {
      HandleElementNode(element);
      return;
    }
    if (auto* data = DynamicTo<CharacterData>(node)) {
      HandleCharacterData(data);
      return;
    }
    NOTREACHED_IN_MIGRATION() << node;
  }

  Node* anchor_node_ = nullptr;
  Node* focus_node_ = nullptr;
  int anchor_offset_ = 0;
  int focus_offset_ = 0;
};

// Serialize DOM/Flat tree to selection text.
template <typename Strategy>
class Serializer final {
  STACK_ALLOCATED();

 public:
  explicit Serializer(const SelectionTemplate<Strategy>& selection)
      : selection_(selection) {}

  std::string Serialize(const ContainerNode& root) {
    SerializeChildren(root);
    return builder_.ToString().Utf8();
  }

 private:
  void HandleCharacterData(const CharacterData& node) {
    const String text = node.data();
    if (selection_.IsNone()) {
      builder_.Append(text);
      return;
    }
    const Node& anchor_node = *selection_.Anchor().ComputeContainerNode();
    const Node& focus_node = *selection_.Focus().ComputeContainerNode();
    const int anchor_offset =
        selection_.Anchor().ComputeOffsetInContainerNode();
    const int focus_offset = selection_.Focus().ComputeOffsetInContainerNode();
    if (anchor_node == node && focus_node == node) {
      if (anchor_offset == focus_offset) {
        builder_.Append(text.Left(anchor_offset));
        builder_.Append('|');
        builder_.Append(text.Substring(anchor_offset));
        return;
      }
      if (anchor_offset < focus_offset) {
        builder_.Append(text.Left(anchor_offset));
        builder_.Append('^');
        builder_.Append(
            text.Substring(anchor_offset, focus_offset - anchor_offset));
        builder_.Append('|');
        builder_.Append(text.Substring(focus_offset));
        return;
      }
      builder_.Append(text.Left(focus_offset));
      builder_.Append('|');
      builder_.Append(
          text.Substring(focus_offset, anchor_offset - focus_offset));
      builder_.Append('^');
      builder_.Append(text.Substring(anchor_offset));
      return;
    }
    if (anchor_node == node) {
      builder_.Append(text.Left(anchor_offset));
      builder_.Append('^');
      builder_.Append(text.Substring(anchor_offset));
      return;
    }
    if (focus_node == node) {
      builder_.Append(text.Left(focus_offset));
      builder_.Append('|');
      builder_.Append(text.Substring(focus_offset));
      return;
    }
    builder_.Append(text);
  }

  void HandleAttribute(const Attribute& attribute) {
    builder_.Append(attribute.GetName().ToString());
    if (attribute.Value().empty())
      return;
    builder_.Append("=\"");
    for (wtf_size_t i = 0; i < attribute.Value().length(); ++i) {
      const UChar char_code = attribute.Value()[i];
      if (char_code == '"') {
        builder_.Append("&quot;");
        continue;
      }
      if (char_code == '&') {
        builder_.Append("&amp;");
        continue;
      }
      builder_.Append(char_code);
    }
    builder_.Append('"');
  }

  void HandleAttributes(const Element& element) {
    Vector<const Attribute*> attributes;
    for (const Attribute& attribute : element.Attributes())
      attributes.push_back(&attribute);
    std::sort(attributes.begin(), attributes.end(),
              [](const Attribute* attribute1, const Attribute* attribute2) {
                return CodeUnitCompareLessThan(
                    attribute1->GetName().ToString(),
                    attribute2->GetName().ToString());
              });
    for (const Attribute* attribute : attributes) {
      builder_.Append(' ');
      HandleAttribute(*attribute);
    }
  }

  void HandleElementNode(const Element& element) {
    builder_.Append('<');
    builder_.Append(element.TagQName().ToString());
    HandleAttributes(element);
    builder_.Append('>');
    if (IsVoidElement(element))
      return;
    SerializeChildren(element);
    builder_.Append("</");
    builder_.Append(element.TagQName().ToString());
    builder_.Append('>');
  }

  void HandleNode(const Node& node) {
    if (auto* element = DynamicTo<Element>(node)) {
      HandleElementNode(*element);
      return;
    }
    if (node.IsTextNode()) {
      HandleCharacterData(To<CharacterData>(node));
      return;
    }
    if (node.getNodeType() == Node::kCommentNode) {
      builder_.Append("<!--");
      HandleCharacterData(To<CharacterData>(node));
      builder_.Append("-->");
      return;
    }
    if (auto* processing_instruction_node =
            DynamicTo<ProcessingInstruction>(node)) {
      builder_.Append("<?");
      builder_.Append(processing_instruction_node->target());
      builder_.Append(' ');
      HandleCharacterData(To<CharacterData>(node));
      builder_.Append("?>");
      return;
    }
    NOTREACHED_IN_MIGRATION() << node;
  }

  void HandleSelection(const ContainerNode& node, int offset) {
    if (selection_.IsNone())
      return;
    const PositionTemplate<Strategy> position(node, offset);
    if (selection_.Focus().ToOffsetInAnchor() == position) {
      builder_.Append('|');
      return;
    }
    if (selection_.Anchor().ToOffsetInAnchor() != position) {
      return;
    }
    builder_.Append('^');
  }

  static bool IsVoidElement(const Element& element) {
    if (Strategy::HasChildren(element))
      return false;
    return ElementCannotHaveEndTag(element);
  }

  void SerializeChildren(const ContainerNode& container) {
    int offset_in_container = 0;
    for (const Node& child : Strategy::ChildrenOf(container)) {
      HandleSelection(container, offset_in_container);
      HandleNode(child);
      ++offset_in_container;
    }
    HandleSelection(container, offset_in_container);
  }

  StringBuilder builder_;
  SelectionTemplate<Strategy> selection_;
};

}  // namespace

void SelectionSample::ConvertTemplatesToShadowRootsForTesring(
    HTMLElement& element) {
  ConvertTemplatesToShadowRoots(element);
}

SelectionInDOMTree SelectionSample::SetSelectionText(
    HTMLElement* element,
    const std::string& selection_text) {
  SelectionInDOMTree selection =
      Parser().SetSelectionText(element, selection_text);
  DCHECK(!selection.IsNone()) << "|selection_text| should container caret "
                                 "marker '|' or selection marker '^' and "
                                 "'|'.";
  return selection;
}

std::string SelectionSample::GetSelectionText(
    const ContainerNode& root,
    const SelectionInDOMTree& selection) {
  return Serializer<EditingStrategy>(selection).Serialize(root);
}

std::string SelectionSample::GetSelectionTextInFlatTree(
    const ContainerNode& root,
    const SelectionInFlatTree& selection) {
  return Serializer<EditingInFlatTreeStrategy>(selection).Serialize(root);
}

}  // namespace blink
