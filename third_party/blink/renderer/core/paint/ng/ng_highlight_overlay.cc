// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_highlight_overlay.h"

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/highlight/highlight_registry.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/platform/fonts/ng_text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

using HighlightLayerType = NGHighlightOverlay::HighlightLayerType;
using HighlightLayer = NGHighlightOverlay::HighlightLayer;
using HighlightRange = NGHighlightOverlay::HighlightRange;
using HighlightEdge = NGHighlightOverlay::HighlightEdge;
using HighlightDecoration = NGHighlightOverlay::HighlightDecoration;
using HighlightPart = NGHighlightOverlay::HighlightPart;

unsigned GetTextContentOffset(const Text& text, unsigned offset) {
  // TODO(yoichio): Sanitize DocumentMarker around text length.
  const Position position(text, std::min(offset, text.length()));
  const NGOffsetMapping* const offset_mapping =
      NGOffsetMapping::GetFor(position);
  DCHECK(offset_mapping);
  const absl::optional<unsigned>& ng_offset =
      offset_mapping->GetTextContentOffset(position);
  DCHECK(ng_offset.has_value());
  return ng_offset.value();
}

unsigned ClampOffset(unsigned offset, const NGTextFragmentPaintInfo& fragment) {
  return std::min(std::max(offset, fragment.from), fragment.to);
}

}  // namespace

String HighlightLayer::ToString() const {
  StringBuilder result{};
  switch (type) {
    case HighlightLayerType::kOriginating:
      result.Append("ORIG");
      break;
    case HighlightLayerType::kCustom:
      result.Append(name.GetString());
      break;
    case HighlightLayerType::kGrammar:
      result.Append("GRAM");
      break;
    case HighlightLayerType::kSpelling:
      result.Append("SPEL");
      break;
    case HighlightLayerType::kTargetText:
      result.Append("TARG");
      break;
    case HighlightLayerType::kSelection:
      result.Append("SELE");
      break;
    default:
      NOTREACHED();
  }
  return result.ToString();
}

enum PseudoId HighlightLayer::PseudoId() const {
  switch (type) {
    case HighlightLayerType::kOriginating:
      return kPseudoIdNone;
    case HighlightLayerType::kCustom:
      return kPseudoIdHighlight;
    case HighlightLayerType::kGrammar:
      return kPseudoIdGrammarError;
    case HighlightLayerType::kSpelling:
      return kPseudoIdSpellingError;
    case HighlightLayerType::kTargetText:
      return kPseudoIdTargetText;
    case HighlightLayerType::kSelection:
      return kPseudoIdSelection;
    default:
      NOTREACHED();
  }
}

const AtomicString& HighlightLayer::PseudoArgument() const {
  return name;
}

bool HighlightLayer::operator==(const HighlightLayer& other) const {
  return type == other.type && name == other.name;
}

bool HighlightLayer::operator!=(const HighlightLayer& other) const {
  return !operator==(other);
}

int8_t HighlightLayer::ComparePaintOrder(
    const HighlightLayer& other,
    const HighlightRegistry& registry) const {
  if (type < other.type) {
    return HighlightRegistry::OverlayStackingPosition::
        kOverlayStackingPositionBelow;
  }
  if (type > other.type) {
    return HighlightRegistry::OverlayStackingPosition::
        kOverlayStackingPositionAbove;
  }
  if (type != HighlightLayerType::kCustom) {
    return HighlightRegistry::OverlayStackingPosition::
        kOverlayStackingPositionEquivalent;
  }
  const HighlightRegistryMap& map = registry.GetHighlights();
  auto* this_entry =
      map.Find<HighlightRegistryMapEntryNameTranslator>(PseudoArgument())
          ->Get();
  auto* other_entry =
      map.Find<HighlightRegistryMapEntryNameTranslator>(other.PseudoArgument())
          ->Get();
  return registry.CompareOverlayStackingPosition(
      PseudoArgument(), this_entry->highlight, other.PseudoArgument(),
      other_entry->highlight);
}

HighlightRange::HighlightRange(unsigned from, unsigned to)
    : from(from), to(to) {
  DCHECK_LT(from, to);
}

bool HighlightRange::operator==(const HighlightRange& other) const {
  return from == other.from && to == other.to;
}

bool HighlightRange::operator!=(const HighlightRange& other) const {
  return !operator==(other);
}

String HighlightRange::ToString() const {
  StringBuilder result{};
  result.Append("[");
  result.AppendNumber(from);
  result.Append(",");
  result.AppendNumber(to);
  result.Append(")");
  return result.ToString();
}

String HighlightEdge::ToString() const {
  StringBuilder result{};
  result.AppendNumber(Offset());
  result.Append(type == HighlightEdgeType::kStart ? "<" : ">");
  result.Append(layer.ToString());
  return result.ToString();
}

unsigned HighlightEdge::Offset() const {
  switch (type) {
    case HighlightEdgeType::kStart:
      return range.from;
    case HighlightEdgeType::kEnd:
      return range.to;
  }
}

bool HighlightEdge::LessThan(const HighlightEdge& other,
                             const HighlightRegistry& registry) const {
  if (Offset() < other.Offset()) {
    return true;
  }
  if (Offset() > other.Offset()) {
    return false;
  }
  if (type > other.type)
    return true;
  if (type < other.type)
    return false;
  return layer.ComparePaintOrder(other.layer, registry) < 0;
}

bool HighlightEdge::operator==(const HighlightEdge& other) const {
  return Offset() == other.Offset() && type == other.type &&
         layer == other.layer;
}

bool HighlightEdge::operator!=(const HighlightEdge& other) const {
  return !operator==(other);
}

HighlightDecoration::HighlightDecoration(HighlightLayer layer,
                                         HighlightRange range)
    : layer(layer), range(range) {}

String HighlightDecoration::ToString() const {
  StringBuilder result{};
  result.Append(layer.ToString());
  result.Append(range.ToString());
  return result.ToString();
}

bool HighlightDecoration::operator==(const HighlightDecoration& other) const {
  return layer == other.layer && range == other.range;
}

bool HighlightDecoration::operator!=(const HighlightDecoration& other) const {
  return !operator==(other);
}

HighlightPart::HighlightPart(HighlightLayer layer,
                             HighlightRange range,
                             Vector<HighlightDecoration> decorations)
    : layer(layer), range(range), decorations(decorations) {}

HighlightPart::HighlightPart(HighlightLayer layer, HighlightRange range)
    : HighlightPart(layer, range, Vector<HighlightDecoration>{}) {}

String HighlightPart::ToString() const {
  StringBuilder result{};
  result.Append(layer.ToString());
  result.Append(range.ToString());
  for (const HighlightDecoration& decoration : decorations) {
    result.Append("+");
    result.Append(decoration.ToString());
  }
  return result.ToString();
}

bool HighlightPart::operator==(const HighlightPart& other) const {
  return layer == other.layer && range == other.range &&
         decorations == other.decorations;
}

bool HighlightPart::operator!=(const HighlightPart& other) const {
  return !operator==(other);
}

Vector<HighlightLayer> NGHighlightOverlay::ComputeLayers(
    const HighlightRegistry* registry,
    const LayoutSelectionStatus* selection,
    const DocumentMarkerVector& custom,
    const DocumentMarkerVector& grammar,
    const DocumentMarkerVector& spelling,
    const DocumentMarkerVector& target) {
  DCHECK(RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled());

  Vector<HighlightLayer> result{};
  result.emplace_back(HighlightLayerType::kOriginating);

  for (const auto& marker : custom) {
    auto* custom_marker = To<CustomHighlightMarker>(marker.Get());
    HighlightLayer layer{HighlightLayerType::kCustom,
                         custom_marker->GetHighlightName()};
    if (!result.Contains(layer))
      result.push_back(layer);
  }
  if (!grammar.empty())
    result.emplace_back(HighlightLayerType::kGrammar);
  if (!spelling.empty())
    result.emplace_back(HighlightLayerType::kSpelling);
  if (!target.empty())
    result.emplace_back(HighlightLayerType::kTargetText);
  if (selection)
    result.emplace_back(HighlightLayerType::kSelection);

  std::sort(result.begin(), result.end(),
            [registry](const HighlightLayer& p, const HighlightLayer& q) {
              return p.ComparePaintOrder(q, *registry) < 0;
            });

  return result;
}

Vector<HighlightEdge> NGHighlightOverlay::ComputeEdges(
    const Node* node,
    const HighlightRegistry* registry,
    bool is_generated_text_fragment,
    const NGTextFragmentPaintInfo& originating,
    const LayoutSelectionStatus* selection,
    const DocumentMarkerVector& custom,
    const DocumentMarkerVector& grammar,
    const DocumentMarkerVector& spelling,
    const DocumentMarkerVector& target) {
  DCHECK(RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled());

  Vector<HighlightEdge> result{};

  if (selection) {
    DCHECK_LT(selection->start, selection->end);
    result.emplace_back(HighlightRange{selection->start, selection->end},
                        HighlightLayer{HighlightLayerType::kSelection},
                        HighlightEdgeType::kStart);
    result.emplace_back(HighlightRange{selection->start, selection->end},
                        HighlightLayer{HighlightLayerType::kSelection},
                        HighlightEdgeType::kEnd);
  }

  // |node| might not be a Text node (e.g. <br>), or it might be nullptr (e.g.
  // ::first-letter). In both cases, we should still try to paint kOriginating
  // and kSelection if necessary, but we can’t paint marker-based highlights,
  // because GetTextContentOffset requires a Text node. Markers are defined and
  // stored in terms of Text nodes anyway, so this check should never fail.
  const auto* text_node = DynamicTo<Text>(node);
  if (!text_node) {
    DCHECK(custom.empty() && grammar.empty() && spelling.empty() &&
           target.empty())
        << "markers can not be painted without a valid Text node";
  } else if (is_generated_text_fragment) {
    // Custom highlights and marker-based highlights are defined in terms of
    // DOM ranges in a Text node. Generated text either has no Text node or does
    // not derive its content from the Text node (e.g. ellipsis, soft hyphens).
    // TODO(crbug.com/17528) handle ::first-letter
    DCHECK(custom.empty() && grammar.empty() && spelling.empty() &&
           target.empty())
        << "no marker can ever apply to fragment items with generated text";
  } else {
    // We can save time by skipping marker-based highlights that are outside the
    // originating fragment (e.g. on a different line), but we can only compare
    // offsets that are in the same offset space (DOM or canonical text), and
    // converting each marker to canonical text offsets is the most expensive
    // step of this function. We can avoid that by converting the originating
    // fragment back to DOM offsets for comparison.
    const NGOffsetMapping* mapping =
        NGOffsetMapping::GetFor(text_node->GetLayoutObject());
    unsigned last_from =
        mapping->GetLastPosition(originating.from).OffsetInContainerNode();
    unsigned first_to =
        mapping->GetFirstPosition(originating.to).OffsetInContainerNode();

    for (const auto& marker : custom) {
      if (marker->EndOffset() <= last_from || marker->StartOffset() >= first_to)
        continue;
      auto* custom_marker = To<CustomHighlightMarker>(marker.Get());
      unsigned content_start =
          GetTextContentOffset(*text_node, marker->StartOffset());
      unsigned content_end =
          GetTextContentOffset(*text_node, marker->EndOffset());
      if (content_start >= content_end)
        continue;
      result.emplace_back(HighlightRange{content_start, content_end},
                          HighlightLayer{HighlightLayerType::kCustom,
                                         custom_marker->GetHighlightName()},
                          HighlightEdgeType::kStart);
      result.emplace_back(HighlightRange{content_start, content_end},
                          HighlightLayer{HighlightLayerType::kCustom,
                                         custom_marker->GetHighlightName()},
                          HighlightEdgeType::kEnd);
    }

    for (const auto& marker : grammar) {
      if (marker->EndOffset() <= last_from || marker->StartOffset() >= first_to)
        continue;
      unsigned content_start =
          GetTextContentOffset(*text_node, marker->StartOffset());
      unsigned content_end =
          GetTextContentOffset(*text_node, marker->EndOffset());
      if (content_start >= content_end)
        continue;
      result.emplace_back(HighlightRange{content_start, content_end},
                          HighlightLayer{HighlightLayerType::kGrammar},
                          HighlightEdgeType::kStart);
      result.emplace_back(HighlightRange{content_start, content_end},
                          HighlightLayer{HighlightLayerType::kGrammar},
                          HighlightEdgeType::kEnd);
    }

    for (const auto& marker : spelling) {
      if (marker->EndOffset() <= last_from || marker->StartOffset() >= first_to)
        continue;
      unsigned content_start =
          GetTextContentOffset(*text_node, marker->StartOffset());
      unsigned content_end =
          GetTextContentOffset(*text_node, marker->EndOffset());
      if (content_start >= content_end)
        continue;
      result.emplace_back(HighlightRange{content_start, content_end},
                          HighlightLayer{HighlightLayerType::kSpelling},
                          HighlightEdgeType::kStart);
      result.emplace_back(HighlightRange{content_start, content_end},
                          HighlightLayer{HighlightLayerType::kSpelling},
                          HighlightEdgeType::kEnd);
    }

    for (const auto& marker : target) {
      if (marker->EndOffset() <= last_from || marker->StartOffset() >= first_to)
        continue;
      unsigned content_start =
          GetTextContentOffset(*text_node, marker->StartOffset());
      unsigned content_end =
          GetTextContentOffset(*text_node, marker->EndOffset());
      if (content_start >= content_end)
        continue;
      result.emplace_back(HighlightRange{content_start, content_end},
                          HighlightLayer{HighlightLayerType::kTargetText},
                          HighlightEdgeType::kStart);
      result.emplace_back(HighlightRange{content_start, content_end},
                          HighlightLayer{HighlightLayerType::kTargetText},
                          HighlightEdgeType::kEnd);
    }
  }

  std::sort(result.begin(), result.end(),
            [registry](const HighlightEdge& p, const HighlightEdge& q) {
              return p.LessThan(q, *registry);
            });

  return result;
}

Vector<HighlightPart> NGHighlightOverlay::ComputeParts(
    const NGTextFragmentPaintInfo& originating,
    const Vector<HighlightLayer>& layers,
    const Vector<HighlightEdge>& edges) {
  DCHECK(RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled());
  const HighlightLayer originating_layer{HighlightLayerType::kOriginating};
  const HighlightDecoration originating_decoration{
      originating_layer, {originating.from, originating.to}};
  Vector<HighlightPart> result{};
  Vector<absl::optional<HighlightRange>> active(layers.size());
  absl::optional<unsigned> prev_offset{};
  if (edges.empty()) {
    result.push_back(HighlightPart{originating_layer,
                                   {originating.from, originating.to},
                                   {originating_decoration}});
    return result;
  }
  if (originating.from < edges.front().Offset()) {
    result.push_back(HighlightPart{
        originating_layer,
        {originating.from, ClampOffset(edges.front().Offset(), originating)},
        {originating_decoration}});
  }
  for (const HighlightEdge& edge : edges) {
    // If there is actually some text between the previous and current edges...
    if (prev_offset.has_value() && *prev_offset < edge.Offset()) {
      // ...and the range overlaps with the fragment being painted...
      unsigned part_from = ClampOffset(*prev_offset, originating);
      unsigned part_to = ClampOffset(edge.Offset(), originating);
      if (part_from < part_to) {
        // ...then find the topmost layer and enqueue a new part to be painted.
        HighlightPart part{
            originating_layer, {part_from, part_to}, {originating_decoration}};
        for (wtf_size_t i = 0; i < layers.size(); i++) {
          if (active[i]) {
            unsigned decoration_from =
                ClampOffset(active[i]->from, originating);
            unsigned decoration_to = ClampOffset(active[i]->to, originating);
            part.layer = layers[i];
            part.decorations.push_back(HighlightDecoration{
                layers[i], {decoration_from, decoration_to}});
          }
        }
        result.push_back(part);
      }
    }
    wtf_size_t edge_layer_index = layers.Find(edge.layer);
    DCHECK_NE(edge_layer_index, kNotFound)
        << "edge layer should be one of the given layers";
    // This algorithm malfunctions if the edges represent overlapping ranges.
    DCHECK(active[edge_layer_index] ? edge.type == HighlightEdgeType::kEnd
                                    : edge.type == HighlightEdgeType::kStart)
        << "edge should be kStart iff the layer is active or else kEnd";
    if (edge.type == HighlightEdgeType::kStart) {
      active[edge_layer_index].emplace(edge.range);
    } else {
      active[edge_layer_index].reset();
    }
    prev_offset.emplace(edge.Offset());
  }
  if (edges.back().Offset() < originating.to) {
    result.push_back(HighlightPart{
        originating_layer,
        {ClampOffset(edges.back().Offset(), originating), originating.to},
        {originating_decoration}});
  }
  return result;
}

std::ostream& operator<<(std::ostream& result,
                         const NGHighlightOverlay::HighlightLayer& layer) {
  return result << layer.ToString().Utf8();
}

std::ostream& operator<<(std::ostream& result,
                         const NGHighlightOverlay::HighlightEdge& edge) {
  return result << edge.ToString().Utf8();
}

std::ostream& operator<<(std::ostream& result,
                         const NGHighlightOverlay::HighlightPart& part) {
  return result << part.ToString().Utf8();
}

}  // namespace blink
