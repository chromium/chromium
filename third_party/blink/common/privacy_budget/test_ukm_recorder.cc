// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/privacy_budget/test_ukm_recorder.h"

#include <vector>

#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace blink {
namespace test {

TestUkmRecorder::TestUkmRecorder() = default;
TestUkmRecorder::~TestUkmRecorder() = default;

std::vector<const ukm::mojom::UkmEntry*> TestUkmRecorder::GetEntriesByHash(
    uint64_t event_hash) const {
  std::vector<const ukm::mojom::UkmEntry*> result;
  for (const auto& entry : entries_) {
    if (entry->event_hash == event_hash)
      result.push_back(entry.get());
  }
  return result;
}

}  // namespace test
}  // namespace blink
