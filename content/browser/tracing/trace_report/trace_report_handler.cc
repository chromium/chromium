// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_report/trace_report_handler.h"

#include <optional>

#include "base/uuid.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/trace_report/trace_report_database.h"
#include "content/browser/tracing/trace_report/trace_upload_list.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

TraceReportHandler::TraceReportHandler(
    mojo::PendingReceiver<trace_report::mojom::PageHandler> receiver,
    mojo::PendingRemote<trace_report::mojom::Page> page)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      trace_upload_list_(BackgroundTracingManagerImpl::GetInstance()),
      background_tracing_manager_(BackgroundTracingManagerImpl::GetInstance()) {
  trace_upload_list_->OpenDatabaseIfExists();
}

TraceReportHandler::TraceReportHandler(
    mojo::PendingReceiver<trace_report::mojom::PageHandler> receiver,
    mojo::PendingRemote<trace_report::mojom::Page> page,
    TraceUploadList& trace_upload_list,
    BackgroundTracingManagerImpl& background_tracing_manager)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      trace_upload_list_(trace_upload_list),
      background_tracing_manager_(background_tracing_manager) {
  trace_upload_list_->OpenDatabaseIfExists();
}

TraceReportHandler::~TraceReportHandler() = default;

void TraceReportHandler::DeleteSingleTrace(const base::Token& uuid,
                                           DeleteSingleTraceCallback callback) {
  trace_upload_list_->DeleteSingleTrace(uuid, std::move(callback));
}

void TraceReportHandler::DeleteAllTraces(DeleteAllTracesCallback callback) {
  trace_upload_list_->DeleteAllTraces(std::move(callback));
}

void TraceReportHandler::UserUploadSingleTrace(
    const base::Token& uuid,
    UserUploadSingleTraceCallback callback) {
  trace_upload_list_->UserUploadSingleTrace(uuid, std::move(callback));
}

void TraceReportHandler::DownloadTrace(const base::Token& uuid,
                                       DownloadTraceCallback callback) {
  trace_upload_list_->DownloadTrace(
      uuid, base::BindOnce(
                [](DownloadTraceCallback callback,
                   std::optional<base::span<const char>> trace) {
                  if (trace) {
                    std::move(callback).Run(
                        mojo_base::BigBuffer(base::as_bytes(*trace)));
                  } else {
                    std::move(callback).Run(std::nullopt);
                  }
                },
                std::move(callback)));
}

void TraceReportHandler::GetAllTraceReports(
    GetAllTraceReportsCallback callback) {
  trace_upload_list_->GetAllTraceReports(
      base::BindOnce(&TraceReportHandler::OnGetAllReportsTaskComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void TraceReportHandler::OnGetAllReportsTaskComplete(
    GetAllTraceReportsCallback callback,
    std::vector<ClientTraceReport> results) {
  std::vector<trace_report::mojom::ClientTraceReportPtr> reports;
  for (const auto& single_report : results) {
    auto new_trace = trace_report::mojom::ClientTraceReport::New();
    new_trace->uuid = single_report.uuid;
    new_trace->creation_time = single_report.creation_time;
    new_trace->scenario_name = single_report.scenario_name;
    new_trace->upload_rule_name = single_report.upload_rule_name;
    new_trace->total_size = single_report.total_size;
    new_trace->upload_state = single_report.upload_state;
    new_trace->upload_time = single_report.upload_time;
    new_trace->skip_reason = single_report.skip_reason;
    new_trace->has_trace_content = single_report.has_trace_content;
    reports.push_back(std::move(new_trace));
  }
  std::move(callback).Run(std::move(reports));
}

void TraceReportHandler::GetAllPresetScenarios(
    GetAllPresetScenariosCallback callback) {
  std::move(callback).Run(background_tracing_manager_->GetAllPresetScenarios());
}

void TraceReportHandler::GetAllFieldScenarios(
    GetAllFieldScenariosCallback callback) {
  std::move(callback).Run(background_tracing_manager_->GetAllFieldScenarios());
}

void TraceReportHandler::GetEnabledScenarios(
    GetEnabledScenariosCallback callback) {
  std::move(callback).Run(background_tracing_manager_->GetEnabledScenarios());
}

void TraceReportHandler::SetEnabledScenarios(
    const std::vector<std::string>& new_config,
    SetEnabledScenariosCallback callback) {
  auto response = background_tracing_manager_->SetEnabledScenarios(new_config);
  if (response) {
    tracing::BackgroundTracingStateManager::GetInstance()
        .UpdateEnabledScenarios(new_config);
  }
  std::move(callback).Run(std::move(response));
}

}  // namespace content
