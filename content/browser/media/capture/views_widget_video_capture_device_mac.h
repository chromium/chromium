// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_VIEWS_WIDGET_VIDEO_CAPTURE_DEVICE_MAC_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_VIEWS_WIDGET_VIDEO_CAPTURE_DEVICE_MAC_H_

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/browser/media/capture/frame_sink_video_capture_device.h"
#include "content/common/content_export.h"

namespace content {

struct DesktopMediaID;

// Captures the displayed contents of an views::Widget on macOS. This class is
// instantiated and destroyed on the same thread (the device capture thread,
// which is not the UI thread).
class CONTENT_EXPORT ViewsWidgetVideoCaptureDeviceMac final
    : public FrameSinkVideoCaptureDevice {
 public:
  explicit ViewsWidgetVideoCaptureDeviceMac(const DesktopMediaID& source_id);
  ~ViewsWidgetVideoCaptureDeviceMac() final;

 private:
  THREAD_CHECKER(thread_checker_);
  // The UIThreadDelegate is a class that will perform the operations that
  // ViewsWidgetVideoCaptureDeviceMac needs performed on the UI thread.
  class UIThreadDelegate;
  std::unique_ptr<UIThreadDelegate> ui_thread_delegate_;

  base::WeakPtrFactory<ViewsWidgetVideoCaptureDeviceMac> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_VIEWS_WIDGET_VIDEO_CAPTURE_DEVICE_MAC_H_
