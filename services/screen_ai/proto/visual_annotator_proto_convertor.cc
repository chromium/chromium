// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/screen_ai/proto/visual_annotator_proto_convertor.h"

#include <stdint.h>

#include <iterator>
#include <map>
#include <ranges>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/ranges.h"
#include "base/strings/utf_string_conversions.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "services/strings/grit/services_strings.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace ranges = base::ranges;

namespace {

// A negative ID for ui::AXNodeID needs to start from -2 as using -1 for this
// node id is still incorrectly treated as invalid.
// TODO(crbug.com/40908646): fix code treating -1 as invalid for ui::AXNodeID.
constexpr int kFirstValidNegativeId = -2;

ui::AXNodeID next_negative_node_id{kFirstValidNegativeId};

// Returns the next valid negative ID that can be used for identifying
// `AXNode`s in the accessibility tree. Using negative IDs here enables
// adding nodes built from OCR results to PDF accessibility tree in PDF
// renderer without updating their IDs.
ui::AXNodeID GetNextNegativeNodeID() {
  return next_negative_node_id--;
}

gfx::RectF ToGfxRect(const chrome_screen_ai::Rect& rect) {
  return gfx::RectF(rect.x(), rect.y(), rect.width(), rect.height());
}

// The bounding box angle specifies the rotation around the bounding box's (x,y)
// point.
void UpdateBoundingBoxIfRotated(chrome_screen_ai::Rect* bounding_box) {
  if (bounding_box->angle() == 0) {
    return;
  }

  gfx::Transform transform;
  transform.Rotate(bounding_box->angle());
  transform.ApplyTransformOrigin(bounding_box->x(), bounding_box->y(), 0);

  gfx::RectF rotated_rect = transform.MapRect(ToGfxRect(*bounding_box));
  bounding_box->set_x(rotated_rect.x());
  bounding_box->set_y(rotated_rect.y());
  bounding_box->set_width(rotated_rect.width());
  bounding_box->set_height(rotated_rect.height());
}

bool HaveIdenticalFormattingStyle(const chrome_screen_ai::WordBox& word_1,
                                  const chrome_screen_ai::WordBox& word_2) {
  if (word_1.language() != word_2.language()) {
    return false;
  }
  if (word_1.direction() != word_2.direction()) {
    return false;
  }
  if (word_1.content_type() != word_2.content_type()) {
    return false;
  }
  return true;
}

void SerializeBoundingBox(const chrome_screen_ai::Rect& bounding_box,
                          const ui::AXNodeID& container_id,
                          ui::AXNodeData& out_data) {
  out_data.relative_bounds.bounds = ToGfxRect(bounding_box);
  // TODO(crbug.com/347622611): Instead of DCHECK, drop empty boxes in
  // preprocessing.
  DCHECK(!out_data.relative_bounds.bounds.IsEmpty());
}

void SerializeDirection(const chrome_screen_ai::Direction& direction,
                        ui::AXNodeData& out_data) {
  DCHECK(chrome_screen_ai::Direction_IsValid(direction));
  switch (direction) {
    case chrome_screen_ai::DIRECTION_UNSPECIFIED:
    // We assume that LEFT_TO_RIGHT is the default direction.
    case chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT:
      out_data.AddIntAttribute(
          ax::mojom::IntAttribute::kTextDirection,
          static_cast<int32_t>(ax::mojom::WritingDirection::kLtr));
      break;
    case chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT:
      out_data.AddIntAttribute(
          ax::mojom::IntAttribute::kTextDirection,
          static_cast<int32_t>(ax::mojom::WritingDirection::kRtl));
      break;
    case chrome_screen_ai::DIRECTION_TOP_TO_BOTTOM:
      out_data.AddIntAttribute(
          ax::mojom::IntAttribute::kTextDirection,
          static_cast<int32_t>(ax::mojom::WritingDirection::kTtb));
      break;
    case google::protobuf::kint32min:
    case google::protobuf::kint32max:
      // Ordinarily, a default case should have been added to permit future
      // additions to `chrome_screen_ai::Direction`. However, in this
      // case, both the screen_ai library and this code should always be in
      // sync.
      NOTREACHED_IN_MIGRATION()
          << "Unrecognized chrome_screen_ai::Direction value: " << direction;
      break;
  }
}

void SerializeContentType(const chrome_screen_ai::ContentType& content_type,
                          ui::AXNodeData& out_data) {
  DCHECK(chrome_screen_ai::ContentType_IsValid(content_type));
  switch (content_type) {
    case chrome_screen_ai::CONTENT_TYPE_PRINTED_TEXT:
    case chrome_screen_ai::CONTENT_TYPE_HANDWRITTEN_TEXT:
      out_data.role = ax::mojom::Role::kStaticText;
      break;
    case chrome_screen_ai::CONTENT_TYPE_IMAGE:
      out_data.role = ax::mojom::Role::kImage;
      break;
    case chrome_screen_ai::CONTENT_TYPE_LINE_DRAWING:
      out_data.role = ax::mojom::Role::kGraphicsObject;
      break;
    case chrome_screen_ai::CONTENT_TYPE_SEPARATOR:
      out_data.role = ax::mojom::Role::kSplitter;
      break;
    case chrome_screen_ai::CONTENT_TYPE_UNREADABLE_TEXT:
      out_data.role = ax::mojom::Role::kGraphicsObject;
      break;
    case chrome_screen_ai::CONTENT_TYPE_FORMULA:
    case chrome_screen_ai::CONTENT_TYPE_HANDWRITTEN_FORMULA:
      // Note that `Role::kMath` indicates that the formula is not represented
      // as a subtree of MathML elements in the accessibility tree, but as a raw
      // string which may optionally be written in MathML, but could also be
      // written in plain text.
      out_data.role = ax::mojom::Role::kMath;
      break;
    case chrome_screen_ai::CONTENT_TYPE_SIGNATURE:
      // Signatures may be readable, but even when they are not we could still
      // try our best.
      // TODO(accessibility): Explore adding a description attribute informing
      // the user that this is a signature, e.g. via ARIA Annotations.
      out_data.role = ax::mojom::Role::kStaticText;
      break;
    case google::protobuf::kint32min:
    case google::protobuf::kint32max:
      // Ordinarily, a default case should have been added to permit future
      // additions to `chrome_screen_ai::ContentType`. However, in this
      // case, both the screen_ai library and this code should always be in
      // sync.
      NOTREACHED_IN_MIGRATION()
          << "Unrecognized chrome_screen_ai::ContentType value: "
          << content_type;
      break;
  }
}

void UpdateCharacterOffsets(const chrome_screen_ai::WordBox& word_box,
                            ui::AXNodeData& inline_text_box,
                            bool space_after_previous_word) {
  chrome_screen_ai::Direction direction = word_box.direction();

  if (direction != chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT &&
      direction != chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT) {
    // To be added when OCR supports it. Without Character Offsets, character
    // and word boundary boxes are not drawn.
    return;
  }
  bool left_to_right = (direction == chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT);

  if (word_box.symbols().empty()) {
    return;
  }

  std::vector<int32_t> character_offsets = inline_text_box.GetIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets);

  // Unrotate the line and symbols before computing the offsets.
  gfx::Transform transform;
  transform.Rotate(-word_box.bounding_box().angle());
  transform.ApplyTransformOrigin(inline_text_box.relative_bounds.bounds.x(),
                                 inline_text_box.relative_bounds.bounds.y(), 0);

  gfx::RectF line_rect =
      transform.MapRect(inline_text_box.relative_bounds.bounds);
  std::vector<gfx::RectF> symbols;
  ranges::transform(
      word_box.symbols(), std::back_inserter(symbols),
      [transform](const chrome_screen_ai::SymbolBox& symbol) {
        return transform.MapRect(ToGfxRect(symbol.bounding_box()));
      });

  int32_t line_offset =
      left_to_right ? base::ClampRound(line_rect.x())
                    : base::ClampRound(line_rect.x() + line_rect.width());

  if (space_after_previous_word) {
    gfx::RectF word_rect =
        transform.MapRect(ToGfxRect(word_box.bounding_box()));
    int word_start =
        left_to_right ? word_rect.x() : (word_rect.x() + word_rect.width());
    character_offsets.push_back(abs(line_offset - word_start));
  }

  ranges::transform(
      symbols.begin(), symbols.end(), std::back_inserter(character_offsets),
      [line_offset, left_to_right](const gfx::RectF& symbol) {
        int symbol_end =
            left_to_right ? symbol.x() + symbol.width() : symbol.x();
        return abs(symbol_end - line_offset);
      });

  inline_text_box.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets);
}

void SerializeWordBox(const chrome_screen_ai::WordBox& word_box,
                      ui::AXNodeData& inline_text_box,
                      bool space_after_previous_word) {
  DCHECK_NE(inline_text_box.id, ui::kInvalidAXNodeID);

  // TODO(crbug.com/347622611): Drop empty words in preprocessing.
  if (word_box.utf8_string().empty()) {
    return;
  }

  // The boundaries of each `inline_text_box` is computed as the union of
  // the boundaries of all `word_box`es that are inside.
  inline_text_box.relative_bounds.bounds.Union(
      ToGfxRect(word_box.bounding_box()));

  UpdateCharacterOffsets(word_box, inline_text_box, space_after_previous_word);

  std::string inner_text =
      inline_text_box.GetStringAttribute(ax::mojom::StringAttribute::kName);
  inner_text += word_box.utf8_string();
  // Word length should specify the number of characters, which differs
  // from the number of bytes in multi-byte characters.
  size_t word_length = base::UTF8ToUTF16(word_box.utf8_string()).length();
  if (word_box.has_space_after()) {
    inner_text += " ";
    ++word_length;
  }
  inline_text_box.SetNameChecked(inner_text);

  std::vector<int32_t> word_starts = inline_text_box.GetIntListAttribute(
      ax::mojom::IntListAttribute::kWordStarts);
  std::vector<int32_t> word_ends = inline_text_box.GetIntListAttribute(
      ax::mojom::IntListAttribute::kWordEnds);
  int32_t new_word_start = 0;
  int32_t new_word_end = base::checked_cast<int32_t>(word_length) - 1;
  if (!word_ends.empty()) {
    new_word_start += word_ends[word_ends.size() - 1] + 1;
    new_word_end += new_word_start;
  }
  word_starts.push_back(new_word_start);
  word_ends.push_back(new_word_end);
  inline_text_box.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                      word_starts);
  inline_text_box.AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                      word_ends);
  DCHECK_LE(new_word_start, new_word_end);
  DCHECK_LE(
      new_word_end,
      base::checked_cast<int32_t>(
          inline_text_box.GetStringAttribute(ax::mojom::StringAttribute::kName)
              .length()));

  if (word_box.estimate_color_success()) {
    if (!inline_text_box.HasIntAttribute(
            ax::mojom::IntAttribute::kBackgroundColor)) {
      inline_text_box.AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                                      word_box.background_rgb_value());
    }
    if (!inline_text_box.HasIntAttribute(ax::mojom::IntAttribute::kColor)) {
      inline_text_box.AddIntAttribute(ax::mojom::IntAttribute::kColor,
                                      word_box.foreground_rgb_value());
    }
  }
  SerializeDirection(word_box.direction(), inline_text_box);
}

// Creates an inline text box for every style span in the provided
// `static_text_node`, starting from `start_from_word_index` in the node's
// `word_boxes`. Returns the number of inline text box nodes that have
// been initialized in `node_data`.
size_t SerializeWordBoxes(const google::protobuf::RepeatedPtrField<
                              chrome_screen_ai::WordBox>& word_boxes,
                          const int start_from_word_index,
                          const size_t node_index,
                          ui::AXNodeData& static_text_node,
                          std::vector<ui::AXNodeData>& node_data) {
  if (word_boxes.empty()) {
    return 0u;
  }
  DCHECK_LT(start_from_word_index, word_boxes.size());
  DCHECK_LT(node_index, node_data.size());
  DCHECK_NE(static_text_node.id, ui::kInvalidAXNodeID);
  ui::AXNodeData& inline_text_box_node = node_data[node_index];
  DCHECK_EQ(inline_text_box_node.role, ax::mojom::Role::kUnknown);
  inline_text_box_node.role = ax::mojom::Role::kInlineTextBox;
  inline_text_box_node.id = GetNextNegativeNodeID();
  // The union of the bounding boxes in this formatting context is set as
  // the bounding box of `inline_text_box_node`.
  DCHECK(inline_text_box_node.relative_bounds.bounds.IsEmpty());

  static_text_node.child_ids.push_back(inline_text_box_node.id);

  const auto formatting_context_start =
      std::cbegin(word_boxes) + start_from_word_index;
  const auto formatting_context_end =
      ranges::find_if_not(formatting_context_start,
                          std::ranges::end(word_boxes),
                          [formatting_context_start](const auto& word_box) {
                            return HaveIdenticalFormattingStyle(
                                *formatting_context_start, word_box);
                          });
  bool has_space_after_previous_word = false;
  for (auto word_iter = formatting_context_start;
       word_iter != formatting_context_end; ++word_iter) {
    SerializeWordBox(*word_iter, inline_text_box_node,
                     has_space_after_previous_word);
    has_space_after_previous_word = word_iter->has_space_after();
  }

  std::string language = formatting_context_start->language();
  if (!language.empty() &&
      language != static_text_node.GetStringAttribute(
                      ax::mojom::StringAttribute::kLanguage)) {
    inline_text_box_node.AddStringAttribute(
        ax::mojom::StringAttribute::kLanguage, language);
  }

  if (formatting_context_end != std::cend(word_boxes)) {
    return 1u +
           SerializeWordBoxes(
               word_boxes,
               std::distance(std::cbegin(word_boxes), formatting_context_end),
               (node_index + 1u), static_text_node, node_data);
  }
  return 1u;
}

// Returns the number of accessibility nodes that have been initialized in
// `node_data`. A single `line_box` may turn into a number of inline text
// boxes depending on how many formatting contexts it contains. If
// `line_box` is of a non-textual nature, only one node will be
// initialized.
size_t SerializeLineBox(const chrome_screen_ai::LineBox& line_box,
                        const size_t index,
                        ui::AXNodeData& parent_node,
                        std::vector<ui::AXNodeData>& node_data) {
  DCHECK_LT(index, node_data.size());
  DCHECK_NE(parent_node.id, ui::kInvalidAXNodeID);
  ui::AXNodeData& line_box_node = node_data[index];
  DCHECK_EQ(line_box_node.role, ax::mojom::Role::kUnknown);

  SerializeContentType(line_box.content_type(), line_box_node);
  line_box_node.id = GetNextNegativeNodeID();
  SerializeBoundingBox(line_box.bounding_box(), parent_node.id, line_box_node);
  // `ax::mojom::NameFrom` should be set to the correct value based on the
  // role.
  line_box_node.SetNameChecked(line_box.utf8_string());
  if (!line_box.language().empty() &&
      line_box.language() != parent_node.GetStringAttribute(
                                 ax::mojom::StringAttribute::kLanguage)) {
    line_box_node.AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                     line_box.language());
  }
  SerializeDirection(line_box.direction(), line_box_node);
  parent_node.child_ids.push_back(line_box_node.id);

  if (!ui::IsText(line_box_node.role)) {
    return 1u;
  }
  return 1u + SerializeWordBoxes(line_box.words(),
                                 /* start_from_word_index */ 0, (index + 1u),
                                 line_box_node, node_data);
}

gfx::Rect ProtoToMojo(const chrome_screen_ai::Rect& source) {
  gfx::Rect dest;
  dest.set_x(source.x());
  dest.set_y(source.y());
  dest.set_width(source.width());
  dest.set_height(source.height());
  return dest;
}

screen_ai::mojom::Direction ProtoToMojo(chrome_screen_ai::Direction direction) {
  switch (direction) {
    case chrome_screen_ai::Direction::DIRECTION_UNSPECIFIED:
      return screen_ai::mojom::Direction::DIRECTION_UNSPECIFIED;

    case chrome_screen_ai::Direction::DIRECTION_LEFT_TO_RIGHT:
      return screen_ai::mojom::Direction::DIRECTION_LEFT_TO_RIGHT;

    case chrome_screen_ai::Direction::DIRECTION_RIGHT_TO_LEFT:
      return screen_ai::mojom::Direction::DIRECTION_RIGHT_TO_LEFT;

    case chrome_screen_ai::Direction::DIRECTION_TOP_TO_BOTTOM:
      return screen_ai::mojom::Direction::DIRECTION_TOP_TO_BOTTOM;

    case chrome_screen_ai::Direction_INT_MIN_SENTINEL_DO_NOT_USE_:
    case chrome_screen_ai::Direction_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED_IN_MIGRATION();
      return screen_ai::mojom::Direction::DIRECTION_UNSPECIFIED;
  }
}

}  // namespace

namespace screen_ai {

void ResetNodeIDForTesting() {
  next_negative_node_id = kFirstValidNegativeId;
}

ui::AXTreeUpdate VisualAnnotationToAXTreeUpdate(
    chrome_screen_ai::VisualAnnotation& visual_annotation,
    const gfx::Rect& image_rect) {
  ui::AXTreeUpdate update;

  if (visual_annotation.lines().empty()) {
    return update;
  }

  // TODO(https://crbug.com/327298772): Create an AXTreeSource and create the
  // update using AXTreeSerializer.

  // TODO(crbug.com/347622611): Consider adding 'const' to `visual_annotation`
  // argument when preprocessing step is added.
  for (chrome_screen_ai::LineBox& line : *visual_annotation.mutable_lines()) {
    if (line.has_bounding_box()) {
      UpdateBoundingBoxIfRotated(line.mutable_bounding_box());
    }
    for (chrome_screen_ai::WordBox& word : *line.mutable_words()) {
      if (word.has_bounding_box()) {
        UpdateBoundingBoxIfRotated(word.mutable_bounding_box());
      }
      for (chrome_screen_ai::SymbolBox& symbol : *word.mutable_symbols()) {
        if (symbol.has_bounding_box()) {
          UpdateBoundingBoxIfRotated(symbol.mutable_bounding_box());
        }
      }
    }
  }

  // Each `UIComponent`, `LineBox`, as well as every `WordBox` that results in a
  // different formatting context, will take up one node in the accessibility
  // tree, resulting in hundreds of nodes, making it inefficient to push_back
  // one node at a time. We pre-allocate the needed nodes making node creation
  // an O(n) operation.
  size_t formatting_context_count = 0u;
  for (const chrome_screen_ai::LineBox& line : visual_annotation.lines()) {
    // By design, and same as in Blink, every line creates a separate formatting
    // context regardless as to whether the format styles are identical with
    // previous lines or not.
    ++formatting_context_count;
    DCHECK(!line.words().empty())
        << "Empty lines should have been pruned in the Screen AI library.";
    for (auto iter = std::cbegin(line.words());
         std::next(iter) != std::cend(line.words()); ++iter) {
      if (!HaveIdenticalFormattingStyle(*iter, *std::next(iter)))
        ++formatting_context_count;
    }
  }

  // Each unique `chrome_screen_ai::LineBox::block_id` signifies a different
  // block of text, and so it creates a new static text node in the
  // accessibility tree. Each block has a sorted set of line boxes, everyone of
  // which is turned into one or more inline text box nodes in the accessibility
  // tree. Line boxes are sorted using their
  // `chrome_screen_ai::LineBox::order_within_block` member and are identified
  // by their index in the container of line boxes. Use std::map to sort both
  // text blocks and the line boxes that belong to each one, both operations
  // having an O(n * log(n)) complexity.
  // TODO(accessibility): Determine reading order based on visual positioning of
  // text blocks, not on the order of their block IDs.
  std::map<int32_t, std::map<int32_t, int>> blocks_to_lines_map;
  for (int i = 0; i < visual_annotation.lines_size(); ++i) {
    const chrome_screen_ai::LineBox& line = visual_annotation.lines(i);
    blocks_to_lines_map[line.block_id()].emplace(
        std::make_pair(line.order_within_block(), i));
  }

  // Need four more nodes that convey the disclaimer messages. There are two
  // messages, one before the content and one after. Each message is wrapped
  // in an ARIA landmark so that it can easily be navigated to by a screen
  // reader user and thus not missed.
  formatting_context_count += 4;

  // There are the same number of paragraphs as blocks.
  size_t paragraph_count = blocks_to_lines_map.size();

  std::vector<ui::AXNodeData> nodes(1 +  // Root Node
                                    visual_annotation.lines().size() +
                                    paragraph_count + formatting_context_count);

  size_t index = 0u;

  // We assume that OCR is performed on a page-by-page basis.
  ui::AXNodeData& page_node = nodes[index++];
  page_node.role = ax::mojom::Role::kRegion;
  page_node.id = GetNextNegativeNodeID();
  update.root_id = page_node.id;
  page_node.AddBoolAttribute(ax::mojom::BoolAttribute::kIsPageBreakingObject,
                             true);
  page_node.AddStringAttribute(ax::mojom::StringAttribute::kClassName,
                               "ocred_page");
  page_node.relative_bounds.bounds = gfx::RectF(image_rect);

  // Add a disclaimer node informing the user of the beginning of extracted
  // text, and place the message inside an appropriate ARIA landmark for easy
  // navigation.
  ui::AXNodeData& begin_node_wrapper = nodes[index++];
  begin_node_wrapper.role = ax::mojom::Role::kBanner;
  begin_node_wrapper.id = GetNextNegativeNodeID();
  begin_node_wrapper.relative_bounds.bounds =
      gfx::RectF(image_rect.x(), image_rect.y(), 1, 1);
  page_node.child_ids.push_back(begin_node_wrapper.id);
  ui::AXNodeData& begin_node = nodes[index++];
  begin_node.role = ax::mojom::Role::kStaticText;
  begin_node.id = GetNextNegativeNodeID();
  begin_node.SetNameChecked(l10n_util::GetStringUTF8(IDS_PDF_OCR_RESULT_BEGIN));
  begin_node.relative_bounds.bounds = begin_node_wrapper.relative_bounds.bounds;
  begin_node_wrapper.child_ids.push_back(begin_node.id);

  for (const auto& block_to_lines_pair : blocks_to_lines_map) {
    // TODO(crbug.com/347622611): Create separate paragraphs based on the
    // blocks' spacing (e.g. by utilizing heuristics found in
    // PdfAccessibilityTree). Blocks as returned by the OCR engine are still
    // too small.
    ui::AXNodeData& paragraph_node = nodes[index++];
    paragraph_node.role = ax::mojom::Role::kParagraph;
    paragraph_node.id = GetNextNegativeNodeID();
    page_node.child_ids.push_back(paragraph_node.id);

    for (const auto& line_sequence_number_to_index_pair :
         block_to_lines_pair.second) {
      const chrome_screen_ai::LineBox& line_box =
          visual_annotation.lines(line_sequence_number_to_index_pair.second);
      // Every line with a textual accessibility role should turn into one or
      // more inline text boxes, each one  representing a formatting context.
      // If the line is not of a textual role, only one node is initialized
      // having a more specific role such as `ax::mojom::Role::kImage`.
      index += SerializeLineBox(line_box, index, paragraph_node, nodes);

      // Accumulate bounds of all lines for the paragraph.
      auto& bounding_box = line_box.bounding_box();
      paragraph_node.relative_bounds.bounds.Union(ToGfxRect(bounding_box));
    }
  }

  // Add a disclaimer node informing the user of the end of extracted text,
  // and place the message inside an appropriate ARIA landmark for easy
  // navigation.
  ui::AXNodeData& end_node_wrapper = nodes[index++];
  end_node_wrapper.role = ax::mojom::Role::kContentInfo;
  end_node_wrapper.id = GetNextNegativeNodeID();
  end_node_wrapper.relative_bounds.bounds =
      gfx::RectF(image_rect.width(), image_rect.height(), 1, 1);
  page_node.child_ids.push_back(end_node_wrapper.id);
  ui::AXNodeData& end_node = nodes[index++];
  end_node.role = ax::mojom::Role::kStaticText;
  end_node.id = GetNextNegativeNodeID();
  end_node.SetNameChecked(l10n_util::GetStringUTF8(IDS_PDF_OCR_RESULT_END));
  end_node.relative_bounds.bounds = end_node_wrapper.relative_bounds.bounds;
  end_node_wrapper.child_ids.push_back(end_node.id);

  // Filter out invalid / unrecognized / unused nodes from the update.
  update.nodes.resize(nodes.size());
  const auto end_node_iter = ranges::copy_if(
      nodes, std::ranges::begin(update.nodes),
      [](const ui::AXNodeData& node_data) {
        return node_data.role != ax::mojom::Role::kUnknown &&
               node_data.id != ui::kInvalidAXNodeID;
      });
  update.nodes.resize(
      std::distance(std::ranges::begin(update.nodes), end_node_iter));

  return update;
}

mojom::VisualAnnotationPtr ConvertProtoToVisualAnnotation(
    const chrome_screen_ai::VisualAnnotation& annotation_proto) {
  auto annotation = screen_ai::mojom::VisualAnnotation::New();

  for (const auto& line : annotation_proto.lines()) {
    auto line_box = screen_ai::mojom::LineBox::New();
    line_box->text_line = line.utf8_string();
    line_box->block_id = line.block_id();
    line_box->language = line.language();
    line_box->order_within_block = line.order_within_block();
    line_box->bounding_box = ProtoToMojo(line.bounding_box());
    line_box->bounding_box_angle = line.bounding_box().angle();
    line_box->confidence = line.confidence();

    // `baseline_box` is not available in ChromeScreenAI library prior to
    // version 122.1.
    // If it is not provided by the OCR, the library assigns bounding box's
    // value to it and it's done the same here.
    auto baseline_box =
        line.has_baseline_box() ? line.baseline_box() : line.bounding_box();
    line_box->baseline_box = ProtoToMojo(baseline_box);
    line_box->baseline_box_angle = baseline_box.angle();

    for (const auto& word : line.words()) {
      auto word_box = screen_ai::mojom::WordBox::New();
      word_box->word = word.utf8_string();
      word_box->dictionary_word = word.dictionary_word();
      word_box->language = word.language();
      word_box->bounding_box = ProtoToMojo(word.bounding_box());
      word_box->bounding_box_angle = word.bounding_box().angle();
      word_box->direction = ProtoToMojo(word.direction());
      word_box->has_space_after = word.has_space_after();
      word_box->confidence = word.confidence();
      line_box->words.push_back(std::move(word_box));
    }
    annotation->lines.push_back(std::move(line_box));
  }

  return annotation;
}

}  // namespace screen_ai
