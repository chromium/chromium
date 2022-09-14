// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_VAAPI_DEVICE_H_
#define MEDIA_GPU_VAAPI_TEST_VAAPI_DEVICE_H_

#include <va/va.h>

#include "base/files/file.h"

namespace media {
namespace vaapi_test {

// This class manages the lifetime of a VADisplay in a RAII fashion from
// vaGetDisplayDRM() to vaTerminate(). Decoders may use the display() method to
// access the VADisplay and issue direct libva calls.
class VaapiDevice {
 public:
  // Initializes the VADisplay. Success is ASSERTed.
  VaapiDevice();

  VaapiDevice(const VaapiDevice&) = delete;
  VaapiDevice& operator=(const VaapiDevice&) = delete;
  ~VaapiDevice();

  VADisplay display() const { return display_; }

 private:
  base::File display_file_;
  VADisplay display_;
};

}  // namespace vaapi_test
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_TEST_VAAPI_DEVICE_H_
