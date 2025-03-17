// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_report/trace_report_handler.h"

#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/uuid.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/trace_report/trace_report_database.h"
#include "content/browser/tracing/trace_report/trace_upload_list.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/tracing_delegate.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/snappy/src/snappy.h"

#if BUILDFLAG(IS_WIN)
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/thread_pool.h"
#include "skia/ext/codec_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/win/get_elevation_icon.h"
#endif

namespace content {
namespace {

std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
ParseSerializedPresetTracingConfig(
    const base::span<const uint8_t>& config_bytes) {
  perfetto::protos::gen::ChromeFieldTracingConfig config;
  if (config_bytes.empty()) {
    return std::nullopt;
  }
  if (config.ParseFromArray(config_bytes.data(), config_bytes.size())) {
    return config;
  }
  return std::nullopt;
}

std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
ParseEncodedPresetTracingConfig(const std::string& config_string) {
  std::string serialized_config;
  if (!base::Base64Decode(config_string, &serialized_config)) {
    return std::nullopt;
  }

  // `serialized_config` may optionally be compressed.
  std::string decompressed_config;
  if (!snappy::Uncompress(serialized_config.data(), serialized_config.size(),
                          &decompressed_config)) {
    return ParseSerializedPresetTracingConfig(
        base::as_byte_span(serialized_config));
  }

  return ParseSerializedPresetTracingConfig(
      base::as_byte_span(decompressed_config));
}

}  // namespace

TraceReportHandler::TraceReportHandler(
    mojo::PendingReceiver<trace_report::mojom::PageHandler> receiver,
    mojo::PendingRemote<trace_report::mojom::Page> page)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      trace_upload_list_(BackgroundTracingManagerImpl::GetInstance()),
      background_tracing_manager_(BackgroundTracingManagerImpl::GetInstance()),
      tracing_delegate_(
          BackgroundTracingManagerImpl::GetInstance().tracing_delegate()) {
  trace_upload_list_->OpenDatabaseIfExists();
}

TraceReportHandler::TraceReportHandler(
    mojo::PendingReceiver<trace_report::mojom::PageHandler> receiver,
    mojo::PendingRemote<trace_report::mojom::Page> page,
    TraceUploadList& trace_upload_list,
    BackgroundTracingManagerImpl& background_tracing_manager,
    TracingDelegate* tracing_delegate)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      trace_upload_list_(trace_upload_list),
      background_tracing_manager_(background_tracing_manager),
      tracing_delegate_(tracing_delegate) {
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
  for (const auto& report : results) {
    reports.push_back(trace_report::mojom::ClientTraceReport::New(
        report.uuid, report.creation_time, report.scenario_name,
        report.upload_rule_name, report.upload_rule_value, report.total_size,
        report.upload_state, report.upload_time, report.skip_reason,
        report.has_trace_content));
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

void TraceReportHandler::GetPrivacyFilterEnabled(
    GetPrivacyFilterEnabledCallback callback) {
  std::move(callback).Run(tracing::BackgroundTracingStateManager::GetInstance()
                              .privacy_filter_enabled());
}

void TraceReportHandler::SetPrivacyFilterEnabled(bool enable) {
  tracing::BackgroundTracingStateManager::GetInstance().UpdatePrivacyFilter(
      enable);
}

void TraceReportHandler::SetScenariosConfigFromString(
    const std::string& config_string,
    SetScenariosConfigFromStringCallback callback) {
  auto field_tracing_config = ParseEncodedPresetTracingConfig(config_string);
  if (!field_tracing_config) {
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(SetScenariosConfig(std::move(*field_tracing_config)));
}

void TraceReportHandler::SetScenariosConfigFromBuffer(
    mojo_base::BigBuffer config_pb,
    SetScenariosConfigFromBufferCallback callback) {
  auto field_tracing_config =
      ParseSerializedPresetTracingConfig(config_pb.byte_span());
  if (!field_tracing_config) {
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(SetScenariosConfig(std::move(*field_tracing_config)));
}

bool TraceReportHandler::SetScenariosConfig(
    const perfetto::protos::gen::ChromeFieldTracingConfig& config) {
  content::BackgroundTracingManager::DataFiltering data_filtering =
      tracing::BackgroundTracingStateManager::GetInstance()
              .privacy_filter_enabled()
          ? content::BackgroundTracingManager::ANONYMIZE_DATA
          : content::BackgroundTracingManager::NO_DATA_FILTERING;
  background_tracing_manager_->AddPresetScenarios(std::move(config),
                                                  data_filtering);
  const auto& enabled_scenarios =
      tracing::BackgroundTracingStateManager::GetInstance().enabled_scenarios();
  if (!enabled_scenarios.empty()) {
    background_tracing_manager_->SetEnabledScenarios(enabled_scenarios);
  }
  return true;
}

#if BUILDFLAG(IS_WIN)
void TraceReportHandler::GetSystemTracingState(
    GetSystemTracingStateCallback callback) {
  if (!tracing_delegate_) {
    std::move(callback).Run(/*service_supported=*/false,
                            /*service_enabled=*/false);
    return;
  }
  tracing_delegate_->GetSystemTracingState(std::move(callback));
}

void TraceReportHandler::GetSecurityShieldIconUrl(
    GetSecurityShieldIconUrlCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&gfx::win::GetElevationIcon),
      base::BindOnce(
          [](GetSecurityShieldIconUrlCallback callback, SkBitmap shield_icon) {
            if (!shield_icon.empty()) {
              std::move(callback).Run(
                  GURL(skia::EncodePngAsDataUri(shield_icon.pixmap())));
            } else {
              std::move(callback).Run({});
            }
          },
          std::move(callback)));
}

void TraceReportHandler::EnableSystemTracing(
    EnableSystemTracingCallback callback) {
  if (!tracing_delegate_) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  tracing_delegate_->EnableSystemTracing(std::move(callback));
}

void TraceReportHandler::DisableSystemTracing(
    DisableSystemTracingCallback callback) {
  if (!tracing_delegate_) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  tracing_delegate_->DisableSystemTracing(std::move(callback));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
