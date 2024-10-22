// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_AUTO_SCALER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_AUTO_SCALER_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "content/common/content_export.h"
#include "media/capture/video/video_capture_feedback.h"
#include "ui/gfx/geometry/size.h"

namespace content {

class WebContentsAutoScaler final {
 public:
  // Delegate used to get and set the device scale ratio override for the
  // currently captured tab.
  class CONTENT_EXPORT Delegate {
   public:
    Delegate();
    virtual ~Delegate();
    // Sets the capture scale override for the currently captured view.
    virtual void SetCaptureScaleOverride(float new_value) = 0;
    // Gets the current capture scale override for the currently captured view.
    virtual float GetCaptureScaleOverride() const = 0;
  };

  WebContentsAutoScaler(Delegate& delegate, const gfx::Size& capture_size);
  WebContentsAutoScaler(WebContentsAutoScaler&&) = delete;
  WebContentsAutoScaler(const WebContentsAutoScaler&) = delete;
  ~WebContentsAutoScaler();

  void SetCapturedContentSize(const gfx::Size& content_size);

  // NOTE: the way this report gets applied in this class assumes that it is
  // updated relatively infrequently (e.g. multiple seconds between reports). If
  // we find that this is called more frequently the algorithm should be updated
  // to weigh historic reports as well as the last received feedback.
  void OnUtilizationReport(media::VideoCaptureFeedback feedback);
  float GetDesiredCaptureScaleOverride() const;
  int GetScaleOverrideChangeCount() const;

 private:
  // Sets the new capture scale override via `delegate_`.
  void SetCaptureScaleOverride(float new_value);

  // Determines the preferred DPI scaling factor based on the current content
  // size of the video frame, meaning the populated pixels, and the unscaled
  // current content size, meaning the original size of the frame before scaling
  // was applied to fit the frame. These values are used to compare against
  // the currently requested `capture_size_` set in
  // `WillStartCapturingWebContents()`.
  float CalculatePreferredScaleFactor(
      const gfx::Size& current_content_size,
      const gfx::Size& unscaled_current_content_size);

  // Helper for applying the latest-received utilization feedback to throttle
  // the capture scale override, if necessary.
  float DetermineMaxScaleOverride();

  // The maximum capture scale override.
  static const float kMaxCaptureScaleOverride;

  // Scale multiplier used for the captured content when HiDPI capture mode is
  // active. A value of 1.0 means no override, using the original unmodified
  // resolution. The scale override is a multiplier applied to both the X and Y
  // dimensions, so a value of 2.0 means four times the pixel count. This value
  // tracks the intended scale according to the heuristic. Whenever the value
  // changes, the new scale is immediately applied to the RenderWidgetHostView
  // via SetScaleOverrideForCapture. The value is also saved in this attribute
  // so that it can be undone and/or re-applied when the RenderFrameHost
  // changes.
  //
  // NOTE: the value provided to the captured content is the min of this
  // property and `max_capture_scale_override_`.
  float desired_capture_scale_override_ = 1.0f;

  // Track the number of times the capture scale override mutates in a single
  // session.
  int scale_override_change_count_ = 0;

  // The maximum capture scale override, based on the last received
  // `capture_feedback_`.
  float max_capture_scale_override_ = kMaxCaptureScaleOverride;

  // Delegate for getting and setting the scale override.
  raw_ref<Delegate> delegate_;

  // The consumer-requested capture size.
  const gfx::Size capture_size_;

  // The last reported content size, if any.
  std::optional<gfx::Size> content_size_;

  // The last received video capture feedback, if any.
  std::optional<media::VideoCaptureFeedback> capture_feedback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_AUTO_SCALER_H_
