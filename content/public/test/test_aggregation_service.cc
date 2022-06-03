// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_aggregation_service.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "content/test/test_aggregation_service_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

TestAggregationService::AssembleRequest::AssembleRequest(
    Operation operation,
    int bucket,
    int value,
    ProcessingType processing_type,
    url::Origin reporting_origin,
    std::string privacy_budget_key,
    std::vector<url::Origin> processing_origins)
    : operation(operation),
      bucket(bucket),
      value(value),
      processing_type(processing_type),
      reporting_origin(std::move(reporting_origin)),
      privacy_budget_key(std::move(privacy_budget_key)),
      processing_origins(std::move(processing_origins)) {}

TestAggregationService::AssembleRequest::AssembleRequest(
    AssembleRequest&& other) = default;

TestAggregationService::AssembleRequest&
TestAggregationService::AssembleRequest::operator=(AssembleRequest&& other) =
    default;

TestAggregationService::AssembleRequest::~AssembleRequest() = default;

std::unique_ptr<TestAggregationService> TestAggregationService::Create(
    const base::Clock* clock,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<TestAggregationServiceImpl>(
      clock, std::move(url_loader_factory));
}

}  // namespace content