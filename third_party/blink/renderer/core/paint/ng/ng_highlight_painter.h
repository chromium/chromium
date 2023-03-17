// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_HIGHLIGHT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_HIGHLIGHT_PAINTER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/layout/api/selection_state.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/ng/ng_highlight_overlay.h"
#include "third_party/blink/renderer/core/paint/text_decoration_info.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/gc_plugin.h"

namespace blink {

class ComputedStyle;
class FrameSelection;
class LayoutObject;
class NGFragmentItem;
class NGTextPainter;
class NGTextDecorationPainter;
class NGInlineCursor;
class Node;
struct LayoutSelectionStatus;
struct NGTextFragmentPaintInfo;
struct PaintInfo;
struct PhysicalOffset;

// Highlight overlay painter for LayoutNG. Operates on NGFragmentItem that
// IsText(). Delegates to NGTextPainter to paint the text itself.
class CORE_EXPORT NGHighlightPainter {
  STACK_ALLOCATED();

 public:
  class CORE_EXPORT SelectionPaintState {
    STACK_ALLOCATED();

   public:
    explicit SelectionPaintState(
        const NGInlineCursor& containing_block,
        const PhysicalOffset& box_offset,
        const absl::optional<AffineTransform> writing_mode_rotation = {});
    explicit SelectionPaintState(
        const NGInlineCursor& containing_block,
        const PhysicalOffset& box_offset,
        const absl::optional<AffineTransform> writing_mode_rotation,
        const FrameSelection&);

    const LayoutSelectionStatus& Status() const { return selection_status_; }

    const TextPaintStyle& GetSelectionStyle() const { return selection_style_; }

    SelectionState State() const { return state_; }

    bool ShouldPaintSelectedTextOnly() const {
      return paint_selected_text_only_;
    }

    void ComputeSelectionStyle(const Document& document,
                               const ComputedStyle& style,
                               Node* node,
                               const PaintInfo& paint_info,
                               const TextPaintStyle& text_style);

    // When painting text fragments in a vertical writing-mode, we sometimes
    // need to rotate the canvas into a line-relative coordinate space. Paint
    // ops done while rotated need coordinates in this rotated space, but ops
    // done outside of these rotations need the original physical rect.
    const PhysicalRect& RectInPhysicalSpace();
    const PhysicalRect& RectInWritingModeSpace();

    void PaintSelectionBackground(
        GraphicsContext& context,
        Node* node,
        const Document& document,
        const ComputedStyle& style,
        const absl::optional<AffineTransform>& rotation);

    void PaintSelectedText(NGTextPainter& text_painter,
                           const NGTextFragmentPaintInfo&,
                           unsigned length,
                           const TextPaintStyle& text_style,
                           DOMNodeId node_id,
                           const AutoDarkMode& auto_dark_mode);

    void PaintSuppressingTextProperWhereSelected(
        NGTextPainter& text_painter,
        const NGTextFragmentPaintInfo&,
        unsigned length,
        const TextPaintStyle& text_style,
        DOMNodeId node_id,
        const AutoDarkMode& auto_dark_mode);

   private:
    struct SelectionRect {
      PhysicalRect physical;
      PhysicalRect rotated;
      STACK_ALLOCATED();
    };

    // Lazy init |selection_rect_| only when needed, such as when we need to
    // record selection bounds or actually paint the selection. There are many
    // subtle conditions where we won’t ever need this field.
    void ComputeSelectionRectIfNeeded();

    const LayoutSelectionStatus selection_status_;
    const SelectionState state_;
    const NGInlineCursor& containing_block_;
    const PhysicalOffset& box_offset_;
    const absl::optional<AffineTransform> writing_mode_rotation_;
    absl::optional<SelectionRect> selection_rect_;
    TextPaintStyle selection_style_;
    bool paint_selected_text_only_;
  };

  NGHighlightPainter(
      const NGTextFragmentPaintInfo& fragment_paint_info,
      NGTextPainter& text_painter,
      NGTextDecorationPainter& decoration_painter,
      const PaintInfo& paint_info,
      const NGInlineCursor& cursor,
      const NGFragmentItem& fragment_item,
      const absl::optional<AffineTransform> writing_mode_rotation,
      const PhysicalOffset& box_origin,
      const ComputedStyle& style,
      const TextPaintStyle& text_style,
      SelectionPaintState*,
      bool is_printing);

  enum Phase { kBackground, kForeground };

  // Paints backgrounds or foregrounds for markers that are not exposed as CSS
  // highlight pseudos, or all markers if HighlightOverlayPainting is off.
  void Paint(Phase phase);

  // Indicates the way this painter should be used by the caller, aside from
  // the Paint method, which should always be used.
  //
  // The full overlay painting algorithm (kOverlay) is not needed when there
  // are no highlights that change the text color, add backgrounds, or add
  // decorations that are required to paint under decorations from earlier
  // layers (e.g. ::target-text underline with originating overline).
  enum Case {
    // Caller should not use this painter.
    // This happens if nothing is highlighted, or HighlightOverlayPainting is
    // off and nothing is selected.
    kNoHighlights,
    // Caller should use PaintOriginatingText and PaintHighlightOverlays.
    // This happens if HighlightOverlayPainting is on and there are highlights
    // that may involve the text fragment, except in some situations with only
    // spelling/grammar (kFastSpellingGrammar) or selection (kFastSelection).
    kOverlay,
    // Caller should use PaintSelectedText only.
    // This happens if ShouldPaintSelectedTextOnly is true, such as when
    // painting the ::selection drag image.
    kSelectionOnly,
    // Caller should use PaintSuppressingTextProperWhereSelected,
    // PaintSelectionBackground, PaintSelectedText, and NGTextDecorationPainter
    // (which paints highlight decorations incorrectly).
    // This happens if HighlightOverlayPainting is off and the text fragment
    // is selected.
    kOldSelection,
    // Caller should use PaintSuppressingTextProperWhereSelected,
    // PaintSelectionBackground, and PaintSelectedText.
    // This happens if the only highlight that may involve the text fragment
    // is a selection, and neither the selection nor the originating content
    // has any decorations.
    kFastSelection,
    // Caller should use FastPaintSpellingGrammarDecorations.
    // This happens if the only highlights that may involve the text fragment
    // are spelling and/or grammar errors, they are completely unstyled (since
    // the default style only adds a spelling or grammar decoration), and the
    // originating content has no decorations.
    kFastSpellingGrammar
  };
  Case PaintCase() const;

  // PaintCase() == kFastSpellingGrammar only
  void FastPaintSpellingGrammarDecorations();

  // PaintCase() == kOverlay only
  void PaintOriginatingText(const TextPaintStyle&, DOMNodeId);
  void PaintHighlightOverlays(const TextPaintStyle&,
                              DOMNodeId,
                              bool paint_marker_backgrounds,
                              absl::optional<AffineTransform> rotation);

  SelectionPaintState* Selection() { return selection_; }

 private:
  struct LayerPaintState {
    DISALLOW_NEW();

   public:
    LayerPaintState(NGHighlightOverlay::HighlightLayer id,
                    const scoped_refptr<const ComputedStyle> style,
                    TextPaintStyle text_style);

    // Equality on HighlightLayer id only, for Vector::Find.
    bool operator==(const LayerPaintState&) const = delete;
    bool operator!=(const LayerPaintState&) const = delete;
    bool operator==(const NGHighlightOverlay::HighlightLayer&) const;
    bool operator!=(const NGHighlightOverlay::HighlightLayer&) const;

    const NGHighlightOverlay::HighlightLayer id;
    const scoped_refptr<const ComputedStyle> style;
    const TextPaintStyle text_style;
    const TextDecorationLine decorations_in_effect;
  };

  Case ComputePaintCase() const;
  void FastPaintSpellingGrammarDecorations(const Text& text_node,
                                           const StringView& text,
                                           const DocumentMarkerVector& markers);
  void PaintOneSpellingGrammarDecoration(DocumentMarker::MarkerType,
                                         const StringView& text,
                                         unsigned paint_start_offset,
                                         unsigned paint_end_offset);
  void PaintOneSpellingGrammarDecoration(
      DocumentMarker::MarkerType,
      const StringView& text,
      unsigned paint_start_offset,
      unsigned paint_end_offset,
      const ComputedStyle& style,
      const TextPaintStyle& text_style,
      const AppliedTextDecoration* decoration_override);
  PhysicalRect RectInWritingModeSpace(
      const NGHighlightOverlay::HighlightRange&);
  void ClipToPartDecorations(const PhysicalRect&);
  void PaintDecorationsExceptLineThrough(
      const NGHighlightOverlay::HighlightPart&);
  void PaintDecorationsExceptLineThrough(
      const NGHighlightOverlay::HighlightPart&,
      TextDecorationLine lines_to_paint);
  void PaintDecorationsOnlyLineThrough(
      const NGHighlightOverlay::HighlightPart&);
  void PaintSpellingGrammarDecorations(
      const NGHighlightOverlay::HighlightPart&);

  const NGTextFragmentPaintInfo& fragment_paint_info_;
  NGTextPainter& text_painter_;
  NGTextDecorationPainter& decoration_painter_;
  const PaintInfo& paint_info_;
  const NGInlineCursor& cursor_;
  const NGFragmentItem& fragment_item_;
  const PhysicalOffset& box_origin_;
  const ComputedStyle& originating_style_;
  const TextPaintStyle& originating_text_style_;
  SelectionPaintState* selection_;
  const LayoutObject* layout_object_;
  Node* node_;
  const AutoDarkMode foreground_auto_dark_mode_;
  const AutoDarkMode background_auto_dark_mode_;
  DocumentMarkerVector markers_;
  DocumentMarkerVector target_;
  DocumentMarkerVector spelling_;
  DocumentMarkerVector grammar_;
  DocumentMarkerVector custom_;
  Vector<LayerPaintState> layers_;
  Vector<NGHighlightOverlay::HighlightPart> parts_;
  const bool skip_backgrounds_;
  Case paint_case_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_HIGHLIGHT_PAINTER_H_
