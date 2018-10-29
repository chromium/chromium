// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_device.h"

#include "base/command_line.h"
#include "base/i18n/timezone.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"

namespace media {

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

void VideoCaptureDevice::GetPhotoState(GetPhotoStateCallback callback) {}

void VideoCaptureDevice::SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                                         SetPhotoOptionsCallback callback) {}

void VideoCaptureDevice::TakePhoto(TakePhotoCallback callback) {}

PowerLineFrequency VideoCaptureDevice::GetPowerLineFrequencyForLocation()
    const {
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
  const char** countries_using_60Hz_end =
      countries_using_60Hz + arraysize(countries_using_60Hz);
  if (std::find(countries_using_60Hz, countries_using_60Hz_end,
                current_country) == countries_using_60Hz_end) {
    return PowerLineFrequency::FREQUENCY_50HZ;
  }
  return PowerLineFrequency::FREQUENCY_60HZ;
}

PowerLineFrequency VideoCaptureDevice::GetPowerLineFrequency(
    const VideoCaptureParams& params) const {
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
