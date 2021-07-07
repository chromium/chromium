// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_AGGREGATION_SERVICE_H_
#define CONTENT_PUBLIC_TEST_TEST_AGGREGATION_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "url/origin.h"

namespace content {

// Interface for a test aggregation service which can be created without any
// dependencies. Supports configuring public keys at runtime.
class TestAggregationService {
 public:
  virtual ~TestAggregationService() = default;

  // Creates an instance of the service.
  static std::unique_ptr<TestAggregationService> Create();

  // Parses the keys for `origin` from `json_string`, and saves the set of keys
  // to storage. `callback` will be run once completed which takes a boolean
  // value indicating whether the keys were parsed successfully.
  virtual void SetPublicKeys(const url::Origin& origin,
                             const std::string& json_string,
                             base::OnceCallback<void(bool)> callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_AGGREGATION_SERVICE_H_