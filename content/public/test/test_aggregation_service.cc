// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_aggregation_service.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "content/test/test_aggregation_service_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

std::unique_ptr<TestAggregationService> TestAggregationService::Create(
    const base::Clock* clock,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<TestAggregationServiceImpl>(
      clock, std::move(url_loader_factory));
}

}  // namespace content