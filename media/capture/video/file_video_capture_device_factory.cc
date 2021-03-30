// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/file_video_capture_device_factory.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/capture/video/file_video_capture_device.h"

namespace media {

const char kFileVideoCaptureDeviceName[] =
    "/dev/placeholder-for-file-backed-fake-capture-device";

// Inspects the command line and retrieves the file path parameter.
base::FilePath GetFilePathFromCommandLine() {
  base::FilePath command_line_file_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kUseFileForFakeVideoCapture);
  CHECK(!command_line_file_path.empty());
  return command_line_file_path;
}

std::unique_ptr<VideoCaptureDevice> FileVideoCaptureDeviceFactory::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
#if defined(OS_WIN)
  return std::unique_ptr<VideoCaptureDevice>(new FileVideoCaptureDevice(
      base::FilePath(base::SysUTF8ToWide(device_descriptor.display_name()))));
#else
  return std::unique_ptr<VideoCaptureDevice>(new FileVideoCaptureDevice(
      base::FilePath(device_descriptor.display_name())));
#endif
}

void FileVideoCaptureDeviceFactory::GetDevicesInfo(
    GetDevicesInfoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ThreadRestrictions::SetIOAllowed(true);

  std::vector<VideoCaptureDeviceInfo> devices_info;

  auto api =
#if defined(OS_WIN)
      VideoCaptureApi::WIN_DIRECT_SHOW;
#elif defined(OS_MAC)
      VideoCaptureApi::MACOSX_AVFOUNDATION;
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
      VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE;
#else
      VideoCaptureApi::UNKNOWN;
#endif

  devices_info.emplace_back(
      VideoCaptureDeviceDescriptor(GetFilePathFromCommandLine().AsUTF8Unsafe(),
                                   kFileVideoCaptureDeviceName, api));

  VideoCaptureFormat capture_format;
  if (!FileVideoCaptureDevice::GetVideoCaptureFormat(
          GetFilePathFromCommandLine(), &capture_format)) {
    std::move(callback).Run({});
    return;
  }
  devices_info.back().supported_formats.push_back(capture_format);

  std::move(callback).Run(std::move(devices_info));
}

}  // namespace media
