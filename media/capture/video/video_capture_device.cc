// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_device.h"

#include <string_view>

#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
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

#if BUILDFLAG(IS_WIN)
CapturedExternalVideoBuffer::CapturedExternalVideoBuffer(
    Microsoft::WRL::ComPtr<IMFMediaBuffer> imf_buffer,
    gfx::GpuMemoryBufferHandle handle,
    VideoCaptureFormat format,
    gfx::ColorSpace color_space)
    : imf_buffer(std::move(imf_buffer)),
      handle(std::move(handle)),
      format(std::move(format)),
      color_space(std::move(color_space)) {}
#endif

CapturedExternalVideoBuffer::CapturedExternalVideoBuffer(
    CapturedExternalVideoBuffer&& other)
    : handle(std::move(other.handle)),
      format(std::move(other.format)),
      color_space(std::move(other.color_space)) {
#if BUILDFLAG(IS_WIN)
  imf_buffer = std::move(other.imf_buffer);
#endif
}

CapturedExternalVideoBuffer& CapturedExternalVideoBuffer::operator=(
    CapturedExternalVideoBuffer&& other) {
  handle = std::move(other.handle);
  format = std::move(other.format);
  color_space = std::move(other.color_space);
#if BUILDFLAG(IS_WIN)
  imf_buffer = std::move(other.imf_buffer);
#endif
  return *this;
}

CapturedExternalVideoBuffer::~CapturedExternalVideoBuffer() = default;

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

void VideoCaptureDevice::Client::OnIncomingCapturedData(
    const uint8_t* data,
    int length,
    const VideoCaptureFormat& frame_format,
    const gfx::ColorSpace& color_space,
    int clockwise_rotation,
    bool flip_y,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_timestamp) {
  OnIncomingCapturedData(data, length, frame_format, color_space,
                         clockwise_rotation, flip_y, reference_time, timestamp,
                         capture_begin_timestamp,
                         /*frame_feedback_id=*/0);
}

void VideoCaptureDevice::Client::OnIncomingCapturedGfxBuffer(
    gfx::GpuMemoryBuffer* buffer,
    const VideoCaptureFormat& frame_format,
    int clockwise_rotation,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_timestamp) {
  OnIncomingCapturedGfxBuffer(buffer, frame_format, clockwise_rotation,
                              reference_time, timestamp,
                              capture_begin_timestamp,
                              /*frame_feedback_id=*/0);
}

VideoCaptureDevice::~VideoCaptureDevice() = default;

void VideoCaptureDevice::ApplySubCaptureTarget(
    mojom::SubCaptureTargetType type,
    const base::Token& target,
    uint32_t sub_capture_target_version,
    base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
        callback) {
  std::move(callback).Run(
      media::mojom::ApplySubCaptureTargetResult::kUnsupportedCaptureDevice);
}

void VideoCaptureDevice::GetPhotoState(GetPhotoStateCallback callback) {}

void VideoCaptureDevice::SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                                         SetPhotoOptionsCallback callback) {}

void VideoCaptureDevice::TakePhoto(TakePhotoCallback callback) {}

// static
PowerLineFrequency VideoCaptureDevice::GetPowerLineFrequencyForLocation() {
  const std::string current_country = base::CountryCodeForCurrentTimezone();
  if (current_country.empty()) {
    return PowerLineFrequency::kDefault;
  }

  // Sorted out list of countries with 60Hz power line frequency, from
  // http://en.wikipedia.org/wiki/Mains_electricity_by_country
  constexpr auto kCountriesUsing60Hz = base::MakeFixedFlatSet<std::string_view>(
      {"AI", "AO", "AS", "AW", "AZ", "BM", "BR", "BS", "BZ", "CA", "CO",
       "CR", "CU", "DO", "EC", "FM", "GT", "GU", "GY", "HN", "HT", "JP",
       "KN", "KR", "KY", "MS", "MX", "NI", "PA", "PE", "PF", "PH", "PR",
       "PW", "SA", "SR", "SV", "TT", "TW", "UM", "US", "VG", "VI", "VE"});
  if (kCountriesUsing60Hz.contains(current_country)) {
    return PowerLineFrequency::k60Hz;
  }
  return PowerLineFrequency::k50Hz;
}

// static
PowerLineFrequency VideoCaptureDevice::GetPowerLineFrequency(
    const VideoCaptureParams& params) {
  switch (params.power_line_frequency) {
    case PowerLineFrequency::k50Hz:  // fall through
    case PowerLineFrequency::k60Hz:
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
