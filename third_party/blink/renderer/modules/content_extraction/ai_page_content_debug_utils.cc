// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_debug_utils.h"

#include "base/logging.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

const char* AttributeTypeToString(
    mojom::blink::AIPageContentAttributeType type) {
  switch (type) {
    case mojom::blink::AIPageContentAttributeType::kRoot:
      return "Root";
    case mojom::blink::AIPageContentAttributeType::kIframe:
      return "Iframe";
    case mojom::blink::AIPageContentAttributeType::kContainer:
      return "Container";
    case mojom::blink::AIPageContentAttributeType::kText:
      return "Text";
    case mojom::blink::AIPageContentAttributeType::kAnchor:
      return "Anchor";
    case mojom::blink::AIPageContentAttributeType::kParagraph:
      return "Paragraph";
    case mojom::blink::AIPageContentAttributeType::kHeading:
      return "Heading";
    case mojom::blink::AIPageContentAttributeType::kImage:
      return "Image";
    case mojom::blink::AIPageContentAttributeType::kSvgRoot:
      return "SvgRoot";
    case mojom::blink::AIPageContentAttributeType::kCanvas:
      return "Canvas";
    case mojom::blink::AIPageContentAttributeType::kVideo:
      return "Video";
    case mojom::blink::AIPageContentAttributeType::kForm:
      return "Form";
    case mojom::blink::AIPageContentAttributeType::kFormControl:
      return "FormControl";
    case mojom::blink::AIPageContentAttributeType::kTable:
      return "Table";
    case mojom::blink::AIPageContentAttributeType::kTableRow:
      return "TableRow";
    case mojom::blink::AIPageContentAttributeType::kTableCell:
      return "TableCell";
    case mojom::blink::AIPageContentAttributeType::kOrderedList:
      return "OrderedList";
    case mojom::blink::AIPageContentAttributeType::kUnorderedList:
      return "UnorderedList";
    case mojom::blink::AIPageContentAttributeType::kListItem:
      return "ListItem";
  }
  return "Unknown";
}

void AppendNodeToBuilder(const mojom::blink::AIPageContentNode* node,
                         StringBuilder& builder,
                         bool single_line = true) {
  if (!node) {
    builder.Append("<null>");
    return;
  }

  if (single_line) {
    // Single-line format for tree dumps
    builder.Append('<');
    builder.Append(
        AttributeTypeToString(node->content_attributes->attribute_type));
    builder.Append('>');

    if (node->content_attributes->text_info) {
      builder.Append(' ');
      builder.Append('"');
      String text_content =
          node->content_attributes->text_info->text_content.Replace('\n', ' ');
      builder.Append(text_content);
      builder.Append('"');
    }

    if (node->content_attributes->geometry) {
      bool is_same_box =
          node->content_attributes->geometry->outer_bounding_box ==
          node->content_attributes->geometry->visible_bounding_box;
      if (is_same_box) {
        builder.Append(" [box: ");
        builder.Append(String::FromUTF8(
            node->content_attributes->geometry->outer_bounding_box.ToString()));
        builder.Append(']');
      } else {
        builder.Append(" [outerBox: ");
        builder.Append(String::FromUTF8(
            node->content_attributes->geometry->outer_bounding_box.ToString()));
        builder.Append(']');
        builder.Append(" [visibleBox: ");
        builder.Append(String::FromUTF8(node->content_attributes->geometry
                                            ->visible_bounding_box.ToString()));
        builder.Append(']');
      }
      int fragment_index = 0;
      for (const auto& fragment_rect : node->content_attributes->geometry
                                           ->fragment_visible_bounding_boxes) {
        builder.Append(" [fragmentBox#");
        builder.AppendNumber(fragment_index++);
        builder.Append(' ');
        builder.Append(String::FromUTF8(fragment_rect.ToString()));
        builder.Append(']');
      }
    }
  } else {
    // Multi-line format for individual nodes
    String type_name =
        AttributeTypeToString(node->content_attributes->attribute_type);
    builder.Append(type_name.LowerASCII());

    if (node->content_attributes->text_info) {
      builder.Append(' ');
      builder.Append('"');
      String text_content =
          node->content_attributes->text_info->text_content.Replace('\n', ' ');
      builder.Append(text_content);
      builder.Append('"');
    }

    if (node->content_attributes->geometry) {
      bool is_same_box =
          node->content_attributes->geometry->outer_bounding_box ==
          node->content_attributes->geometry->visible_bounding_box;

      builder.Append('\n');
      if (is_same_box) {
        builder.Append("  bounding_box: [");
        builder.Append(String::FromUTF8(
            node->content_attributes->geometry->outer_bounding_box.ToString()));
        builder.Append(']');
      } else {
        builder.Append("  outer_bounding_box: [");
        builder.Append(String::FromUTF8(
            node->content_attributes->geometry->outer_bounding_box.ToString()));
        builder.Append("]\n");
        builder.Append("  visible_bounding_box: [");
        builder.Append(String::FromUTF8(node->content_attributes->geometry
                                            ->visible_bounding_box.ToString()));
        builder.Append(']');
      }

      if (!node->content_attributes->geometry->fragment_visible_bounding_boxes
               .empty()) {
        builder.Append('\n');
        builder.Append("  fragment_visible_bounding_boxes:");
        for (const auto& fragment_rect :
             node->content_attributes->geometry
                 ->fragment_visible_bounding_boxes) {
          builder.Append('\n');
          builder.Append("    [");
          builder.Append(String::FromUTF8(fragment_rect.ToString()));
          builder.Append(']');
        }
      }
    }
  }
}

void ContentNodeTreeToStringHelper(
    const mojom::blink::AIPageContentNode* node,
    const mojom::blink::AIPageContentNode* marked_node,
    int indent,
    StringBuilder& builder) {
  if (!node) {
    builder.Append("(null)\n");
    return;
  }

  if (marked_node) {
    builder.Append(node == marked_node ? "* " : "  ");
  }
  for (int i = 0; i < indent * 2; ++i) {
    builder.Append(' ');
  }
  AppendNodeToBuilder(node, builder);
  builder.Append('\n');

  for (const auto& child : node->children_nodes) {
    ContentNodeTreeToStringHelper(child.get(), marked_node, indent + 1,
                                  builder);
  }
}

}  // namespace

String ContentNodeTreeToString(const mojom::blink::AIPageContentNode* node) {
  StringBuilder builder;
  ContentNodeTreeToStringHelper(node, nullptr, 0, builder);
  return builder.ToString();
}

String ContentNodeTreeToStringWithMarkedNode(
    const mojom::blink::AIPageContentNode* node,
    const mojom::blink::AIPageContentNode* marked_node) {
  StringBuilder builder;
  ContentNodeTreeToStringHelper(node, marked_node, 0, builder);
  return builder.ToString();
}

namespace {

bool FindNodeAndBuildParentChain(
    const mojom::blink::AIPageContentNode* current,
    const mojom::blink::AIPageContentNode* target,
    Vector<const mojom::blink::AIPageContentNode*>& chain) {
  if (!current) {
    return false;
  }

  chain.push_back(current);

  if (current == target) {
    return true;
  }

  for (const auto& child : current->children_nodes) {
    if (FindNodeAndBuildParentChain(child.get(), target, chain)) {
      return true;
    }
  }

  chain.pop_back();
  return false;
}

}  // namespace

String ContentNodeParentChainToString(
    const mojom::blink::AIPageContentNode* root,
    const mojom::blink::AIPageContentNode* target) {
  Vector<const mojom::blink::AIPageContentNode*> chain;
  if (!FindNodeAndBuildParentChain(root, target, chain)) {
    return "Target node not found in tree.";
  }

  StringBuilder builder;
  for (wtf_size_t i = 0; i < chain.size(); ++i) {
    const auto* node = chain[i];
    for (wtf_size_t j = 0; j < i * 2; ++j) {
      builder.Append(' ');
    }
    AppendNodeToBuilder(node, builder);
    builder.Append('\n');
  }
  return builder.ToString();
}

String ContentNodeToString(const mojom::blink::AIPageContentNode* target,
                           bool format_on_single_line) {
  StringBuilder builder;
  AppendNodeToBuilder(target, builder, format_on_single_line);
  return builder.ToString();
}

}  // namespace blink
