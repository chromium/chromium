// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/file_video_capture_device_factory.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
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
  base::AssertBlockingAllowedDeprecated();
#if defined(OS_WIN)
  return std::unique_ptr<VideoCaptureDevice>(new FileVideoCaptureDevice(
      base::FilePath(base::SysUTF8ToWide(device_descriptor.display_name()))));
#else
  return std::unique_ptr<VideoCaptureDevice>(new FileVideoCaptureDevice(
      base::FilePath(device_descriptor.display_name())));
#endif
}

void FileVideoCaptureDeviceFactory::GetDeviceDescriptors(
    VideoCaptureDeviceDescriptors* device_descriptors) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(device_descriptors->empty());
  base::ThreadRestrictions::SetIOAllowed(true);

  const base::FilePath command_line_file_path = GetFilePathFromCommandLine();
  device_descriptors->emplace_back(
#if defined(OS_WIN)
      base::SysWideToUTF8(command_line_file_path.value()),
      kFileVideoCaptureDeviceName, VideoCaptureApi::WIN_DIRECT_SHOW
#elif defined(OS_MACOSX)
      command_line_file_path.value(), kFileVideoCaptureDeviceName,
      VideoCaptureApi::MACOSX_AVFOUNDATION
#elif defined(OS_LINUX)
      command_line_file_path.value(), kFileVideoCaptureDeviceName,
      VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE
#else
      command_line_file_path.value(), kFileVideoCaptureDeviceName,
      VideoCaptureApi::UNKNOWN
#endif
      );
}

void FileVideoCaptureDeviceFactory::GetSupportedFormats(
    const VideoCaptureDeviceDescriptor& device_descriptor,
    VideoCaptureFormats* supported_formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AssertBlockingAllowedDeprecated();

  VideoCaptureFormat capture_format;
  if (!FileVideoCaptureDevice::GetVideoCaptureFormat(
        GetFilePathFromCommandLine(), &capture_format)) {
    return;
  }

  supported_formats->push_back(capture_format);
}

void FileVideoCaptureDeviceFactory::GetCameraLocationsAsync(
    std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors,
    DeviceDescriptorsCallback result_callback) {
  std::move(result_callback).Run(std::move(device_descriptors));
}

}  // namespace media
