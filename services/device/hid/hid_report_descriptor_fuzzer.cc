// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include "services/device/public/cpp/hid/hid_report_descriptor.h"
#include "services/device/public/mojom/hid.mojom.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  device::HidReportDescriptor desc(std::vector<uint8_t>(data, data + size));
  std::vector<device::mojom::HidCollectionInfoPtr> top_level_collections;
  bool has_report_id;
  size_t max_input_report_size;
  size_t max_output_report_size;
  size_t max_feature_report_size;
  desc.GetDetails(&top_level_collections, &has_report_id,
                  &max_input_report_size, &max_output_report_size,
                  &max_feature_report_size);
  return 0;
}
