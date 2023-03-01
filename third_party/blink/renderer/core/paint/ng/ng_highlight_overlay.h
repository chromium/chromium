// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_HIGHLIGHT_OVERLAY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_HIGHLIGHT_OVERLAY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/highlight/highlight_registry.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class HighlightRegistry;
struct LayoutSelectionStatus;
struct NGTextFragmentPaintInfo;

class CORE_EXPORT NGHighlightOverlay {
  STATIC_ONLY(NGHighlightOverlay);

 public:
  enum class HighlightLayerType : unsigned {
    kOriginating,
    kCustom,
    kGrammar,
    kSpelling,
    kTargetText,
    kSelection
  };

  enum class HighlightEdgeType : unsigned { kStart, kEnd };

  // Identifies a highlight layer, such as the originating content, one of the
  // highlight pseudos, or a custom highlight (name unique within a registry).
  struct CORE_EXPORT HighlightLayer {
    DISALLOW_NEW();

   public:
    explicit HighlightLayer(HighlightLayerType type,
                            AtomicString name = g_null_atom)
        : type(type), name(name) {}

    String ToString() const;
    enum PseudoId PseudoId() const;
    const AtomicString& PseudoArgument() const;

    int8_t ComparePaintOrder(const HighlightLayer&,
                             const HighlightRegistry&) const;
    bool operator==(const HighlightLayer&) const;
    bool operator!=(const HighlightLayer&) const;

    HighlightLayerType type;
    AtomicString name;
  };

  // Represents a range of the fragment, as offsets in canonical text space.
  // More details about canonical text offsets: <https://goo.gl/CJbxky>
  struct CORE_EXPORT HighlightRange {
    DISALLOW_NEW();

   public:
    HighlightRange(unsigned from, unsigned to);

    String ToString() const;

    bool operator==(const HighlightRange&) const;
    bool operator!=(const HighlightRange&) const;

    unsigned from;
    unsigned to;
  };

  // Represents the start or end (indicated by |type|) of a highlighted |range|
  // for the given |layer|. Storing both offsets of the range, rather than just
  // the offset of this edge, allows decorations added by highlights to recover
  // the original range for the purposes of decoration phase and wavelength.
  struct CORE_EXPORT HighlightEdge {
    DISALLOW_NEW();

   public:
    HighlightEdge(HighlightRange range,
                  HighlightLayer layer,
                  HighlightEdgeType type)
        : range(range), layer(layer), type(type) {}

    String ToString() const;
    unsigned Offset() const;

    // Order by offset asc, then “end” edges first, then by layer paint order.
    // ComputeParts requires “end” edges first in case two ranges of the same
    // highlight are immediately adjacent. The opposite would be required for
    // empty highlight ranges, but they’re illegal as per DocumentMarker ctor.
    bool LessThan(const HighlightEdge&, const HighlightRegistry&) const;
    bool operator==(const HighlightEdge&) const;
    bool operator!=(const HighlightEdge&) const;

    HighlightRange range;
    HighlightLayer layer;
    HighlightEdgeType type;
  };

  // Represents a potential decoration for the given |layer| that would need to
  // be painted over the given |range|.
  //
  // Note that decorations are painted with this range, but clipped to the range
  // in each HighlightPart, ensuring that decoration phase and wavelength are
  // maintained while allowing them to be recolored or split across layers.
  struct CORE_EXPORT HighlightDecoration {
    DISALLOW_NEW();

   public:
    HighlightDecoration(HighlightLayer layer, HighlightRange range);

    String ToString() const;

    bool operator==(const HighlightDecoration&) const;
    bool operator!=(const HighlightDecoration&) const;

    HighlightLayer layer;
    HighlightRange range;
  };

  // Represents a |range| of the fragment that needs its text proper painted in
  // the style of the given topmost |layer| with the given |decorations|.
  //
  // Note that decorations are clipped to this range, but painted with the range
  // in each HighlightDecoration, ensuring that decoration phase and wavelength
  // are maintained while allowing them to be recolored or split across layers.
  struct CORE_EXPORT HighlightPart {
    DISALLOW_NEW();

   public:
    HighlightPart(HighlightLayer, HighlightRange, Vector<HighlightDecoration>);
    HighlightPart(HighlightLayer, HighlightRange);

    String ToString() const;

    bool operator==(const HighlightPart&) const;
    bool operator!=(const HighlightPart&) const;

    HighlightLayer layer;
    HighlightRange range;
    Vector<HighlightDecoration> decorations;
  };

  // Given details of a fragment and how it is highlighted, returns the layers
  // that need to be painted, in overlay painting order.
  static Vector<HighlightLayer> ComputeLayers(
      const HighlightRegistry*,
      const LayoutSelectionStatus* selection,
      const DocumentMarkerVector& custom,
      const DocumentMarkerVector& grammar,
      const DocumentMarkerVector& spelling,
      const DocumentMarkerVector& target);

  // Given details of a fragment and how it is highlighted, returns the start
  // and end transitions (edges) of the layers, in offset and layer order.
  static Vector<HighlightEdge> ComputeEdges(
      const Node*,
      const HighlightRegistry*,
      bool is_generated_text_fragment,
      const NGTextFragmentPaintInfo& originating,
      const LayoutSelectionStatus* selection,
      const DocumentMarkerVector& custom,
      const DocumentMarkerVector& grammar,
      const DocumentMarkerVector& spelling,
      const DocumentMarkerVector& target);

  // Given highlight |layers| and |edges|, returns the ranges of text that can
  // be painted in the same layer with the same decorations, clamping the result
  // to the given |originating| fragment.
  //
  // The edges must not represent overlapping ranges. If the highlight is active
  // in overlapping ranges, those ranges must be merged before ComputeEdges.
  static Vector<HighlightPart> ComputeParts(
      const NGTextFragmentPaintInfo& originating,
      const Vector<HighlightLayer>& layers,
      const Vector<HighlightEdge>& edges);
};

CORE_EXPORT std::ostream& operator<<(std::ostream&,
                                     const NGHighlightOverlay::HighlightLayer&);
CORE_EXPORT std::ostream& operator<<(std::ostream&,
                                     const NGHighlightOverlay::HighlightEdge&);
CORE_EXPORT std::ostream& operator<<(std::ostream&,
                                     const NGHighlightOverlay::HighlightPart&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_HIGHLIGHT_OVERLAY_H_
