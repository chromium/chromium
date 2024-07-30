// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_aggregation_service_impl.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "base/uuid.h"
#include "base/values.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregatable_report_assembler.h"
#include "content/browser/aggregation_service/aggregatable_report_sender.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
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

blink::mojom::AggregationServiceMode ConvertToAggregationMode(
    TestAggregationService::AggregationMode aggregation_mode) {
  switch (aggregation_mode) {
    case TestAggregationService::AggregationMode::kTeeBased:
      return blink::mojom::AggregationServiceMode::kTeeBased;
    case TestAggregationService::AggregationMode::kExperimentalPoplar:
      return blink::mojom::AggregationServiceMode::kExperimentalPoplar;
  }
}

void HandleAggregatableReportCallback(
    base::OnceCallback<void(base::Value::Dict)> callback,
    AggregatableReportRequest,
    std::optional<AggregatableReport> report,
    AggregatableReportAssembler::AssemblyStatus status) {
  if (!report.has_value()) {
    LOG(ERROR) << "Failed to assemble the report, status: "
               << static_cast<int>(status);
    std::move(callback).Run(base::Value::Dict());
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
      sender_(AggregatableReportSender::CreateForTesting(
          url_loader_factory,
          /*enable_debug_logging=*/true)),
      assembler_(AggregatableReportAssembler::CreateForTesting(
          /*storage_context=*/this,
          url_loader_factory,
          /*enable_debug_logging=*/true)) {
  DCHECK(clock);
}

TestAggregationServiceImpl::~TestAggregationServiceImpl() = default;

const base::SequenceBound<AggregationServiceStorage>&
TestAggregationServiceImpl::GetStorage() {
  return storage_;
}

void TestAggregationServiceImpl::SetDisablePayloadEncryption(
    bool should_disable) {
  content::AggregatableReport::Provider::SetDisableEncryptionForTestingTool(
      should_disable);
}

void TestAggregationServiceImpl::SetPublicKeys(
    const GURL& url,
    const base::FilePath& json_file,
    base::OnceCallback<void(bool)> callback) {
  ASSIGN_OR_RETURN(
      PublicKeyset keyset,
      aggregation_service::ReadAndParsePublicKeys(json_file, clock_->Now()),
      [&](std::string error) {
        LOG(ERROR) << error;
        std::move(callback).Run(false);
      });

  storage_.AsyncCall(&AggregationServiceStorage::SetPublicKeys)
      .WithArgs(url, std::move(keyset))
      .Then(base::BindOnce(std::move(callback), true));
}

void TestAggregationServiceImpl::AssembleReport(
    AssembleRequest request,
    base::OnceCallback<void(base::Value::Dict)> callback) {
  AggregationServicePayloadContents payload_contents(
      ConvertToOperation(request.operation),
      {blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/request.bucket, /*value=*/request.value,
          /*filtering_id=*/std::nullopt)},
      ConvertToAggregationMode(request.aggregation_mode),
      /*aggregation_coordinator_origin=*/std::nullopt,
      /*max_contributions_allowed=*/20u,
      // TODO(crbug.com/330744610): Allow setting.
      /*filtering_id_max_bytes=*/std::nullopt);

  AggregatableReportSharedInfo shared_info(
      /*scheduled_report_time=*/base::Time::Now() + base::Seconds(30),
      /*report_id=*/base::Uuid::GenerateRandomV4(),
      std::move(request.reporting_origin),
      request.is_debug_mode_enabled
          ? AggregatableReportSharedInfo::DebugMode::kEnabled
          : AggregatableReportSharedInfo::DebugMode::kDisabled,
      std::move(request.additional_fields), std::move(request.api_version),
      std::move(request.api_identifier));

  std::optional<AggregatableReportRequest> report_request =
      AggregatableReportRequest::CreateForTesting(
          std::move(request.processing_urls), std::move(payload_contents),
          std::move(shared_info));
  if (!report_request.has_value()) {
    std::move(callback).Run(base::Value::Dict());
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
      url, contents, AggregatableReportRequest::DelayType::Unscheduled,
      base::BindOnce(
          [&](base::OnceCallback<void(bool)> callback,
              AggregatableReportSender::RequestStatus status) {
            std::move(callback).Run(
                status == AggregatableReportSender::RequestStatus::kOk);
          },
          std::move(callback)));
}

void TestAggregationServiceImpl::GetPublicKeys(
    const GURL& url,
    base::OnceCallback<void(std::vector<PublicKey>)> callback) const {
  storage_.AsyncCall(&AggregationServiceStorage::GetPublicKeys)
      .WithArgs(url)
      .Then(std::move(callback));
}

}  // namespace content
