// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_EMBEDDED_CONTENT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_EMBEDDED_CONTENT_PAINTER_H_

#include "third_party/blink/renderer/platform/geometry/physical_offset.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Document;
class EffectPaintPropertyNodeOrAlias;
class LayoutEmbeddedContent;
class ScopedPaintChunkProperties;
struct PaintInfo;

class EmbeddedContentPainter {
  STACK_ALLOCATED();

 public:
  EmbeddedContentPainter(const LayoutEmbeddedContent& layout_embedded_content)
      : layout_embedded_content_(layout_embedded_content) {}

  void PaintReplaced(const PaintInfo&, const PhysicalOffset& paint_offset);

  // Strips SVG reference filter effects when painting out-of-process frames
  // (`RemoteFrameView`) and plugins (`WebPluginContainerImpl`), and records the
  // deprecation counter for all cross-origin embedded content (including
  // in-process cross-origin `LocalFrameView`s, whose filters are already
  // stripped during PrePaint in
  // `PaintPropertyTreeBuilder::SetupContextForFrame`).
  [[nodiscard]] static std::optional<ScopedPaintChunkProperties>
  RemoveSvgFilterPaint(const LayoutEmbeddedContent& layout_embedded_content,
                       const PaintInfo& paint_info);

  // View transition content (`DisplayItem::kForeignLayerViewTransitionContent`
  // snapshots and live content) is produced by the compositor and may contain
  // the pixels of cross-origin frames and plugins, so, as with
  // `RemoveSvgFilterPaint`, it must not be painted under an SVG reference
  // filter. Whether the content actually contains such pixels isn't tracked
  // (e.g. the old snapshot of a cross-document transition comes from the
  // previous document), so this applies to all view transition content.
  // Returns the effect to paint such content with instead of `effect`: the
  // parent of the highest effect with a reference filter in the chain of
  // `effect`, or `effect` itself if there is none. Reports the
  // ViewTransitionPreventSvgFilterPaint deprecation to `document` if a filter
  // is removed.
  //
  // TODO(crbug.com/568974861): Only do this for content that contains
  // cross-origin iframes (or plugins), to reduce the compat impact. This needs
  // to be tracked from capture time, since such frames can be removed before
  // the content is painted.
  [[nodiscard]] static const EffectPaintPropertyNodeOrAlias&
  EffectForViewTransitionContent(const EffectPaintPropertyNodeOrAlias& effect,
                                 Document& document);

 private:
  const LayoutEmbeddedContent& layout_embedded_content_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_EMBEDDED_CONTENT_PAINTER_H_
