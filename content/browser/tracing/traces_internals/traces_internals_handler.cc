// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/traces_internals/traces_internals_handler.h"

#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/trace_event/builtin_categories.h"
#include "base/uuid.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "components/tracing/common/tracing_scenarios_config.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/trace_report_database.h"
#include "content/browser/tracing/trace_upload_list.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/tracing_delegate.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/cpp/perfetto/perfetto_session.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.gen.h"
#include "third_party/snappy/src/snappy.h"
#include "third_party/webrtc_overrides/init_webrtc.h"
#include "v8/include/v8-trace-categories.h"

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

std::optional<perfetto::protos::gen::TraceConfig> ParseSerializedTraceConfig(
    const base::span<const uint8_t>& config_bytes) {
  perfetto::protos::gen::TraceConfig config;
  if (config_bytes.empty()) {
    return std::nullopt;
  }
  if (config.ParseFromArray(config_bytes.data(), config_bytes.size())) {
    return config;
  }
  return std::nullopt;
}

class TraceReader : public base::RefCountedThreadSafe<TraceReader> {
 public:
  explicit TraceReader(
      std::unique_ptr<perfetto::TracingSession> tracing_session,
      base::OnceCallback<void(std::optional<mojo_base::BigBuffer>,
                              const std::optional<base::Token>&)>
          on_trace_data_complete,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : tracing_session(std::move(tracing_session)),
        on_trace_data_complete(std::move(on_trace_data_complete)),
        task_runner(std::move(task_runner)) {}

  std::unique_ptr<perfetto::TracingSession> tracing_session;
  std::string serialized_trace;
  base::OnceCallback<void(std::optional<mojo_base::BigBuffer>,
                          const std::optional<base::Token>&)>
      on_trace_data_complete;
  scoped_refptr<base::SequencedTaskRunner> task_runner;

  static void ReadTrace(scoped_refptr<TraceReader> reader,
                        const base::Token& uuid) {
    reader->tracing_session->ReadTrace(
        [reader,
         uuid](perfetto::TracingSession::ReadTraceCallbackArgs args) mutable {
          if (args.size) {
            reader->serialized_trace.append(args.data, args.size);
          }
          if (!args.has_more) {
            reader->tracing_session->SetOnErrorCallback({});
            reader->task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](base::OnceCallback<void(
                           std::optional<mojo_base::BigBuffer>,
                           const std::optional<base::Token>&)> callback,
                       const base::Token& uuid,
                       std::string&& serialized_trace) {
                      base::span<const char> trace_span(serialized_trace);
                      std::move(callback).Run(
                          mojo_base::BigBuffer(base::as_bytes(trace_span)),
                          uuid);
                    },
                    std::move(reader->on_trace_data_complete), uuid,
                    std::move(reader->serialized_trace)));
          }
        });
  }

 private:
  friend class base::RefCountedThreadSafe<TraceReader>;

  ~TraceReader() = default;
};

void AddCategoriesToList(
    const perfetto::internal::TrackEventCategoryRegistry& registry,
    std::vector<traces_internals::mojom::TraceCategoryPtr>& categories) {
  for (size_t i = 0; i < registry.category_count(); ++i) {
    const auto* category = registry.GetCategory(i);
    auto mojom_category = traces_internals::mojom::TraceCategory::New();
    mojom_category->name = category->name;
    mojom_category->is_group = category->IsGroup();
    if (category->description) {
      mojom_category->description = category->description;
    }
    std::vector<std::string> tags_vector;
    for (const char* tag : category->tags) {
      if (!tag) {
        break;
      }
      tags_vector.push_back(tag);
    }
    mojom_category->tags = std::move(tags_vector);
    categories.push_back(std::move(mojom_category));
  }
}

}  // namespace

TracesInternalsHandler::TracesInternalsHandler(
    mojo::PendingReceiver<traces_internals::mojom::PageHandler> receiver,
    mojo::PendingRemote<traces_internals::mojom::Page> page)
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      trace_upload_list_(BackgroundTracingManagerImpl::GetInstance()),
      background_tracing_manager_(BackgroundTracingManagerImpl::GetInstance()),
      tracing_delegate_(
          TracingControllerImpl::GetInstance()->tracing_delegate()) {
  trace_upload_list_->OpenDatabaseIfExists();
  MaybeSetupPresetTracingFromFieldTrial();
}

TracesInternalsHandler::TracesInternalsHandler(
    mojo::PendingReceiver<traces_internals::mojom::PageHandler> receiver,
    mojo::PendingRemote<traces_internals::mojom::Page> page,
    TraceUploadList& trace_upload_list,
    BackgroundTracingManagerImpl& background_tracing_manager,
    TracingDelegate* tracing_delegate)
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      trace_upload_list_(trace_upload_list),
      background_tracing_manager_(background_tracing_manager),
      tracing_delegate_(tracing_delegate) {
  trace_upload_list_->OpenDatabaseIfExists();
}

TracesInternalsHandler::~TracesInternalsHandler() = default;

void TracesInternalsHandler::DeleteSingleTrace(
    const base::Token& uuid,
    DeleteSingleTraceCallback callback) {
  trace_upload_list_->DeleteSingleTrace(uuid, std::move(callback));
}

void TracesInternalsHandler::DeleteAllTraces(DeleteAllTracesCallback callback) {
  trace_upload_list_->DeleteAllTraces(std::move(callback));
}

void TracesInternalsHandler::UserUploadSingleTrace(
    const base::Token& uuid,
    UserUploadSingleTraceCallback callback) {
  trace_upload_list_->UserUploadSingleTrace(uuid, std::move(callback));
}

void TracesInternalsHandler::DownloadTrace(const base::Token& uuid,
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

void TracesInternalsHandler::StartTraceSession(
    mojo_base::BigBuffer config_pb,
    bool enable_privacy_filters,
    StartTraceSessionCallback callback) {
  if (tracing_session_) {
    std::move(callback).Run(false);
    return;
  }

  start_callback_ = std::move(callback);
  tracing_session_ = CreateTracingSession();

  auto trace_config = ParseSerializedTraceConfig(base::span(config_pb));
  if (!trace_config || !tracing::AdaptPerfettoConfigForChrome(
                           &(*trace_config), enable_privacy_filters)) {
    std::move(start_callback_).Run(false);
    return;
  }
  session_id_ = base::Token::CreateRandom();
  trace_config->set_trace_uuid_lsb(session_id_.high());
  trace_config->set_trace_uuid_msb(session_id_.low());
  session_unguessable_name_ = base::UnguessableToken::Create();
  trace_config->set_unique_session_name(session_unguessable_name_.ToString());
  tracing_session_->Setup(*trace_config);
  tracing_session_->SetOnStartCallback(
      [task_runner = task_runner_, weak_ptr = weak_factory_.GetWeakPtr()]() {
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&TracesInternalsHandler::OnTracingStart, weak_ptr));
      });
  tracing_session_->SetOnErrorCallback(
      [task_runner = task_runner_,
       weak_ptr = weak_factory_.GetWeakPtr()](perfetto::TracingError error) {
        task_runner->PostTask(
            FROM_HERE, base::BindOnce(&TracesInternalsHandler::OnTracingError,
                                      weak_ptr, error));
      });
  tracing_session_->SetOnStopCallback(
      [task_runner = task_runner_, weak_ptr = weak_factory_.GetWeakPtr()]() {
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&TracesInternalsHandler::OnTracingStop, weak_ptr));
      });
  tracing_session_->Start();
}

void TracesInternalsHandler::CloneTraceSession(
    CloneTraceSessionCallback callback) {
  if (!tracing_session_) {
    std::move(callback).Run(std::nullopt, std::nullopt);
    return;
  }
  auto cloned_session = CreateTracingSession();
  auto trace_reader = base::MakeRefCounted<TraceReader>(
      std::move(cloned_session), std::move(callback), task_runner_);
  perfetto::TracingSession::CloneTraceArgs args{
      .unique_session_name = session_unguessable_name_.ToString()};
  trace_reader->tracing_session->CloneTrace(
      args,
      [trace_reader](perfetto::TracingSession::CloneTraceCallbackArgs args) {
        if (!args.success) {
          std::move(trace_reader->on_trace_data_complete)
              .Run(std::nullopt, base::Token());
          return;
        }
        TraceReader::ReadTrace(std::move(trace_reader),
                               base::Token(args.uuid_lsb, args.uuid_msb));
      });
}

void TracesInternalsHandler::StopTraceSession(
    StopTraceSessionCallback callback) {
  if (!tracing_session_) {
    std::move(callback).Run(false);
    return;
  }
  stop_callback_ = std::move(callback);
  tracing_session_->Stop();
}

void TracesInternalsHandler::GetTrackEventCategories(
    GetTrackEventCategoriesCallback callback) {
  std::vector<traces_internals::mojom::TraceCategoryPtr> categories;

  AddCategoriesToList(base::perfetto_track_event::internal::kCategoryRegistry,
                      categories);
  AddCategoriesToList(v8::GetTrackEventCategoryRegistry(), categories);
  AddCategoriesToList(GetWebRtcTrackEventCategoryRegistry(), categories);

  std::move(callback).Run(std::move(categories));
}

void TracesInternalsHandler::GetBufferUsage(GetBufferUsageCallback callback) {
  if (!tracing_session_ || on_buffer_usage_callback_) {
    std::move(callback).Run(false, 0, false);
    return;
  }

  on_buffer_usage_callback_ = std::move(callback);
  tracing_session_->GetTraceStats(
      [task_runner = task_runner_, weak_ptr = weak_factory_.GetWeakPtr()](
          perfetto::TracingSession::GetTraceStatsCallbackArgs args) {
        tracing::ReadTraceStats(
            args,
            base::BindOnce(&TracesInternalsHandler::OnBufferUsage, weak_ptr),
            task_runner);
      });
}

void TracesInternalsHandler::OnBufferUsage(bool success,
                                           float percent_full,
                                           bool data_loss) {
  if (on_buffer_usage_callback_) {
    std::move(on_buffer_usage_callback_).Run(success, percent_full, data_loss);
  }
}

void TracesInternalsHandler::OnTracingError(perfetto::TracingError error) {
  if (start_callback_) {
    std::move(start_callback_).Run(false);
  }
  if (stop_callback_) {
    std::move(stop_callback_).Run(false);
  }
  page_->OnTraceComplete(std::nullopt, std::nullopt);
}

void TracesInternalsHandler::OnTracingStop() {
  if (stop_callback_) {
    std::move(stop_callback_).Run(true);
  }
  auto trace_reader = base::MakeRefCounted<TraceReader>(
      std::move(tracing_session_),
      base::BindOnce(&TracesInternalsHandler::OnTraceComplete,
                     weak_factory_.GetWeakPtr()),
      task_runner_);
  TraceReader::ReadTrace(std::move(trace_reader), session_id_);
}

void TracesInternalsHandler::OnTracingStart() {
  if (start_callback_) {
    std::move(start_callback_).Run(true);
  }
}

void TracesInternalsHandler::OnTraceComplete(
    std::optional<mojo_base::BigBuffer> serialized_trace,
    const std::optional<base::Token>& uuid) {
  page_->OnTraceComplete(std::move(serialized_trace), uuid);
}

void TracesInternalsHandler::GetAllTraceReports(
    GetAllTraceReportsCallback callback) {
  trace_upload_list_->GetAllTraceReports(
      base::BindOnce(&TracesInternalsHandler::OnGetAllReportsTaskComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void TracesInternalsHandler::OnGetAllReportsTaskComplete(
    GetAllTraceReportsCallback callback,
    std::vector<ClientTraceReport> results) {
  std::vector<traces_internals::mojom::ClientTraceReportPtr> reports;
  for (const auto& report : results) {
    reports.push_back(traces_internals::mojom::ClientTraceReport::New(
        report.uuid, report.creation_time, report.scenario_name,
        report.upload_rule_name, report.upload_rule_value, report.total_size,
        report.upload_state, report.upload_time, report.skip_reason,
        report.has_trace_content));
  }
  std::move(callback).Run(std::move(reports));
}

void TracesInternalsHandler::GetAllScenarios(GetAllScenariosCallback callback) {
  std::move(callback).Run(background_tracing_manager_->GetAllScenarios());
}

void TracesInternalsHandler::SetEnabledScenarios(
    const std::vector<std::string>& new_config,
    SetEnabledScenariosCallback callback) {
  auto response = background_tracing_manager_->SetEnabledScenarios(new_config);
  if (response) {
    tracing::BackgroundTracingStateManager::GetInstance()
        .UpdateEnabledScenarios(new_config);
  }
  std::move(callback).Run(std::move(response));
}

void TracesInternalsHandler::GetPrivacyFilterEnabled(
    GetPrivacyFilterEnabledCallback callback) {
  std::move(callback).Run(tracing::BackgroundTracingStateManager::GetInstance()
                              .privacy_filter_enabled());
}

void TracesInternalsHandler::SetPrivacyFilterEnabled(bool enable) {
  tracing::BackgroundTracingStateManager::GetInstance().UpdatePrivacyFilter(
      enable);
}

void TracesInternalsHandler::SetScenariosConfigFromString(
    const std::string& config_string,
    SetScenariosConfigFromStringCallback callback) {
  auto field_tracing_config =
      tracing::ParseEncodedTracingScenariosConfig(config_string);
  if (!field_tracing_config) {
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(SetScenariosConfig(std::move(*field_tracing_config)));
}

void TracesInternalsHandler::SetScenariosConfigFromBuffer(
    mojo_base::BigBuffer config_pb,
    SetScenariosConfigFromBufferCallback callback) {
  auto field_tracing_config =
      tracing::ParseSerializedTracingScenariosConfig(base::span(config_pb));
  if (!field_tracing_config) {
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(SetScenariosConfig(std::move(*field_tracing_config)));
}

bool TracesInternalsHandler::SetScenariosConfig(
    const perfetto::protos::gen::ChromeFieldTracingConfig& config) {
  content::BackgroundTracingManager::DataFiltering data_filtering =
      tracing::BackgroundTracingStateManager::GetInstance()
              .privacy_filter_enabled()
          ? content::BackgroundTracingManager::ANONYMIZE_DATA
          : content::BackgroundTracingManager::NO_DATA_FILTERING;
  background_tracing_manager_->OverwritePresetScenarios(std::move(config),
                                                        data_filtering);
  const auto& enabled_scenarios =
      tracing::BackgroundTracingStateManager::GetInstance().enabled_scenarios();
  if (!enabled_scenarios.empty()) {
    background_tracing_manager_->SetEnabledScenarios(enabled_scenarios);
  }
  return true;
}

void TracesInternalsHandler::MaybeSetupPresetTracingFromFieldTrial() {
  if (tracing::IsBackgroundTracingEnabledFromCommandLine()) {
    return;
  }
  auto tracing_scenarios_config = tracing::GetPresetTracingScenariosConfig();
  if (!tracing_scenarios_config) {
    return;
  }
  auto& config = tracing::BackgroundTracingStateManager::GetInstance();
  content::BackgroundTracingManager::DataFiltering data_filtering =
      config.privacy_filter_enabled()
          ? content::BackgroundTracingManager::ANONYMIZE_DATA
          : content::BackgroundTracingManager::NO_DATA_FILTERING;
  background_tracing_manager_->AddPresetScenarios(
      std::move(*tracing_scenarios_config), data_filtering);
}

#if BUILDFLAG(IS_WIN)
void TracesInternalsHandler::GetSystemTracingState(
    GetSystemTracingStateCallback callback) {
  if (!tracing_delegate_) {
    std::move(callback).Run(/*service_supported=*/false,
                            /*service_enabled=*/false);
    return;
  }
  tracing_delegate_->GetSystemTracingState(std::move(callback));
}

void TracesInternalsHandler::GetSecurityShieldIconUrl(
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

void TracesInternalsHandler::EnableSystemTracing(
    EnableSystemTracingCallback callback) {
  if (!tracing_delegate_) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  tracing_delegate_->EnableSystemTracing(std::move(callback));
}

void TracesInternalsHandler::DisableSystemTracing(
    DisableSystemTracingCallback callback) {
  if (!tracing_delegate_) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  tracing_delegate_->DisableSystemTracing(std::move(callback));
}
#endif  // BUILDFLAG(IS_WIN)

std::unique_ptr<perfetto::TracingSession>
TracesInternalsHandler::CreateTracingSession() {
  return perfetto::Tracing::NewTrace(perfetto::BackendType::kCustomBackend);
}

}  // namespace content
