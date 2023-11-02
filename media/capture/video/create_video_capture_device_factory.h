// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CREATE_VIDEO_CAPTURE_DEVICE_FACTORY_H_
#define MEDIA_CAPTURE_VIDEO_CREATE_VIDEO_CAPTURE_DEVICE_FACTORY_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace media {

bool CAPTURE_EXPORT ShouldUseFakeVideoCaptureDeviceFactory();

std::unique_ptr<VideoCaptureDeviceFactory> CAPTURE_EXPORT
CreateVideoCaptureDeviceFactory(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CREATE_VIDEO_CAPTURE_DEVICE_FACTORY_H_
