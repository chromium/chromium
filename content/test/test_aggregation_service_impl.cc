// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_aggregation_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregatable_report_assembler.h"
#include "content/browser/aggregation_service/aggregatable_report_sender.h"
#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/browser/aggregation_service/public_key_parsing_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

AggregationServicePayloadContents::Operation ConvertToOperation(
    TestAggregationService::Operation operation) {
  switch (operation) {
    case TestAggregationService::Operation::kHistogram:
      return AggregationServicePayloadContents::Operation::kHistogram;
  }
}

AggregationServicePayloadContents::ProcessingType ConvertToProcessingType(
    TestAggregationService::ProcessingType processing_type) {
  switch (processing_type) {
    case TestAggregationService::ProcessingType::kTwoParty:
      return AggregationServicePayloadContents::ProcessingType::kTwoParty;
    case TestAggregationService::ProcessingType::kSingleServer:
      return AggregationServicePayloadContents::ProcessingType::kSingleServer;
  }
}

void HandleAggregatableReportCallback(
    base::OnceCallback<void(base::Value::DictStorage)> callback,
    absl::optional<AggregatableReport> report,
    AggregatableReportAssembler::AssemblyStatus status) {
  if (!report.has_value()) {
    LOG(ERROR) << "Failed to assemble the report, status: "
               << static_cast<int>(status);
    std::move(callback).Run(base::Value::DictStorage());
    return;
  }

  std::move(callback).Run(report->GetAsJson());
}

}  // namespace

TestAggregationServiceImpl::TestAggregationServiceImpl(
    const base::Clock* clock,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : clock_(*clock),
      storage_(base::SequenceBound<AggregationServiceStorageSql>(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
          /*run_in_memory=*/true,
          /*path_to_database=*/base::FilePath(),
          clock)),
      sender_(AggregatableReportSender::CreateForTesting(url_loader_factory)),
      assembler_(
          AggregatableReportAssembler::CreateForTesting(/*manager=*/this,
                                                        url_loader_factory)) {
  DCHECK(clock);
}

TestAggregationServiceImpl::~TestAggregationServiceImpl() = default;

const base::SequenceBound<AggregationServiceKeyStorage>&
TestAggregationServiceImpl::GetKeyStorage() {
  return storage_;
}

void TestAggregationServiceImpl::SetDisablePayloadEncryption(
    bool should_disable) {
  content::AggregatableReport::Provider::SetDisableEncryptionForTestingTool(
      should_disable);
}

void TestAggregationServiceImpl::SetPublicKeys(
    const url::Origin& origin,
    const std::string& json_string,
    base::OnceCallback<void(bool)> callback) {
  JSONStringValueDeserializer deserializer(json_string);
  std::string error_message;
  std::unique_ptr<base::Value> value_ptr =
      deserializer.Deserialize(nullptr, &error_message);
  if (!value_ptr) {
    LOG(ERROR) << "Unable to deserialze json string: " << json_string
               << ", error: " << error_message;
    std::move(callback).Run(false);
    return;
  }

  std::vector<PublicKey> keys = aggregation_service::GetPublicKeys(*value_ptr);
  if (keys.empty()) {
    std::move(callback).Run(false);
    return;
  }

  PublicKeyset keyset(std::move(keys),
                      /*fetch_time=*/clock_.Now(),
                      /*expiry_time=*/base::Time::Max());
  storage_.AsyncCall(&AggregationServiceKeyStorage::SetPublicKeys)
      .WithArgs(origin, std::move(keyset))
      .Then(base::BindOnce(std::move(callback), true));
}

void TestAggregationServiceImpl::AssembleReport(
    AssembleRequest request,
    base::OnceCallback<void(base::Value::DictStorage)> callback) {
  AggregationServicePayloadContents payload_contents(
      ConvertToOperation(request.operation), request.bucket, request.value,
      ConvertToProcessingType(request.processing_type),
      std::move(request.reporting_origin));

  AggregatableReportSharedInfo shared_info(
      /*scheduled_report_time=*/base::Time::Now() + base::Seconds(30),
      std::move(request.privacy_budget_key));

  absl::optional<AggregatableReportRequest> report_request =
      AggregatableReportRequest::Create(std::move(request.processing_origins),
                                        std::move(payload_contents),
                                        std::move(shared_info));
  if (!report_request.has_value()) {
    std::move(callback).Run(base::Value::DictStorage());
    return;
  }

  assembler_->AssembleReport(
      std::move(report_request.value()),
      base::BindOnce(HandleAggregatableReportCallback, std::move(callback)));
}

void TestAggregationServiceImpl::SendReport(
    const GURL& url,
    const base::Value& contents,
    base::OnceCallback<void(bool)> callback) {
  sender_->SendReport(
      url, contents,
      base::BindOnce(
          [&](base::OnceCallback<void(bool)> callback,
              AggregatableReportSender::RequestStatus status) {
            std::move(callback).Run(
                status == AggregatableReportSender::RequestStatus::kOk);
          },
          std::move(callback)));
}

void TestAggregationServiceImpl::GetPublicKeys(
    const url::Origin& origin,
    base::OnceCallback<void(std::vector<PublicKey>)> callback) const {
  storage_.AsyncCall(&AggregationServiceKeyStorage::GetPublicKeys)
      .WithArgs(origin)
      .Then(std::move(callback));
}

}  // namespace content