// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_system_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/scoped_async_trace.h"
#include "media/capture/video/video_capture_device_factory.h"
#include "media/capture/video/video_capture_metrics.h"

using ScopedCaptureTrace =
    media::TypedScopedAsyncTrace<media::TraceCategory::kVideoAndImageCapture>;

namespace {

// Compares two VideoCaptureFormat by checking smallest frame_size area, then
// by width, and then by _largest_ frame_rate. Used to order a
// VideoCaptureFormats vector so that the first entry for a given resolution has
// the largest frame rate.
bool IsCaptureFormatSmaller(const media::VideoCaptureFormat& format1,
                            const media::VideoCaptureFormat& format2) {
  DCHECK(format1.frame_size.GetCheckedArea().IsValid());
  DCHECK(format2.frame_size.GetCheckedArea().IsValid());
  if (format1.frame_size.GetCheckedArea().ValueOrDefault(0) ==
      format2.frame_size.GetCheckedArea().ValueOrDefault(0)) {
    if (format1.frame_size.width() == format2.frame_size.width()) {
      return format1.frame_rate > format2.frame_rate;
    }
    return format1.frame_size.width() > format2.frame_size.width();
  }
  return format1.frame_size.GetCheckedArea().ValueOrDefault(0) <
         format2.frame_size.GetCheckedArea().ValueOrDefault(0);
}

bool IsCaptureFormatEqual(const media::VideoCaptureFormat& format1,
                          const media::VideoCaptureFormat& format2) {
  return format1.frame_size == format2.frame_size &&
         format1.frame_rate == format2.frame_rate &&
         format1.pixel_format == format2.pixel_format;
}

// This function receives a list of capture formats, sets all of them to I420
// (while keeping Y16 as is), and then removes duplicates.
void ConsolidateCaptureFormats(media::VideoCaptureFormats* formats) {
  if (formats->empty())
    return;
  // Mark all formats as I420, since this is what the renderer side will get
  // anyhow: the actual pixel format is decided at the device level.
  // Don't do this for the Y16 or NV12 formats as they are handled separately.
  for (auto& format : *formats) {
    if (format.pixel_format != media::PIXEL_FORMAT_Y16 &&
        format.pixel_format != media::PIXEL_FORMAT_NV12)
      format.pixel_format = media::PIXEL_FORMAT_I420;
  }
  std::sort(formats->begin(), formats->end(), IsCaptureFormatSmaller);
  // Remove duplicates
  auto last =
      std::unique(formats->begin(), formats->end(), IsCaptureFormatEqual);
  formats->erase(last, formats->end());
}

void DeviceInfosCallbackTrampoline(
    media::VideoCaptureSystem::DeviceInfoCallback callback,
    std::unique_ptr<ScopedCaptureTrace> trace,
    const std::vector<media::VideoCaptureDeviceInfo>& infos) {
  std::move(callback).Run(infos);
}

}  // anonymous namespace

namespace media {

VideoCaptureSystemImpl::VideoCaptureSystemImpl(
    std::unique_ptr<VideoCaptureDeviceFactory> factory)
    : factory_(std::move(factory)) {
  thread_checker_.DetachFromThread();
}

VideoCaptureSystemImpl::~VideoCaptureSystemImpl() = default;

void VideoCaptureSystemImpl::GetDeviceInfosAsync(
    DeviceInfoCallback result_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  device_enum_request_queue_.push_back(base::BindOnce(
      &DeviceInfosCallbackTrampoline, std::move(result_callback),
      ScopedCaptureTrace::CreateIfEnabled("GetDeviceInfosAsync")));
  if (device_enum_request_queue_.size() == 1) {
    // base::Unretained() is safe because |factory_| is owned and it guarantees
    // not to call the callback after destruction.
    factory_->GetDevicesInfo(base::BindOnce(
        &VideoCaptureSystemImpl::DevicesInfoReady, base::Unretained(this)));
  }
}

VideoCaptureErrorOrDevice VideoCaptureSystemImpl::CreateDevice(
    const std::string& device_id) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureSystemImpl::CreateDevice");
  DCHECK(thread_checker_.CalledOnValidThread());
  const VideoCaptureDeviceInfo* device_info = LookupDeviceInfoFromId(device_id);
  if (!device_info) {
    return VideoCaptureErrorOrDevice(
        VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound);
  }
  return factory_->CreateDevice(device_info->descriptor);
}

const VideoCaptureDeviceInfo* VideoCaptureSystemImpl::LookupDeviceInfoFromId(
    const std::string& device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto iter = base::ranges::find(devices_info_cache_, device_id,
                                 [](const VideoCaptureDeviceInfo& device_info) {
                                   return device_info.descriptor.device_id;
                                 });
  if (iter == devices_info_cache_.end())
    return nullptr;
  return &(*iter);
}

void VideoCaptureSystemImpl::DevicesInfoReady(
    std::vector<VideoCaptureDeviceInfo> devices_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!device_enum_request_queue_.empty());

  // Only save metrics the first time device infos are populated.
  if (devices_info_cache_.empty()) {
    LogCaptureDeviceMetrics(devices_info);
  }

  for (auto& device_info : devices_info) {
    ConsolidateCaptureFormats(&device_info.supported_formats);
  }

  devices_info_cache_ = std::move(devices_info);

  DeviceEnumQueue requests;
  std::swap(requests, device_enum_request_queue_);

  auto weak_this = weak_factory_.GetWeakPtr();
  for (auto& request : requests) {
    std::move(request).Run(devices_info_cache_);

    // Callbacks may destroy |this|.
    if (!weak_this)
      return;
  }
}

VideoCaptureDeviceFactory* VideoCaptureSystemImpl::GetFactory() {
  return factory_.get();
}

}  // namespace media
