// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_METRICS_MAC_H_
#define MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_METRICS_MAC_H_

#import <AVFoundation/AVFoundation.h>
#include <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>

#include "media/capture/capture_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

CAPTURE_EXPORT
void LogFirstCapturedVideoFrame(const AVCaptureDeviceFormat* bestCaptureFormat,
                                const CMSampleBufferRef buffer);
CAPTURE_EXPORT void LogReactionEffectsGesturesState();

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_METRICS_MAC_H_