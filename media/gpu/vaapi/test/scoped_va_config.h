// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_SCOPED_VA_CONFIG_H_
#define MEDIA_GPU_VAAPI_TEST_SCOPED_VA_CONFIG_H_

#include "media/gpu/vaapi/test/vaapi_device.h"

namespace media {
namespace vaapi_test {

// This class holds configuration information for a VaapiDevice. The VaapiDevice
// must be externally guaranteed to outlive the ScopedVAConfig.
class ScopedVAConfig {
 public:
  // Initializes the VA handles for a VAConfig with |profile| and
  // |va_rt_format|. Requires an initialized |device|. Success is ASSERTed.
  ScopedVAConfig(const VaapiDevice& device,
                 VAProfile profile,
                 unsigned int va_rt_format);

  ScopedVAConfig(const ScopedVAConfig&) = delete;
  ScopedVAConfig& operator=(const ScopedVAConfig&) = delete;
  ~ScopedVAConfig();

  VAConfigID id() const { return config_id_; }
  VAProfile profile() const { return profile_; }
  unsigned int va_rt_format() const { return va_rt_format_; }

 private:
  // Non-owned.
  const VaapiDevice& device_;

  VAConfigID config_id_;
  const VAProfile profile_;
  const unsigned int va_rt_format_;
};

}  // namespace vaapi_test
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_TEST_SCOPED_VA_CONFIG_H_
