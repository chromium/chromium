// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/video_painter.h"

#include "cc/layers/layer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/image_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"

namespace blink {

void VideoPainter::PaintReplaced(const PaintInfo& paint_info,
                                 const PhysicalOffset& paint_offset) {
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelectionDragImage)
    return;

  WebMediaPlayer* media_player =
      layout_video_.MediaElement()->GetWebMediaPlayer();
  bool force_video_poster =
      layout_video_.GetDocument().GetPaintPreviewState() ==
      Document::kPaintingPreviewSkipAcceleratedContent;
  bool should_display_poster =
      layout_video_.GetDisplayMode() == LayoutVideo::kPoster ||
      force_video_poster;
  if (!should_display_poster && !media_player)
    return;

  PhysicalRect replaced_rect = layout_video_.ReplacedContentRect();
  replaced_rect.Move(paint_offset);
  gfx::Rect snapped_replaced_rect = ToPixelSnappedRect(replaced_rect);

  if (snapped_replaced_rect.IsEmpty())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_video_, paint_info.phase))
    return;

  GraphicsContext& context = paint_info.context;
  // Here we're not painting the video but rather preparing the layer for the
  // compositor to submit video frames. But the compositor will do all the work
  // related to the video moving forward. Therefore we mark the FCP here.
  context.GetPaintController().SetImagePainted();
  PhysicalRect content_box_rect = layout_video_.PhysicalContentBoxRect();
  content_box_rect.Move(paint_offset);

  if (layout_video_.GetDocument().GetPaintPreviewState() !=
      Document::kNotPaintingPreview) {
    // Create a canvas and draw a URL rect to it for the paint preview.
    BoxDrawingRecorder recorder(context, layout_video_, paint_info.phase,
                                paint_offset);
    context.SetURLForRect(layout_video_.GetDocument().Url(),
                          snapped_replaced_rect);
  }

  // Video frames are only painted in software for printing or capturing node
  // images via web APIs.
  bool force_software_video_paint =
      paint_info.ShouldOmitCompositingInfo() && !force_video_poster;

  bool paint_with_foreign_layer = paint_info.phase == PaintPhase::kForeground &&
                                  !should_display_poster &&
                                  !force_software_video_paint;
  if (paint_with_foreign_layer) {
    if (cc::Layer* layer = layout_video_.MediaElement()->CcLayer()) {
      layer->SetBounds(snapped_replaced_rect.size());
      layer->SetIsDrawable(true);
      layer->SetHitTestable(true);
      RecordForeignLayer(context, layout_video_,
                         DisplayItem::kForeignLayerVideo, layer,
                         snapped_replaced_rect.origin());
      return;
    }
  }

  const PhysicalRect visual_rect =
      layout_video_.ClipsToContentBox() ? content_box_rect : replaced_rect;

  DrawingRecorder recorder(context, layout_video_, paint_info.phase,
                           ToEnclosingRect(visual_rect));

  if (should_display_poster || !force_software_video_paint) {
    // This will display the poster image, if one is present, and otherwise
    // paint nothing.

    ImagePainter(layout_video_)
        .PaintIntoRect(context, replaced_rect, visual_rect);
  } else {
    cc::PaintFlags video_flags = context.FillFlags();
    video_flags.setColor(SK_ColorBLACK);
    layout_video_.VideoElement()->PaintCurrentFrame(
        context.Canvas(), snapped_replaced_rect, &video_flags);
  }
}

}  // namespace blink
