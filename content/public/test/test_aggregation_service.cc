// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_aggregation_service.h"

#include "content/test/test_aggregation_service_impl.h"

namespace content {

std::unique_ptr<TestAggregationService> TestAggregationService::Create() {
  return std::make_unique<TestAggregationServiceImpl>();
}

}  // namespace content