// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/output_device_info.h"

#include <sstream>

namespace media {

// Output device information returned by GetOutputDeviceInfo() methods of
// various interfaces.
OutputDeviceInfo::OutputDeviceInfo()
    : OutputDeviceInfo(OUTPUT_DEVICE_STATUS_ERROR_INTERNAL) {}

OutputDeviceInfo::OutputDeviceInfo(OutputDeviceStatus device_status)
    : OutputDeviceInfo(std::string(),
                       device_status,
                       AudioParameters::UnavailableDeviceParams()) {}

OutputDeviceInfo::OutputDeviceInfo(const std::string& device_id,
                                   OutputDeviceStatus device_status,
                                   const AudioParameters& output_params)
    : device_id_(device_id),
      device_status_(device_status),
      output_params_(output_params) {}

OutputDeviceInfo::OutputDeviceInfo(const OutputDeviceInfo&) = default;

OutputDeviceInfo& OutputDeviceInfo::operator=(const OutputDeviceInfo&) =
    default;

OutputDeviceInfo::~OutputDeviceInfo() = default;

std::string OutputDeviceInfo::AsHumanReadableString() const {
  std::ostringstream s;
  s << "device_id: " << device_id() << " device_status: " << device_status()
    << " output_params: [ " << output_params().AsHumanReadableString() << " ]";
  return s.str();
}

}  // namespace media
