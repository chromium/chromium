// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_filter_painter.h"

#include <utility>

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_filter.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/filter_effect_builder.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/svg_filter_element.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/filters/source_graphic.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

GraphicsContext* SVGFilterRecordingContext::BeginContent() {
  // Create a new context so the contents of the filter can be drawn and cached.
  paint_controller_ = std::make_unique<PaintController>();
  context_ = std::make_unique<GraphicsContext>(*paint_controller_);

  // Use initial_context_'s current paint chunk properties so that any new
  // chunk created during painting the content will be in the correct state.
  paint_controller_->UpdateCurrentPaintChunkProperties(
      base::nullopt,
      initial_context_.GetPaintController().CurrentPaintChunkProperties());

  return context_.get();
}

sk_sp<PaintRecord> SVGFilterRecordingContext::EndContent(
    const FloatRect& bounds) {
  // Use the context that contains the filtered content.
  DCHECK(paint_controller_);
  DCHECK(context_);
  context_->BeginRecording(bounds);
  paint_controller_->CommitNewDisplayItems();

  paint_controller_->GetPaintArtifact().Replay(
      *context_,
      initial_context_.GetPaintController().CurrentPaintChunkProperties());

  sk_sp<PaintRecord> content = context_->EndRecording();
  // Content is cached by the source graphic so temporaries can be freed.
  paint_controller_ = nullptr;
  context_ = nullptr;
  return content;
}

void SVGFilterRecordingContext::Abort() {
  if (!paint_controller_)
    return;
  EndContent(FloatRect());
}

static void PaintFilteredContent(GraphicsContext& context,
                                 const LayoutObject& object,
                                 const FloatRect& bounds,
                                 FilterEffect* effect) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, object,
                                                  DisplayItem::kSVGFilter))
    return;

  DrawingRecorder recorder(context, object, DisplayItem::kSVGFilter);
  sk_sp<PaintFilter> image_filter =
      paint_filter_builder::Build(effect, kInterpolationSpaceSRGB);
  context.Save();

  // Clip drawing of filtered image to the minimum required paint rect.
  context.ClipRect(effect->MapRect(object.StrokeBoundingBox()));

  context.BeginLayer(1, SkBlendMode::kSrcOver, &bounds, kColorFilterNone,
                     std::move(image_filter));
  context.EndLayer();
  context.Restore();
}

GraphicsContext* SVGFilterPainter::PrepareEffect(
    const LayoutObject& object,
    SVGFilterRecordingContext& recording_context) {
  filter_.ClearInvalidationMask();

  SVGResourceClient* client = SVGResources::GetClient(object);
  if (FilterData* filter_data = filter_.GetFilterDataForClient(client)) {
    // If the filterData already exists we do not need to record the content
    // to be filtered. This can occur if the content was previously recorded
    // or we are in a cycle.
    if (filter_data->state_ == FilterData::kPaintingFilter)
      filter_data->state_ = FilterData::kPaintingFilterCycleDetected;

    if (filter_data->state_ == FilterData::kRecordingContent)
      filter_data->state_ = FilterData::kRecordingContentCycleDetected;

    return nullptr;
  }

  auto* node_map = MakeGarbageCollected<SVGFilterGraphNodeMap>();
  FilterEffectBuilder builder(object.ObjectBoundingBox(), 1);
  Filter* filter = builder.BuildReferenceFilter(
      ToSVGFilterElement(*filter_.GetElement()), nullptr, node_map);
  if (!filter || !filter->LastEffect())
    return nullptr;

  IntRect source_region = EnclosingIntRect(
      Intersection(filter->FilterRegion(), object.StrokeBoundingBox()));
  filter->GetSourceGraphic()->SetSourceRect(source_region);

  auto* filter_data = MakeGarbageCollected<FilterData>();
  filter_data->last_effect = filter->LastEffect();
  filter_data->node_map = node_map;
  DCHECK_EQ(filter_data->state_, FilterData::kInitial);

  // TODO(pdr): Can this be moved out of painter?
  filter_.SetFilterDataForClient(client, filter_data);
  filter_data->state_ = FilterData::kRecordingContent;
  return recording_context.BeginContent();
}

void SVGFilterPainter::FinishEffect(
    const LayoutObject& object,
    SVGFilterRecordingContext& recording_context) {
  SVGResourceClient* client = SVGResources::GetClient(object);
  FilterData* filter_data = filter_.GetFilterDataForClient(client);
  if (!filter_data) {
    // Our state was torn down while we were being painted (selection style for
    // <text> can have this effect), or it was never created (invalid filter.)
    // In the former case we may have been in the process of recording content,
    // so make sure we put recording state into a consistent state.
    recording_context.Abort();
    return;
  }

  // A painting cycle can occur when an FeImage references a source that
  // makes use of the FeImage itself. This is the first place we would hit
  // the cycle so we reset the state and continue.
  if (filter_data->state_ == FilterData::kPaintingFilterCycleDetected) {
    filter_data->state_ = FilterData::kPaintingFilter;
    return;
  }
  if (filter_data->state_ == FilterData::kRecordingContentCycleDetected) {
    filter_data->state_ = FilterData::kRecordingContent;
    return;
  }

  // Check for RecordingContent here because we may can be re-painting
  // without re-recording the contents to be filtered.
  Filter* filter = filter_data->last_effect->GetFilter();
  FloatRect bounds = filter->FilterRegion();
  if (filter_data->state_ == FilterData::kRecordingContent) {
    DCHECK(filter->GetSourceGraphic());
    sk_sp<PaintRecord> content = recording_context.EndContent(bounds);
    paint_filter_builder::BuildSourceGraphic(filter->GetSourceGraphic(),
                                             std::move(content), bounds);
    filter_data->state_ = FilterData::kReadyToPaint;
  }

  DCHECK_EQ(filter_data->state_, FilterData::kReadyToPaint);
  filter_data->state_ = FilterData::kPaintingFilter;
  PaintFilteredContent(recording_context.PaintingContext(), object, bounds,
                       filter_data->last_effect);
  filter_data->state_ = FilterData::kReadyToPaint;
}

}  // namespace blink
