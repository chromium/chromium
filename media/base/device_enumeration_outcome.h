// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DEVICE_ENUMERATION_OUTCOME_H_
#define MEDIA_BASE_DEVICE_ENUMERATION_OUTCOME_H_

namespace media {

// Used to log outcomes of device enumerations (audio and video). These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class DeviceEnumerationOutcome {
  kSuccessEmptyResult = 0,
  kSuccessNonEmptyResult = 1,
  kFailureEmptyResult = 2,
  kFailureNonEmptyResult = 3,
  kMaxValue = kFailureNonEmptyResult
};

constexpr inline DeviceEnumerationOutcome GetDeviceEnumerationOutcome(
    bool success,
    bool has_devices) {
  if (success) {
    return has_devices ? DeviceEnumerationOutcome::kSuccessNonEmptyResult
                       : DeviceEnumerationOutcome::kSuccessEmptyResult;
  }
  return has_devices ? DeviceEnumerationOutcome::kFailureNonEmptyResult
                     : DeviceEnumerationOutcome::kFailureEmptyResult;
}

}  // namespace media

#endif  // MEDIA_BASE_DEVICE_ENUMERATION_OUTCOME_H_
