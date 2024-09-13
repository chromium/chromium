// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/partitioned_popins/partitioned_popins_policy.h"

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string header(reinterpret_cast<const char*>(data), size);

  PartitionedPopinsPolicy policy(header);

  if (policy.wildcard) {
    assert(policy.origins.size() == 0);
  }

  for (size_t i = 0; i < policy.origins.size(); ++i) {
    assert(policy.origins[i].scheme() == url::kHttpsScheme);
  }

  return 0;
}

}  // namespace content
