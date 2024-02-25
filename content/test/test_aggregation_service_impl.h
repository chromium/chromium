// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_AGGREGATION_SERVICE_IMPL_H_
#define CONTENT_TEST_TEST_AGGREGATION_SERVICE_IMPL_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/aggregation_service/aggregation_service_storage_context.h"
#include "content/public/test/test_aggregation_service.h"

namespace base {
class Clock;
}  // namespace base

namespace content {

class AggregatableReportSender;
class AggregatableReportAssembler;
class AggregationServiceStorage;

struct PublicKey;

// Implementation class of a test aggregation service.
class TestAggregationServiceImpl : public AggregationServiceStorageContext,
                                   public TestAggregationService {
 public:
  // `clock` must be a non-null pointer that is valid as long as this object.
  TestAggregationServiceImpl(
      const base::Clock* clock,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  TestAggregationServiceImpl(const TestAggregationServiceImpl& other) = delete;
  TestAggregationServiceImpl& operator=(
      const TestAggregationServiceImpl& other) = delete;
  ~TestAggregationServiceImpl() override;

  // AggregationServiceStorageContext:
  const base::SequenceBound<AggregationServiceStorage>& GetStorage() override;

  // TestAggregationService:
  void SetDisablePayloadEncryption(bool should_disable) override;
  void SetPublicKeys(const GURL& url,
                     const base::FilePath& json_file,
                     base::OnceCallback<void(bool)> callback) override;
  void AssembleReport(
      AssembleRequest request,
      base::OnceCallback<void(base::Value::Dict)> callback) override;
  void SendReport(const GURL& url,
                  const base::Value& contents,
                  base::OnceCallback<void(bool)> callback) override;

  void GetPublicKeys(
      const GURL& url,
      base::OnceCallback<void(std::vector<PublicKey>)> callback) const;

 private:
  const raw_ref<const base::Clock> clock_;

  base::SequenceBound<AggregationServiceStorage> storage_;
  std::unique_ptr<AggregatableReportSender> sender_;
  std::unique_ptr<AggregatableReportAssembler> assembler_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_AGGREGATION_SERVICE_IMPL_H_
