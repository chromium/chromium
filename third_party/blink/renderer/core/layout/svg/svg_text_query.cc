/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/svg/svg_text_query.h"

#include <algorithm>
#include "third_party/blink/renderer/core/layout/api/line_layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/line/inline_flow_box.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_inline_text_box.h"
#include "third_party/blink/renderer/core/layout/svg/svg_text_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/svg_text_metrics.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Base structure for callback user data
struct QueryData {
  QueryData()
      : is_vertical_text(false),
        current_offset(0),
        text_line_layout(nullptr),
        text_box(nullptr) {}

  bool is_vertical_text;
  unsigned current_offset;
  LineLayoutSVGInlineText text_line_layout;
  const SVGInlineTextBox* text_box;
};

static inline InlineFlowBox* FlowBoxForLayoutObject(
    LayoutObject* layout_object) {
  if (!layout_object)
    return nullptr;

  if (layout_object->IsLayoutBlock()) {
    // If we're given a block element, it has to be a LayoutSVGText.
    DCHECK(layout_object->IsSVGText());
    auto* layout_block_flow = To<LayoutBlockFlow>(layout_object);

    // LayoutSVGText only ever contains a single line box.
    InlineFlowBox* flow_box = layout_block_flow->FirstLineBox();
    DCHECK_EQ(flow_box, layout_block_flow->LastLineBox());
    return flow_box;
  }

  if (layout_object->IsLayoutInline()) {
    // We're given a LayoutSVGInline or objects that derive from it
    // (LayoutSVGTSpan / LayoutSVGTextPath)
    LayoutInline* layout_inline = ToLayoutInline(layout_object);

    // LayoutSVGInline only ever contains a single line box.
    InlineFlowBox* flow_box = layout_inline->FirstLineBox();
    DCHECK_EQ(flow_box, layout_inline->LastLineBox());
    return flow_box;
  }

  NOTREACHED();
  return nullptr;
}

static void CollectTextBoxesInFlowBox(InlineFlowBox* flow_box,
                                      Vector<SVGInlineTextBox*>& text_boxes) {
  if (!flow_box)
    return;

  for (InlineBox* child = flow_box->FirstChild(); child;
       child = child->NextOnLine()) {
    if (child->IsInlineFlowBox()) {
      // Skip generated content.
      if (!child->GetLineLayoutItem().GetNode())
        continue;

      CollectTextBoxesInFlowBox(ToInlineFlowBox(child), text_boxes);
      continue;
    }

    if (child->IsSVGInlineTextBox())
      text_boxes.push_back(ToSVGInlineTextBox(child));
  }
}

typedef bool ProcessTextFragmentCallback(QueryData*, const SVGTextFragment&);

static bool QueryTextBox(QueryData* query_data,
                         const SVGInlineTextBox* text_box,
                         ProcessTextFragmentCallback fragment_callback) {
  query_data->text_box = text_box;
  query_data->text_line_layout =
      LineLayoutSVGInlineText(text_box->GetLineLayoutItem());

  query_data->is_vertical_text =
      !query_data->text_line_layout.StyleRef().IsHorizontalWritingMode();

  // Loop over all text fragments in this text box, firing a callback for each.
  for (const SVGTextFragment& fragment : text_box->TextFragments()) {
    if (fragment_callback(query_data, fragment))
      return true;
  }
  return false;
}

// Execute a query in "spatial" order starting at |queryRoot|. This means
// walking the lines boxes in the order they would get painted.
static void SpatialQuery(LayoutObject* query_root,
                         QueryData* query_data,
                         ProcessTextFragmentCallback fragment_callback) {
  Vector<SVGInlineTextBox*> text_boxes;
  CollectTextBoxesInFlowBox(FlowBoxForLayoutObject(query_root), text_boxes);

  // Loop over all text boxes
  for (const SVGInlineTextBox* text_box : text_boxes) {
    if (QueryTextBox(query_data, text_box, fragment_callback))
      return;
  }
}

static void CollectTextBoxesInLogicalOrder(
    LineLayoutSVGInlineText text_line_layout,
    Vector<SVGInlineTextBox*>& text_boxes) {
  text_boxes.Shrink(0);
  for (InlineTextBox* text_box = text_line_layout.FirstTextBox(); text_box;
       text_box = text_box->NextForSameLayoutObject())
    text_boxes.push_back(ToSVGInlineTextBox(text_box));
  std::sort(text_boxes.begin(), text_boxes.end(),
            InlineTextBox::CompareByStart);
}

// Execute a query in "logical" order starting at |queryRoot|. This means
// walking the lines boxes for each layout object in layout tree (pre)order.
static void LogicalQuery(LayoutObject* query_root,
                         QueryData* query_data,
                         ProcessTextFragmentCallback fragment_callback) {
  if (!query_root)
    return;

  // Walk the layout tree in pre-order, starting at the specified root, and
  // run the query for each text node.
  Vector<SVGInlineTextBox*> text_boxes;
  for (LayoutObject* layout_object = query_root->SlowFirstChild();
       layout_object;
       layout_object = layout_object->NextInPreOrder(query_root)) {
    if (!layout_object->IsSVGInlineText())
      continue;

    LineLayoutSVGInlineText text_line_layout =
        LineLayoutSVGInlineText(ToLayoutSVGInlineText(layout_object));
    DCHECK(text_line_layout.Style());

    // TODO(fs): Allow filtering the search earlier, since we should be
    // able to trivially reject (prune) at least some of the queries.
    CollectTextBoxesInLogicalOrder(text_line_layout, text_boxes);

    for (const SVGInlineTextBox* text_box : text_boxes) {
      if (QueryTextBox(query_data, text_box, fragment_callback))
        return;
      query_data->current_offset += text_box->Len();
    }
  }
}

static bool MapStartEndPositionsIntoFragmentCoordinates(
    const QueryData* query_data,
    const SVGTextFragment& fragment,
    int& start_position,
    int& end_position) {
  unsigned box_start = query_data->current_offset;

  // Make <startPosition, endPosition> offsets relative to the current text box.
  start_position -= box_start;
  end_position -= box_start;

  // Reuse the same logic used for text selection & painting, to map our
  // query start/length into start/endPositions of the current text fragment.
  return query_data->text_box->MapStartEndPositionsIntoFragmentCoordinates(
      fragment, start_position, end_position);
}

// numberOfCharacters() implementation
static bool NumberOfCharactersCallback(QueryData*, const SVGTextFragment&) {
  // no-op
  return false;
}

unsigned SVGTextQuery::NumberOfCharacters() const {
  QueryData data;
  LogicalQuery(query_root_layout_object_, &data, NumberOfCharactersCallback);
  return data.current_offset;
}

// textLength() implementation
struct TextLengthData : QueryData {
  TextLengthData() : text_length(0) {}

  float text_length;
};

static bool TextLengthCallback(QueryData* query_data,
                               const SVGTextFragment& fragment) {
  TextLengthData* data = static_cast<TextLengthData*>(query_data);
  data->text_length +=
      query_data->is_vertical_text ? fragment.height : fragment.width;
  return false;
}

float SVGTextQuery::TextLength() const {
  TextLengthData data;
  LogicalQuery(query_root_layout_object_, &data, TextLengthCallback);
  return data.text_length;
}

using MetricsList = Vector<SVGTextMetrics>;

MetricsList::const_iterator FindMetricsForCharacter(
    const MetricsList& metrics_list,
    const SVGTextFragment& fragment,
    unsigned start_in_fragment) {
  // Find the text metrics cell that starts at or contains the character at
  // |startInFragment|.
  MetricsList::const_iterator metrics =
      metrics_list.begin() + fragment.metrics_list_offset;
  unsigned fragment_offset = 0;
  while (fragment_offset < fragment.length) {
    fragment_offset += metrics->length();
    if (start_in_fragment < fragment_offset)
      break;
    ++metrics;
  }
  DCHECK_LE(metrics, metrics_list.end());
  return metrics;
}

static float CalculateGlyphRange(const QueryData* query_data,
                                 const SVGTextFragment& fragment,
                                 unsigned start,
                                 unsigned end) {
  const MetricsList& metrics_list = query_data->text_line_layout.MetricsList();
  auto* metrics = FindMetricsForCharacter(metrics_list, fragment, start);
  auto* end_metrics = FindMetricsForCharacter(metrics_list, fragment, end);
  float glyph_range = 0;
  for (; metrics != end_metrics; ++metrics)
    glyph_range += metrics->Advance(query_data->is_vertical_text);
  return glyph_range;
}

// subStringLength() implementation
struct SubStringLengthData : QueryData {
  SubStringLengthData(unsigned query_start_position, unsigned query_length)
      : start_position(query_start_position),
        length(query_length),
        sub_string_length(0) {}

  unsigned start_position;
  unsigned length;

  float sub_string_length;
};

static bool SubStringLengthCallback(QueryData* query_data,
                                    const SVGTextFragment& fragment) {
  SubStringLengthData* data = static_cast<SubStringLengthData*>(query_data);

  int start_position = data->start_position;
  int end_position = start_position + data->length;
  if (!MapStartEndPositionsIntoFragmentCoordinates(
          query_data, fragment, start_position, end_position))
    return false;

  data->sub_string_length +=
      CalculateGlyphRange(query_data, fragment, start_position, end_position);
  return false;
}

float SVGTextQuery::SubStringLength(unsigned start_position,
                                    unsigned length) const {
  SubStringLengthData data(start_position, length);
  LogicalQuery(query_root_layout_object_, &data, SubStringLengthCallback);
  return data.sub_string_length;
}

// startPositionOfCharacter() implementation
struct StartPositionOfCharacterData : QueryData {
  StartPositionOfCharacterData(unsigned query_position)
      : position(query_position) {}

  unsigned position;
  FloatPoint start_position;
};

static FloatPoint LogicalGlyphPositionToPhysical(
    const QueryData* query_data,
    const SVGTextFragment& fragment,
    float logical_glyph_offset) {
  float physical_glyph_offset = logical_glyph_offset;
  if (!query_data->text_box->IsLeftToRightDirection()) {
    float fragment_extent =
        query_data->is_vertical_text ? fragment.height : fragment.width;
    physical_glyph_offset = fragment_extent - logical_glyph_offset;
  }

  FloatPoint glyph_position(fragment.x, fragment.y);
  if (query_data->is_vertical_text)
    glyph_position.Move(0, physical_glyph_offset);
  else
    glyph_position.Move(physical_glyph_offset, 0);

  return glyph_position;
}

static FloatPoint CalculateGlyphPosition(const QueryData* query_data,
                                         const SVGTextFragment& fragment,
                                         unsigned offset_in_fragment) {
  float glyph_offset_in_direction =
      CalculateGlyphRange(query_data, fragment, 0, offset_in_fragment);
  FloatPoint glyph_position = LogicalGlyphPositionToPhysical(
      query_data, fragment, glyph_offset_in_direction);
  if (fragment.IsTransformed()) {
    AffineTransform fragment_transform = fragment.BuildFragmentTransform(
        SVGTextFragment::kTransformIgnoringTextLength);
    glyph_position = fragment_transform.MapPoint(glyph_position);
  }
  return glyph_position;
}

static bool StartPositionOfCharacterCallback(QueryData* query_data,
                                             const SVGTextFragment& fragment) {
  StartPositionOfCharacterData* data =
      static_cast<StartPositionOfCharacterData*>(query_data);

  int start_position = data->position;
  int end_position = start_position + 1;
  if (!MapStartEndPositionsIntoFragmentCoordinates(
          query_data, fragment, start_position, end_position))
    return false;

  data->start_position =
      CalculateGlyphPosition(query_data, fragment, start_position);
  return true;
}

FloatPoint SVGTextQuery::StartPositionOfCharacter(unsigned position) const {
  StartPositionOfCharacterData data(position);
  LogicalQuery(query_root_layout_object_, &data,
               StartPositionOfCharacterCallback);
  return data.start_position;
}

// endPositionOfCharacter() implementation
struct EndPositionOfCharacterData : QueryData {
  EndPositionOfCharacterData(unsigned query_position)
      : position(query_position) {}

  unsigned position;
  FloatPoint end_position;
};

static bool EndPositionOfCharacterCallback(QueryData* query_data,
                                           const SVGTextFragment& fragment) {
  EndPositionOfCharacterData* data =
      static_cast<EndPositionOfCharacterData*>(query_data);

  int start_position = data->position;
  int end_position = start_position + 1;
  if (!MapStartEndPositionsIntoFragmentCoordinates(
          query_data, fragment, start_position, end_position))
    return false;

  data->end_position =
      CalculateGlyphPosition(query_data, fragment, end_position);
  return true;
}

FloatPoint SVGTextQuery::EndPositionOfCharacter(unsigned position) const {
  EndPositionOfCharacterData data(position);
  LogicalQuery(query_root_layout_object_, &data,
               EndPositionOfCharacterCallback);
  return data.end_position;
}

// rotationOfCharacter() implementation
struct RotationOfCharacterData : QueryData {
  RotationOfCharacterData(unsigned query_position)
      : position(query_position), rotation(0) {}

  unsigned position;
  float rotation;
};

static bool RotationOfCharacterCallback(QueryData* query_data,
                                        const SVGTextFragment& fragment) {
  RotationOfCharacterData* data =
      static_cast<RotationOfCharacterData*>(query_data);

  int start_position = data->position;
  int end_position = start_position + 1;
  if (!MapStartEndPositionsIntoFragmentCoordinates(
          query_data, fragment, start_position, end_position))
    return false;

  if (!fragment.IsTransformed()) {
    data->rotation = 0;
  } else {
    AffineTransform fragment_transform = fragment.BuildFragmentTransform(
        SVGTextFragment::kTransformIgnoringTextLength);
    fragment_transform.Scale(1 / fragment_transform.XScale(),
                             1 / fragment_transform.YScale());
    data->rotation = clampTo<float>(
        rad2deg(atan2(fragment_transform.B(), fragment_transform.A())));
  }
  return true;
}

float SVGTextQuery::RotationOfCharacter(unsigned position) const {
  RotationOfCharacterData data(position);
  LogicalQuery(query_root_layout_object_, &data, RotationOfCharacterCallback);
  return data.rotation;
}

// extentOfCharacter() implementation
struct ExtentOfCharacterData : QueryData {
  ExtentOfCharacterData(unsigned query_position) : position(query_position) {}

  unsigned position;
  FloatRect extent;
};

static FloatRect PhysicalGlyphExtents(const QueryData* query_data,
                                      const SVGTextMetrics& metrics,
                                      const FloatPoint& glyph_position) {
  FloatRect glyph_extents(glyph_position, metrics.Extents());

  // If RTL, adjust the starting point to align with the LHS of the glyph
  // bounding box.
  if (!query_data->text_box->IsLeftToRightDirection()) {
    if (query_data->is_vertical_text)
      glyph_extents.Move(0, -glyph_extents.Height());
    else
      glyph_extents.Move(-glyph_extents.Width(), 0);
  }
  return glyph_extents;
}

static inline FloatRect CalculateGlyphBoundaries(
    const QueryData* query_data,
    const SVGTextFragment& fragment,
    int start_position) {
  const float scaling_factor = query_data->text_line_layout.ScalingFactor();
  DCHECK(scaling_factor);
  const SimpleFontData* font_data =
      query_data->text_line_layout.ScaledFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return FloatRect();

  const float baseline =
      font_data->GetFontMetrics().FloatAscent() / scaling_factor;
  float glyph_offset_in_direction =
      CalculateGlyphRange(query_data, fragment, 0, start_position);
  FloatPoint glyph_position = LogicalGlyphPositionToPhysical(
      query_data, fragment, glyph_offset_in_direction);
  glyph_position.Move(0, -baseline);

  // Use the SVGTextMetrics computed by SVGTextMetricsBuilder.
  const MetricsList& metrics_list = query_data->text_line_layout.MetricsList();
  auto* metrics =
      FindMetricsForCharacter(metrics_list, fragment, start_position);

  FloatRect extent = PhysicalGlyphExtents(query_data, *metrics, glyph_position);
  if (fragment.IsTransformed()) {
    AffineTransform fragment_transform = fragment.BuildFragmentTransform(
        SVGTextFragment::kTransformIgnoringTextLength);
    extent = fragment_transform.MapRect(extent);
  }
  return extent;
}

static bool ExtentOfCharacterCallback(QueryData* query_data,
                                      const SVGTextFragment& fragment) {
  ExtentOfCharacterData* data = static_cast<ExtentOfCharacterData*>(query_data);

  int start_position = data->position;
  int end_position = start_position + 1;
  if (!MapStartEndPositionsIntoFragmentCoordinates(
          query_data, fragment, start_position, end_position))
    return false;

  data->extent = CalculateGlyphBoundaries(query_data, fragment, start_position);
  return true;
}

FloatRect SVGTextQuery::ExtentOfCharacter(unsigned position) const {
  ExtentOfCharacterData data(position);
  LogicalQuery(query_root_layout_object_, &data, ExtentOfCharacterCallback);
  return data.extent;
}

// characterNumberAtPosition() implementation
struct CharacterNumberAtPositionData : QueryData {
  CharacterNumberAtPositionData(const FloatPoint& query_position)
      : position(query_position),
        hit_layout_item(nullptr),
        offset_in_text_node(0) {}

  int CharacterNumberWithin(const LayoutObject* query_root) const;

  FloatPoint position;
  LineLayoutItem hit_layout_item;
  int offset_in_text_node;
};

int CharacterNumberAtPositionData::CharacterNumberWithin(
    const LayoutObject* query_root) const {
  // http://www.w3.org/TR/SVG/single-page.html#text-__svg__SVGTextContentElement__getCharNumAtPosition
  // "If no such character exists, a value of -1 is returned."
  if (!hit_layout_item)
    return -1;
  DCHECK(query_root);
  int character_number = offset_in_text_node;

  // Accumulate the lengths of all the text nodes preceding the target layout
  // object within the queried root, to get the complete character number.
  for (LineLayoutItem layout_item =
           hit_layout_item.PreviousInPreOrder(query_root);
       layout_item; layout_item = layout_item.PreviousInPreOrder(query_root)) {
    if (!layout_item.IsSVGInlineText())
      continue;
    character_number +=
        LineLayoutSVGInlineText(layout_item).ResolvedTextLength();
  }
  return character_number;
}

static unsigned LogicalOffsetInTextNode(
    LineLayoutSVGInlineText text_line_layout,
    const SVGInlineTextBox* start_text_box,
    unsigned fragment_offset) {
  Vector<SVGInlineTextBox*> text_boxes;
  CollectTextBoxesInLogicalOrder(text_line_layout, text_boxes);

  DCHECK(start_text_box);
  wtf_size_t index = text_boxes.Find(start_text_box);
  DCHECK_NE(index, kNotFound);

  unsigned offset = fragment_offset;
  while (index) {
    --index;
    offset += text_boxes[index]->Len();
  }
  return offset;
}

static bool CharacterNumberAtPositionCallback(QueryData* query_data,
                                              const SVGTextFragment& fragment) {
  CharacterNumberAtPositionData* data =
      static_cast<CharacterNumberAtPositionData*>(query_data);

  const float scaling_factor = data->text_line_layout.ScalingFactor();
  DCHECK(scaling_factor);

  const SimpleFontData* font_data =
      data->text_line_layout.ScaledFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return false;

  const float baseline =
      font_data->GetFontMetrics().FloatAscent() / scaling_factor;

  // Test the query point against the bounds of the entire fragment first.
  if (!fragment.BoundingBox(baseline).Contains(data->position))
    return false;

  AffineTransform fragment_transform = fragment.BuildFragmentTransform(
      SVGTextFragment::kTransformIgnoringTextLength);

  // Iterate through the glyphs in this fragment, and check if their extents
  // contain the query point.
  MetricsList::const_iterator metrics =
      data->text_line_layout.MetricsList().begin() +
      fragment.metrics_list_offset;
  unsigned fragment_offset = 0;
  float glyph_offset = 0;
  while (fragment_offset < fragment.length) {
    FloatPoint glyph_position =
        LogicalGlyphPositionToPhysical(data, fragment, glyph_offset);
    glyph_position.Move(0, -baseline);

    FloatRect extent = fragment_transform.MapRect(
        PhysicalGlyphExtents(data, *metrics, glyph_position));
    if (extent.Contains(data->position)) {
      // Compute the character offset of the glyph within the text node.
      unsigned offset_in_box = fragment.character_offset -
                               query_data->text_box->Start() + fragment_offset;
      data->offset_in_text_node = LogicalOffsetInTextNode(
          query_data->text_line_layout, query_data->text_box, offset_in_box);
      data->hit_layout_item = LineLayoutItem(data->text_line_layout);
      return true;
    }
    fragment_offset += metrics->length();
    glyph_offset += metrics->Advance(data->is_vertical_text);
    ++metrics;
  }
  return false;
}

int SVGTextQuery::CharacterNumberAtPosition(const FloatPoint& position) const {
  CharacterNumberAtPositionData data(position);
  SpatialQuery(query_root_layout_object_, &data,
               CharacterNumberAtPositionCallback);
  return data.CharacterNumberWithin(query_root_layout_object_);
}

}  // namespace blink
