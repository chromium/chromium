// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEB_BUNDLE_WEB_BUNDLE_MEMORY_QUOTA_CONSUMER_H_
#define SERVICES_NETWORK_WEB_BUNDLE_WEB_BUNDLE_MEMORY_QUOTA_CONSUMER_H_

#include "base/component_export.h"

namespace network {

// This class is used to check the memory quota while loading subresource
// Web Bundles. The allocated quota is released in the destructor.
class COMPONENT_EXPORT(NETWORK_SERVICE) WebBundleMemoryQuotaConsumer {
 public:
  virtual ~WebBundleMemoryQuotaConsumer() = default;
  virtual bool AllocateMemory(uint64_t num_bytes) = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEB_BUNDLE_WEB_BUNDLE_MEMORY_QUOTA_CONSUMER_H_
