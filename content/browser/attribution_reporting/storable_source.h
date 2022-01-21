// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORABLE_SOURCE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORABLE_SOURCE_H_

#include <stdint.h>

#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// Contains attributes specific to a source that hasn't been stored yet.
class CONTENT_EXPORT StorableSource {
 public:
  StorableSource(CommonSourceInfo common_info,
                 absl::optional<uint64_t> fake_trigger_data);

  ~StorableSource();

  StorableSource(const StorableSource&);
  StorableSource(StorableSource&&);

  StorableSource& operator=(const StorableSource&);
  StorableSource& operator=(StorableSource&&);

  const CommonSourceInfo& common_info() const { return common_info_; }

  absl::optional<uint64_t> fake_trigger_data() const {
    return fake_trigger_data_;
  }

 private:
  CommonSourceInfo common_info_;

  absl::optional<uint64_t> fake_trigger_data_;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORABLE_SOURCE_H_
