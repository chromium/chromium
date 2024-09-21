// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/highlight_overlay.h"

#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/highlight/highlight_registry.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/paint/marker_range_mapping_context.h"
#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

using HighlightLayerType = HighlightOverlay::HighlightLayerType;
using HighlightLayer = HighlightOverlay::HighlightLayer;
using HighlightRange = HighlightOverlay::HighlightRange;
using HighlightEdge = HighlightOverlay::HighlightEdge;
using HighlightDecoration = HighlightOverlay::HighlightDecoration;
using HighlightBackground = HighlightOverlay::HighlightBackground;
using HighlightTextShadow = HighlightOverlay::HighlightTextShadow;
using HighlightPart = HighlightOverlay::HighlightPart;

unsigned ClampOffset(unsigned offset, const TextFragmentPaintInfo& fragment) {
  return std::min(std::max(offset, fragment.from), fragment.to);
}

String HighlightTypeToString(HighlightLayerType type) {
  StringBuilder result{};
  switch (type) {
    case HighlightLayerType::kOriginating:
      result.Append("originating");
      break;
    case HighlightLayerType::kCustom:
      result.Append("custom");
      break;
    case HighlightLayerType::kGrammar:
      result.Append("grammar");
      break;
    case HighlightLayerType::kSpelling:
      result.Append("spelling");
      break;
    case HighlightLayerType::kTargetText:
      result.Append("target");
      break;
    case HighlightLayerType::kSearchText:
      result.Append("search");
      break;
    case HighlightLayerType::kSearchTextCurrent:
      result.Append("search:current");
      break;
    case HighlightLayerType::kSelection:
      result.Append("selection");
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return result.ToString();
}

uint16_t HighlightLayerIndex(const HeapVector<HighlightLayer>& layers,
                             HighlightLayerType type,
                             const AtomicString& name = g_null_atom) {
  // This may be a performance bottleneck when there are many layers,
  // the solution being to keep a Map in addition to the Vector. But in
  // practice it's hard to see a document using more than tens of custom
  // highlights.
  wtf_size_t index = 0;
  wtf_size_t layers_size = layers.size();
  while (index < layers_size &&
         (layers[index].type != type || layers[index].name != name)) {
    index++;
  }
  CHECK_LT(index, layers_size);
  return static_cast<uint16_t>(index);
}

}  // namespace

HighlightLayer::HighlightLayer(HighlightLayerType type,
                               const AtomicString& name)
    : type(type),
      name(std::move(name)) {}

String HighlightLayer::ToString() const {
  StringBuilder result{};
  result.Append(HighlightTypeToString(type));
  if (!name.IsNull()) {
    result.Append("(");
    result.Append(name);
    result.Append(")");
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
    case HighlightLayerType::kSearchText:
      return kPseudoIdSearchText;
    case HighlightLayerType::kSearchTextCurrent:
      return kPseudoIdSearchText;
    case HighlightLayerType::kSelection:
      return kPseudoIdSelection;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

const AtomicString& HighlightLayer::PseudoArgument() const {
  return name;
}

bool HighlightLayer::operator==(const HighlightLayer& other) const {
  // For equality we are not concerned with the styles or decorations.
  // Those are dependent on the type and name.
  return type == other.type && name == other.name;
}

bool HighlightLayer::operator!=(const HighlightLayer& other) const {
  return !operator==(other);
}

int8_t HighlightLayer::ComparePaintOrder(
    const HighlightLayer& other,
    const HighlightRegistry* registry) const {
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
  DCHECK(registry);
  const HighlightRegistryMap& map = registry->GetHighlights();
  auto* this_entry =
      map.Find<HighlightRegistryMapEntryNameTranslator>(PseudoArgument())
          ->Get();
  auto* other_entry =
      map.Find<HighlightRegistryMapEntryNameTranslator>(other.PseudoArgument())
          ->Get();
  return registry->CompareOverlayStackingPosition(
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
  if (edge_type == HighlightEdgeType::kStart) {
    result.Append("<");
    result.AppendNumber(Offset());
    result.Append(" ");
  }
  result.AppendNumber(layer_index);
  result.Append(":");
  result.Append(HighlightTypeToString(layer_type));
  if (edge_type == HighlightEdgeType::kEnd) {
    result.Append(" ");
    result.AppendNumber(Offset());
    result.Append(">");
  }
  return result.ToString();
}

unsigned HighlightEdge::Offset() const {
  switch (edge_type) {
    case HighlightEdgeType::kStart:
      return range.from;
    case HighlightEdgeType::kEnd:
      return range.to;
  }
}

bool HighlightEdge::LessThan(const HighlightEdge& other,
                             const HeapVector<HighlightLayer>& layers,
                             const HighlightRegistry* registry) const {
  if (Offset() < other.Offset()) {
    return true;
  }
  if (Offset() > other.Offset()) {
    return false;
  }
  if (edge_type > other.edge_type) {
    return true;
  }
  if (edge_type < other.edge_type) {
    return false;
  }
  return layers[layer_index].ComparePaintOrder(layers[other.layer_index],
                                               registry) < 0;
}

bool HighlightEdge::operator==(const HighlightEdge& other) const {
  return Offset() == other.Offset() && edge_type == other.edge_type &&
         layer_type == other.layer_type && layer_index == other.layer_index;
}

bool HighlightEdge::operator!=(const HighlightEdge& other) const {
  return !operator==(other);
}

HighlightDecoration::HighlightDecoration(HighlightLayerType type,
                                         uint16_t layer_index,
                                         HighlightRange range,
                                         Color override_color)
    : type(type),
      layer_index(layer_index),
      range(range),
      highlight_override_color(override_color) {}

String HighlightDecoration::ToString() const {
  StringBuilder result{};
  result.AppendNumber(layer_index);
  result.Append(":");
  result.Append(HighlightTypeToString(type));
  result.Append(" ");
  result.Append(range.ToString());
  return result.ToString();
}

bool HighlightDecoration::operator==(const HighlightDecoration& other) const {
  return type == other.type && layer_index == other.layer_index &&
         range == other.range;
}

bool HighlightDecoration::operator!=(const HighlightDecoration& other) const {
  return !operator==(other);
}

String HighlightBackground::ToString() const {
  StringBuilder result{};
  result.AppendNumber(layer_index);
  result.Append(":");
  result.Append(HighlightTypeToString(type));
  result.Append(" ");
  result.Append(color.SerializeAsCSSColor());
  return result.ToString();
}

bool HighlightBackground::operator==(const HighlightBackground& other) const {
  return type == other.type && layer_index == other.layer_index &&
         color == other.color;
}

bool HighlightBackground::operator!=(const HighlightBackground& other) const {
  return !operator==(other);
}

String HighlightTextShadow::ToString() const {
  StringBuilder result{};
  result.AppendNumber(layer_index);
  result.Append(":");
  result.Append(HighlightTypeToString(type));
  result.Append(" ");
  result.Append(current_color.SerializeAsCSSColor());
  return result.ToString();
}

bool HighlightTextShadow::operator==(const HighlightTextShadow& other) const {
  return type == other.type && layer_index == other.layer_index &&
         current_color == other.current_color;
}

bool HighlightTextShadow::operator!=(const HighlightTextShadow& other) const {
  return !operator==(other);
}

HighlightPart::HighlightPart(HighlightLayerType type,
                             uint16_t layer_index,
                             HighlightRange range,
                             TextPaintStyle style,
                             float stroke_width,
                             Vector<HighlightDecoration> decorations,
                             Vector<HighlightBackground> backgrounds,
                             Vector<HighlightTextShadow> text_shadows)
    : type(type),
      layer_index(layer_index),
      range(range),
      style(style),
      stroke_width(stroke_width),
      decorations(std::move(decorations)),
      backgrounds(std::move(backgrounds)),
      text_shadows(std::move(text_shadows)) {}

HighlightPart::HighlightPart(HighlightLayerType type,
                             uint16_t layer_index,
                             HighlightRange range,
                             TextPaintStyle style,
                             float stroke_width,
                             Vector<HighlightDecoration> decorations)
    : type(type),
      layer_index(layer_index),
      range(range),
      style(style),
      stroke_width(stroke_width),
      decorations(std::move(decorations)),
      backgrounds({}),
      text_shadows({}) {}

String HighlightPart::ToString() const {
  StringBuilder result{};
  result.Append("\n");
  result.AppendNumber(layer_index);
  result.Append(":");
  result.Append(HighlightTypeToString(type));
  result.Append(" ");
  result.Append(range.ToString());
  // A part should contain one kOriginating decoration struct, followed by one
  // decoration struct for each active overlay in highlight painting order,
  // along with background and shadow structs for the active overlays only.
  // Stringify the three vectors in a way that keeps the layers aligned.
  if (decorations.size() >= 1) {
    result.Append("\n    decoration ");
    result.Append(decorations[0].ToString());
  }
  wtf_size_t len =
      std::max(std::max(decorations.size(), backgrounds.size() + 1),
               text_shadows.size() + 1) -
      1;
  for (wtf_size_t i = 0; i < len; i++) {
    result.Append("\n  ");
    if (i + 1 < decorations.size()) {
      result.Append("  decoration ");
      result.Append(decorations[i + 1].ToString());
    }
    if (i < backgrounds.size()) {
      result.Append("  background ");
      result.Append(backgrounds[i].ToString());
    }
    if (i < text_shadows.size()) {
      result.Append("  shadow ");
      result.Append(text_shadows[i].ToString());
    }
  }
  return result.ToString();
}

bool HighlightPart::operator==(const HighlightPart& other) const {
  return type == other.type && layer_index == other.layer_index &&
         range == other.range && decorations == other.decorations &&
         backgrounds == other.backgrounds && text_shadows == other.text_shadows;
}

bool HighlightPart::operator!=(const HighlightPart& other) const {
  return !operator==(other);
}

HeapVector<HighlightLayer> HighlightOverlay::ComputeLayers(
    const Document& document,
    Node* node,
    const ComputedStyle& originating_style,
    const TextPaintStyle& originating_text_style,
    const PaintInfo& paint_info,
    const LayoutSelectionStatus* selection,
    const DocumentMarkerVector& custom,
    const DocumentMarkerVector& grammar,
    const DocumentMarkerVector& spelling,
    const DocumentMarkerVector& target,
    const DocumentMarkerVector& search) {
  const HighlightRegistry* registry =
      HighlightRegistry::GetHighlightRegistry(node);
  HeapVector<HighlightLayer> layers{};
  layers.emplace_back(HighlightLayerType::kOriginating);

  const auto* text_node = DynamicTo<Text>(node);
  if (!text_node) {
    DCHECK(custom.empty() && grammar.empty() && spelling.empty() &&
           target.empty() && search.empty())
        << "markers can not be painted without a valid Text node";
    if (selection) {
      layers.emplace_back(HighlightLayerType::kSelection);
    }
    return layers;
  }

  if (!custom.empty()) {
    // We must be able to store the layer index within 16 bits. Enforce
    // that now when making layers.
    unsigned max_custom_layers =
        std::numeric_limits<uint16_t>::max() -
        static_cast<unsigned>(HighlightLayerType::kSelection);
    const HashSet<AtomicString>& active_highlights =
        registry->GetActiveHighlights(*text_node);
    auto highlight_iter = active_highlights.begin();
    unsigned layer_count = 0;
    while (highlight_iter != active_highlights.end() &&
           layer_count < max_custom_layers) {
      HighlightLayer layer{HighlightLayerType::kCustom, *highlight_iter};
      DCHECK(!layers.Contains(layer));
      layers.push_back(layer);
      highlight_iter++;
      layer_count++;
    }
  }
  if (!grammar.empty())
    layers.emplace_back(HighlightLayerType::kGrammar);
  if (!spelling.empty())
    layers.emplace_back(HighlightLayerType::kSpelling);
  if (!target.empty())
    layers.emplace_back(HighlightLayerType::kTargetText);
  if (!search.empty() &&
      RuntimeEnabledFeatures::SearchTextHighlightPseudoEnabled()) {
    layers.emplace_back(HighlightLayerType::kSearchText);
    layers.emplace_back(HighlightLayerType::kSearchTextCurrent);
  }
  if (selection)
    layers.emplace_back(HighlightLayerType::kSelection);

  std::sort(layers.begin(), layers.end(),
            [registry](const HighlightLayer& p, const HighlightLayer& q) {
              return p.ComparePaintOrder(q, registry) < 0;
            });

  layers[0].style = &originating_style;
  layers[0].text_style.style = originating_text_style;
  layers[0].text_style.text_decoration_color =
      originating_style.VisitedDependentColor(
          GetCSSPropertyTextDecorationColor());
  layers[0].decorations_in_effect =
      originating_style.HasAppliedTextDecorations()
          ? originating_style.TextDecorationsInEffect()
          : TextDecorationLine::kNone;
  for (wtf_size_t i = 1; i < layers.size(); i++) {
    layers[i].style =
        layers[i].type == HighlightLayerType::kSearchTextCurrent
            ? originating_style.HighlightData().SearchTextCurrent()
            : HighlightStyleUtils::HighlightPseudoStyle(
                  node, originating_style, layers[i].PseudoId(),
                  layers[i].PseudoArgument());
    layers[i].text_style = HighlightStyleUtils::HighlightPaintingStyle(
        document, originating_style, layers[i].style, node,
        layers[i].PseudoId(), layers[i - 1].text_style.style, paint_info,
        layers[i].type == HighlightLayerType::kSearchTextCurrent
            ? SearchTextIsCurrent::kYes
            : SearchTextIsCurrent::kNo);
    layers[i].decorations_in_effect =
        layers[i].style && layers[i].style->HasAppliedTextDecorations()
            ? layers[i].style->TextDecorationsInEffect()
            : TextDecorationLine::kNone;
  }
  return layers;
}

Vector<HighlightEdge> HighlightOverlay::ComputeEdges(
    const Node* node,
    bool is_generated_text_fragment,
    std::optional<TextOffsetRange> dom_offsets,
    const HeapVector<HighlightLayer>& layers,
    const LayoutSelectionStatus* selection,
    const DocumentMarkerVector& custom,
    const DocumentMarkerVector& grammar,
    const DocumentMarkerVector& spelling,
    const DocumentMarkerVector& target,
    const DocumentMarkerVector& search) {
  const HighlightRegistry* registry =
      HighlightRegistry::GetHighlightRegistry(node);
  Vector<HighlightEdge> result{};

  if (selection) {
    DCHECK_LT(selection->start, selection->end);
    uint16_t layer_index =
        HighlightLayerIndex(layers, HighlightLayerType::kSelection);
    result.emplace_back(HighlightRange{selection->start, selection->end},
                        HighlightLayerType::kSelection, layer_index,
                        HighlightEdgeType::kStart);
    result.emplace_back(HighlightRange{selection->start, selection->end},
                        HighlightLayerType::kSelection, layer_index,
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
           target.empty() && search.empty())
        << "markers can not be painted without a valid Text node";
  } else if (is_generated_text_fragment) {
    // Custom highlights and marker-based highlights are defined in terms of
    // DOM ranges in a Text node. Generated text either has no Text node or does
    // not derive its content from the Text node (e.g. ellipsis, soft hyphens).
    // TODO(crbug.com/17528) handle ::first-letter
    DCHECK(custom.empty() && grammar.empty() && spelling.empty() &&
           target.empty() && search.empty())
        << "no marker can ever apply to fragment items with generated text";
  } else {
    DCHECK(dom_offsets);
    MarkerRangeMappingContext mapping_context(*text_node, *dom_offsets);
    for (const auto& marker : custom) {
      std::optional<TextOffsetRange> marker_offsets =
          mapping_context.GetTextContentOffsets(*marker);
      if (!marker_offsets) {
        continue;
      }
      const unsigned content_start = marker_offsets->start;
      const unsigned content_end = marker_offsets->end;
      if (content_start >= content_end)
        continue;
      auto* custom_marker = To<CustomHighlightMarker>(marker.Get());
      uint16_t layer_index =
          HighlightLayerIndex(layers, HighlightLayerType::kCustom,
                              custom_marker->GetHighlightName());
      result.emplace_back(HighlightRange{content_start, content_end},
                          HighlightLayerType::kCustom, layer_index,
                          HighlightEdgeType::kStart);
      result.emplace_back(HighlightRange{content_start, content_end},
                          HighlightLayerType::kCustom, layer_index,
                          HighlightEdgeType::kEnd);
    }

    if (!grammar.empty()) {
      mapping_context.Reset();
      uint16_t layer_index =
          HighlightLayerIndex(layers, HighlightLayerType::kGrammar);
      for (const auto& marker : grammar) {
        std::optional<TextOffsetRange> marker_offsets =
            mapping_context.GetTextContentOffsets(*marker);
        if (!marker_offsets) {
          continue;
        }
        const unsigned content_start = marker_offsets->start;
        const unsigned content_end = marker_offsets->end;
        if (content_start >= content_end) {
          continue;
        }
        result.emplace_back(HighlightRange{content_start, content_end},
                            HighlightLayerType::kGrammar, layer_index,
                            HighlightEdgeType::kStart);
        result.emplace_back(HighlightRange{content_start, content_end},
                            HighlightLayerType::kGrammar, layer_index,
                            HighlightEdgeType::kEnd);
      }
    }
    if (!spelling.empty()) {
      mapping_context.Reset();
      uint16_t layer_index =
          HighlightLayerIndex(layers, HighlightLayerType::kSpelling);
      for (const auto& marker : spelling) {
        std::optional<TextOffsetRange> marker_offsets =
            mapping_context.GetTextContentOffsets(*marker);
        if (!marker_offsets) {
          continue;
        }
        const unsigned content_start = marker_offsets->start;
        const unsigned content_end = marker_offsets->end;
        if (content_start >= content_end) {
          continue;
        }
        result.emplace_back(HighlightRange{content_start, content_end},
                            HighlightLayerType::kSpelling, layer_index,
                            HighlightEdgeType::kStart);
        result.emplace_back(HighlightRange{content_start, content_end},
                            HighlightLayerType::kSpelling, layer_index,
                            HighlightEdgeType::kEnd);
      }
    }
    if (!target.empty()) {
      mapping_context.Reset();
      uint16_t layer_index =
          HighlightLayerIndex(layers, HighlightLayerType::kTargetText);
      for (const auto& marker : target) {
        std::optional<TextOffsetRange> marker_offsets =
            mapping_context.GetTextContentOffsets(*marker);
        if (!marker_offsets) {
          continue;
        }
        const unsigned content_start = marker_offsets->start;
        const unsigned content_end = marker_offsets->end;
        if (content_start >= content_end) {
          continue;
        }
        result.emplace_back(HighlightRange{content_start, content_end},
                            HighlightLayerType::kTargetText, layer_index,
                            HighlightEdgeType::kStart);
        result.emplace_back(HighlightRange{content_start, content_end},
                            HighlightLayerType::kTargetText, layer_index,
                            HighlightEdgeType::kEnd);
      }
    }
    if (!search.empty() &&
        RuntimeEnabledFeatures::SearchTextHighlightPseudoEnabled()) {
      mapping_context.Reset();
      uint16_t layer_index_not_current =
          HighlightLayerIndex(layers, HighlightLayerType::kSearchText);
      uint16_t layer_index_current =
          HighlightLayerIndex(layers, HighlightLayerType::kSearchTextCurrent);
      for (const auto& marker : search) {
        std::optional<TextOffsetRange> marker_offsets =
            mapping_context.GetTextContentOffsets(*marker);
        if (!marker_offsets) {
          continue;
        }
        const unsigned content_start = marker_offsets->start;
        const unsigned content_end = marker_offsets->end;
        if (content_start >= content_end) {
          continue;
        }
        auto* text_match_marker = To<TextMatchMarker>(marker.Get());
        HighlightLayerType type = text_match_marker->IsActiveMatch()
                                      ? HighlightLayerType::kSearchTextCurrent
                                      : HighlightLayerType::kSearchText;
        uint16_t layer_index = text_match_marker->IsActiveMatch()
                                   ? layer_index_current
                                   : layer_index_not_current;
        result.emplace_back(HighlightRange{content_start, content_end}, type,
                            layer_index, HighlightEdgeType::kStart);
        result.emplace_back(HighlightRange{content_start, content_end}, type,
                            layer_index, HighlightEdgeType::kEnd);
      }
    }
  }

  std::sort(result.begin(), result.end(),
            [layers, registry](const HighlightEdge& p, const HighlightEdge& q) {
              return p.LessThan(q, layers, registry);
            });

  return result;
}

HeapVector<HighlightPart> HighlightOverlay::ComputeParts(
    const TextFragmentPaintInfo& content_offsets,
    const HeapVector<HighlightLayer>& layers,
    const Vector<HighlightEdge>& edges) {
  DCHECK_EQ(layers[0].type, HighlightLayerType::kOriginating);
  const float originating_stroke_width =
      layers[0].style ? layers[0].style->TextStrokeWidth() : 0;
  const HighlightStyleUtils::HighlightTextPaintStyle& originating_text_style =
      layers[0].text_style;
  const HighlightDecoration originating_decoration{
      HighlightLayerType::kOriginating,
      0,
      {content_offsets.from, content_offsets.to},
      originating_text_style.text_decoration_color};

  HeapVector<HighlightPart> result;
  Vector<std::optional<HighlightRange>> active(layers.size());
  std::optional<unsigned> prev_offset{};
  if (edges.empty()) {
    result.push_back(HighlightPart{HighlightLayerType::kOriginating,
                                   0,
                                   {content_offsets.from, content_offsets.to},
                                   originating_text_style.style,
                                   originating_stroke_width,
                                   {originating_decoration}});
    return result;
  }
  if (content_offsets.from < edges.front().Offset()) {
    result.push_back(
        HighlightPart{HighlightLayerType::kOriginating,
                      0,
                      {content_offsets.from,
                       ClampOffset(edges.front().Offset(), content_offsets)},
                      originating_text_style.style,
                      originating_stroke_width,
                      {originating_decoration}});
  }
  for (const HighlightEdge& edge : edges) {
    // If there is actually some text between the previous and current edges...
    if (prev_offset.has_value() && *prev_offset < edge.Offset()) {
      // ...and the range overlaps with the fragment being painted...
      unsigned part_from = ClampOffset(*prev_offset, content_offsets);
      unsigned part_to = ClampOffset(edge.Offset(), content_offsets);
      if (part_from < part_to) {
        // ...then find the topmost layer and enqueue a new part to be painted.
        HighlightPart part{HighlightLayerType::kOriginating,
                           0,
                           {part_from, part_to},
                           originating_text_style.style,
                           originating_stroke_width,
                           {originating_decoration},
                           {},
                           {}};
        HighlightStyleUtils::HighlightTextPaintStyle previous_layer_style =
            originating_text_style;
        for (wtf_size_t i = 1; i < layers.size(); i++) {
          if (active[i]) {
            unsigned decoration_from =
                ClampOffset(active[i]->from, content_offsets);
            unsigned decoration_to =
                ClampOffset(active[i]->to, content_offsets);
            part.type = layers[i].type;
            part.layer_index = static_cast<uint16_t>(i);
            HighlightStyleUtils::HighlightTextPaintStyle part_style =
                layers[i].text_style;
            HighlightStyleUtils::ResolveColorsFromPreviousLayer(
                part_style, previous_layer_style);
            part.style = part_style.style;
            part.decorations.push_back(
                HighlightDecoration{layers[i].type,
                                    static_cast<uint16_t>(i),
                                    {decoration_from, decoration_to},
                                    part_style.text_decoration_color});
            part.backgrounds.push_back(
                HighlightBackground{layers[i].type, static_cast<uint16_t>(i),
                                    part_style.background_color});
            part.text_shadows.push_back(
                HighlightTextShadow{layers[i].type, static_cast<uint16_t>(i),
                                    part_style.style.current_color});
            previous_layer_style = part_style;
          }
        }
        result.push_back(part);
      }
    }
    // This algorithm malfunctions if the edges represent overlapping ranges.
    DCHECK(active[edge.layer_index]
               ? edge.edge_type == HighlightEdgeType::kEnd
               : edge.edge_type == HighlightEdgeType::kStart)
        << "edge should be kStart iff the layer is active or else kEnd";
    if (edge.edge_type == HighlightEdgeType::kStart) {
      active[edge.layer_index].emplace(edge.range);
    } else {
      active[edge.layer_index].reset();
    }
    prev_offset.emplace(edge.Offset());
  }
  if (edges.back().Offset() < content_offsets.to) {
    result.push_back(
        HighlightPart{HighlightLayerType::kOriginating,
                      0,
                      {ClampOffset(edges.back().Offset(), content_offsets),
                       content_offsets.to},
                      originating_text_style.style,
                      originating_stroke_width,
                      {originating_decoration}});
  }
  return result;
}

std::ostream& operator<<(std::ostream& result,
                         const HighlightOverlay::HighlightLayer& layer) {
  return result << layer.ToString().Utf8();
}

std::ostream& operator<<(std::ostream& result,
                         const HighlightOverlay::HighlightEdge& edge) {
  return result << edge.ToString().Utf8();
}

std::ostream& operator<<(std::ostream& result,
                         const HighlightOverlay::HighlightPart& part) {
  return result << part.ToString().Utf8();
}

}  // namespace blink
