// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_device.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/i18n/timezone.h"
#include "base/strings/string_util.h"
#include "base/token.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/capture/mojom/video_capture_types.mojom.h"

namespace media {

CapturedExternalVideoBuffer::CapturedExternalVideoBuffer(
    gfx::GpuMemoryBufferHandle handle,
    VideoCaptureFormat format,
    gfx::ColorSpace color_space)
    : handle(std::move(handle)),
      format(std::move(format)),
      color_space(std::move(color_space)) {}

CapturedExternalVideoBuffer::CapturedExternalVideoBuffer(
    CapturedExternalVideoBuffer&& other)
    : handle(std::move(other.handle)),
      format(std::move(other.format)),
      color_space(std::move(other.color_space)) {}

CapturedExternalVideoBuffer::~CapturedExternalVideoBuffer() = default;

CapturedExternalVideoBuffer& CapturedExternalVideoBuffer::operator=(
    CapturedExternalVideoBuffer&& other) {
  handle = std::move(other.handle);
  format = std::move(other.format);
  color_space = std::move(other.color_space);
  return *this;
}

VideoCaptureDevice::Client::Buffer::Buffer() : id(0), frame_feedback_id(0) {}

VideoCaptureDevice::Client::Buffer::Buffer(
    int buffer_id,
    int frame_feedback_id,
    std::unique_ptr<HandleProvider> handle_provider,
    std::unique_ptr<ScopedAccessPermission> access_permission)
    : id(buffer_id),
      frame_feedback_id(frame_feedback_id),
      handle_provider(std::move(handle_provider)),
      access_permission(std::move(access_permission)) {}

VideoCaptureDevice::Client::Buffer::Buffer(
    VideoCaptureDevice::Client::Buffer&& other) = default;

VideoCaptureDevice::Client::Buffer::~Buffer() = default;

VideoCaptureDevice::Client::Buffer& VideoCaptureDevice::Client::Buffer::
operator=(VideoCaptureDevice::Client::Buffer&& other) = default;

VideoCaptureDevice::~VideoCaptureDevice() = default;

void VideoCaptureDevice::Crop(
    const base::Token& crop_id,
    uint32_t crop_version,
    base::OnceCallback<void(media::mojom::CropRequestResult)> callback) {
  std::move(callback).Run(
      media::mojom::CropRequestResult::kUnsupportedCaptureDevice);
}

void VideoCaptureDevice::GetPhotoState(GetPhotoStateCallback callback) {}

void VideoCaptureDevice::SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                                         SetPhotoOptionsCallback callback) {}

void VideoCaptureDevice::TakePhoto(TakePhotoCallback callback) {}

// static
PowerLineFrequency VideoCaptureDevice::GetPowerLineFrequencyForLocation() {
  const std::string current_country = base::CountryCodeForCurrentTimezone();
  if (current_country.empty())
    return PowerLineFrequency::FREQUENCY_DEFAULT;
  // Sorted out list of countries with 60Hz power line frequency, from
  // http://en.wikipedia.org/wiki/Mains_electricity_by_country
  const char* countries_using_60Hz[] = {
      "AI", "AO", "AS", "AW", "AZ", "BM", "BR", "BS", "BZ", "CA", "CO",
      "CR", "CU", "DO", "EC", "FM", "GT", "GU", "GY", "HN", "HT", "JP",
      "KN", "KR", "KY", "MS", "MX", "NI", "PA", "PE", "PF", "PH", "PR",
      "PW", "SA", "SR", "SV", "TT", "TW", "UM", "US", "VG", "VI", "VE"};
  if (!base::Contains(countries_using_60Hz, current_country)) {
    return PowerLineFrequency::FREQUENCY_50HZ;
  }
  return PowerLineFrequency::FREQUENCY_60HZ;
}

// static
PowerLineFrequency VideoCaptureDevice::GetPowerLineFrequency(
    const VideoCaptureParams& params) {
  switch (params.power_line_frequency) {
    case PowerLineFrequency::FREQUENCY_50HZ:  // fall through
    case PowerLineFrequency::FREQUENCY_60HZ:
      return params.power_line_frequency;
    default:
      return GetPowerLineFrequencyForLocation();
  }
}

VideoCaptureFrameDropReason ConvertReservationFailureToFrameDropReason(
    VideoCaptureDevice::Client::ReserveResult reserve_result) {
  switch (reserve_result) {
    case VideoCaptureDevice::Client::ReserveResult::kSucceeded:
      return VideoCaptureFrameDropReason::kNone;
    case VideoCaptureDevice::Client::ReserveResult::kMaxBufferCountExceeded:
      return VideoCaptureFrameDropReason::kBufferPoolMaxBufferCountExceeded;
    case VideoCaptureDevice::Client::ReserveResult::kAllocationFailed:
      return VideoCaptureFrameDropReason::kBufferPoolBufferAllocationFailed;
  }
}

}  // namespace media
