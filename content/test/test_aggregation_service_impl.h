// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_AGGREGATION_SERVICE_IMPL_H_
#define CONTENT_TEST_TEST_AGGREGATION_SERVICE_IMPL_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/aggregation_service/aggregatable_report_manager.h"
#include "content/browser/aggregation_service/aggregation_service_key_storage.h"
#include "content/public/test/test_aggregation_service.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class AggregatableReportSender;

struct PublicKeysForOrigin;

// Implementation class of a test aggregation service.
class TestAggregationServiceImpl : public AggregatableReportManager,
                                   public TestAggregationService {
 public:
  TestAggregationServiceImpl();
  TestAggregationServiceImpl(const TestAggregationServiceImpl& other) = delete;
  TestAggregationServiceImpl& operator=(
      const TestAggregationServiceImpl& other) = delete;
  ~TestAggregationServiceImpl() override;

  // AggregatableReportManager:
  const base::SequenceBound<AggregationServiceKeyStorage>& GetKeyStorage()
      override;

  // TestAggregationService:
  void SetPublicKeys(const url::Origin& origin,
                     const std::string& json_string,
                     base::OnceCallback<void(bool)> callback) override;
  void SetURLLoaderFactory(scoped_refptr<network::SharedURLLoaderFactory>
                               url_loader_factory) override;
  void SendReport(const GURL& url,
                  const base::Value& contents,
                  base::OnceCallback<void(bool)> callback) override;

  void GetPublicKeys(
      const url::Origin& origin,
      base::OnceCallback<void(PublicKeysForOrigin)> callback) const;

 private:
  base::SequenceBound<AggregationServiceKeyStorage> storage_;
  std::unique_ptr<AggregatableReportSender> sender_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_AGGREGATION_SERVICE_IMPL_H_