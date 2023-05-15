// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame_view_auto_size_info.h"

#include "base/auto_reset.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

FrameViewAutoSizeInfo::FrameViewAutoSizeInfo(LocalFrameView* view)
    : frame_view_(view), in_auto_size_(false), did_run_autosize_(false) {
  DCHECK(frame_view_);
}

void FrameViewAutoSizeInfo::Trace(Visitor* visitor) const {
  visitor->Trace(frame_view_);
}

void FrameViewAutoSizeInfo::ConfigureAutoSizeMode(const gfx::Size& min_size,
                                                  const gfx::Size& max_size) {
  DCHECK(!min_size.IsEmpty());
  DCHECK_LE(min_size.width(), max_size.width());
  DCHECK_LE(min_size.height(), max_size.height());

  if (min_auto_size_ == min_size && max_auto_size_ == max_size)
    return;

  min_auto_size_ = min_size;
  max_auto_size_ = max_size;
  did_run_autosize_ = false;
}

bool FrameViewAutoSizeInfo::AutoSizeIfNeeded() {
  DCHECK(!in_auto_size_);
  base::AutoReset<bool> change_in_auto_size(&in_auto_size_, true);

  Document* document = frame_view_->GetFrame().GetDocument();
  if (!document || !document->IsActive())
    return false;

  Element* document_element = document->documentElement();
  if (!document_element)
    return false;

  // If this is the first time we run autosize, start from small height and
  // allow it to grow.
  gfx::Size size = frame_view_->Size();
  if (!did_run_autosize_) {
    running_first_autosize_ = true;
    did_run_autosize_ = true;
    if (size.height() != min_auto_size_.height()) {
      frame_view_->Resize(size.width(), min_auto_size_.height());
      return true;
    }
  }

  PaintLayerScrollableArea* layout_viewport = frame_view_->LayoutViewport();

  // Do the resizing twice. The first time is basically a rough calculation
  // using the preferred width which may result in a height change during the
  // second iteration.
  if (++num_passes_ > 2u)
    return false;

  auto* layout_view = document->GetLayoutView();
  if (!layout_view)
    return false;

  // TODO(bokan): This code doesn't handle subpixel sizes correctly. Because
  // of that, it's forced to maintain all the special ScrollbarMode code
  // below. https://crbug.com/812311.
  int width = layout_view->ComputeMinimumWidth().ToInt();

  LayoutBox* document_layout_box = document_element->GetLayoutBox();
  if (!document_layout_box)
    return false;

  int height = document_layout_box->ScrollHeight().ToInt();
  gfx::Size new_size(width, height);

  // Check to see if a scrollbar is needed for a given dimension and
  // if so, increase the other dimension to account for the scrollbar.
  // Since the dimensions are only for the view rectangle, once a
  // dimension exceeds the maximum, there is no need to increase it further.
  if (new_size.width() > max_auto_size_.width()) {
    new_size.Enlarge(0, layout_viewport->HypotheticalScrollbarThickness(
                            kHorizontalScrollbar));
    // Don't bother checking for a vertical scrollbar because the width is at
    // already greater the maximum.
  } else if (new_size.height() > max_auto_size_.height() &&
             // If we have a real vertical scrollbar, it's already included in
             // PreferredLogicalWidths(), so don't add a hypothetical one.
             !layout_viewport->HasVerticalScrollbar()) {
    new_size.Enlarge(
        layout_viewport->HypotheticalScrollbarThickness(kVerticalScrollbar), 0);
    // Don't bother checking for a horizontal scrollbar because the height is
    // already greater the maximum.
  }

  // Ensure the size is at least the min bounds.
  new_size.SetToMax(min_auto_size_);

  // Bound the dimensions by the max bounds and determine what scrollbars to
  // show.
  mojom::blink::ScrollbarMode horizontal_scrollbar_mode =
      mojom::blink::ScrollbarMode::kAlwaysOff;
  if (new_size.width() > max_auto_size_.width()) {
    new_size.set_width(max_auto_size_.width());
    horizontal_scrollbar_mode = mojom::blink::ScrollbarMode::kAlwaysOn;
  }
  mojom::blink::ScrollbarMode vertical_scrollbar_mode =
      mojom::blink::ScrollbarMode::kAlwaysOff;
  if (new_size.height() > max_auto_size_.height()) {
    new_size.set_height(max_auto_size_.height());
    vertical_scrollbar_mode = mojom::blink::ScrollbarMode::kAlwaysOn;
  }

  bool change_size = (new_size != size);

  // While loading only allow the size to increase (to avoid twitching during
  // intermediate smaller states) unless autoresize has just been turned on or
  // the maximum size is smaller than the current size.
  if (!running_first_autosize_ && size.height() <= max_auto_size_.height() &&
      size.width() <= max_auto_size_.width() &&
      !frame_view_->GetFrame().GetDocument()->LoadEventFinished() &&
      (new_size.height() < size.height() || new_size.width() < size.width())) {
    change_size = false;
  }

  if (change_size)
    frame_view_->Resize(new_size.width(), new_size.height());

  // Force the scrollbar state to avoid the scrollbar code adding them and
  // causing them to be needed. For example, a vertical scrollbar may cause
  // text to wrap and thus increase the height (which is the only reason the
  // scollbar is needed).
  //
  // Note: since the overflow may have changed, we need to do this even if the
  // size of the frame isn't changing.
  frame_view_->GetLayoutView()->SetAutosizeScrollbarModes(
      horizontal_scrollbar_mode, vertical_scrollbar_mode);

  return change_size;
}

void FrameViewAutoSizeInfo::Clear() {
  if (num_passes_) {
    num_passes_ = 0u;
    running_first_autosize_ = false;
  }
}

}  // namespace blink
