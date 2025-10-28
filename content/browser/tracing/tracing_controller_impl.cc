// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/tracing_controller_impl.h"

#include <inttypes.h>

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/cpu.h"
#include "base/dcheck_is_on.h"
#include "base/files/file_tracing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_view_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_log.h"
#include "base/tracing/protos/grit/tracing_proto_resources.h"
#include "base/values.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/variations/active_field_trials.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/tracing/file_tracing_provider_impl.h"
#include "content/browser/tracing/tracing_ui.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/tracing_service.h"
#include "content/public/common/content_client.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/network_change_notifier.h"
#include "net/log/net_log_util.h"
#include "services/tracing/public/cpp/perfetto/metadata_data_source.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/traced_process_impl.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/perfetto/include/perfetto/protozero/message.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/extension_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/webrtc_overrides/init_webrtc.h"
#include "v8/include/v8-trace-categories.h"
#include "v8/include/v8-version-string.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/system/statistics_provider.h"
#include "content/browser/tracing/cros_tracing_agent.h"
#endif

#if defined(CAST_TRACING_AGENT)
#include "content/browser/tracing/cast_tracing_agent.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/power_monitor/cpu_frequency_utils.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include <sys/time.h>
#include "content/browser/android/tracing_controller_android.h"
#include "services/tracing/public/cpp/perfetto/java_heap_profiler/java_heap_profiler_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {

namespace {

inline constexpr char kNetConstantMetadataPrefix[] = "net-constant-";
inline constexpr char kUserAgentKey[] = "user-agent";
inline constexpr char kRevisionMetadataKey[] = "revision";

TracingControllerImpl* g_tracing_controller = nullptr;

void AddCategoriesToSet(
    const perfetto::internal::TrackEventCategoryRegistry& registry,
    std::set<std::string>& category_set) {
  for (size_t i = 0; i < registry.category_count(); ++i) {
    if (registry.GetCategory(i)->IsGroup()) {
      continue;
    }
    category_set.insert(registry.GetCategory(i)->name);
  }
}

}  // namespace

TracingController* TracingController::GetInstance() {
  return TracingControllerImpl::GetInstance();
}

TracingControllerImpl::TracingControllerImpl()
    : delegate_(GetContentClient()->browser()->CreateTracingDelegate()) {
  DCHECK(!g_tracing_controller);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(delegate_);
  // Deliberately leaked, like this class.
  base::FileTracing::SetProvider(new FileTracingProviderImpl);
  InitializeDataSources();
  g_tracing_controller = this;

#if BUILDFLAG(IS_CHROMEOS)
  // Bind hwclass once the statistics are available.
  ash::system::StatisticsProvider::GetInstance()
      ->ScheduleOnMachineStatisticsLoaded(
          base::BindOnce(&TracingControllerImpl::OnMachineStatisticsLoaded,
                         weak_ptr_factory_.GetWeakPtr()));
#endif

  tracing::PerfettoTracedProcess::Get().SetConsumerConnectionFactory(
      &GetTracingService, base::SingleThreadTaskRunner::GetCurrentDefault());
}

TracingControllerImpl::~TracingControllerImpl() = default;

void TracingControllerImpl::InitializeDataSources() {
  tracing::TracedProcessImpl::GetInstance()->SetTaskRunner(
      base::SequencedTaskRunner::GetCurrentDefault());

  // Metadata only needs to be installed in the browser process.
  tracing::MetadataDataSource::Register(
      base::SequencedTaskRunner::GetCurrentDefault(),
      {tracing_delegate()->CreateSystemProfileMetadataRecorder(),
       base::BindRepeating(&TracingControllerImpl::RecorderMetadataToBundle)},
      {base::BindRepeating(&TracingControllerImpl::GenerateMetadataPacket)});

#if BUILDFLAG(IS_CHROMEOS)
  RegisterCrOSTracingDataSource();
#elif defined(CAST_TRACING_AGENT)
  RegisterCastTracingDataSource();
#endif
}

void TracingControllerImpl::ConnectToServiceIfNeeded() {
  if (!consumer_host_) {
    GetTracingService().BindConsumerHost(
        consumer_host_.BindNewPipeAndPassReceiver());
    consumer_host_.reset_on_disconnect();
  }
}

void TracingControllerImpl::RecorderMetadataToBundle(
    perfetto::protos::pbzero::ChromeEventBundle* bundle) {
  tracing::MetadataDataSource::AddMetadataToBundle(
      kRevisionMetadataKey, version_info::GetLastChange(), bundle);
  tracing::MetadataDataSource::AddMetadataToBundle(
      kUserAgentKey, GetContentClient()->browser()->GetUserAgent(), bundle);
  for (auto constant :
       net::GetNetConstants(net::NetConstantsRequestMode::kTracing)) {
    tracing::MetadataDataSource::AddMetadataToBundle(
        base::StrCat({kNetConstantMetadataPrefix, constant.first}),
        constant.second, bundle);
  }
}

void TracingControllerImpl::GenerateMetadataPacket(
    perfetto::protos::pbzero::TracePacket* handle,
    bool privacy_filtering_enabled) {
  if (privacy_filtering_enabled)
    return;

  auto* extension_descriptor = handle->BeginNestedMessage<protozero::Message>(
      perfetto::protos::pbzero::TracePacket::kExtensionDescriptorFieldNumber);
  scoped_refptr<base::RefCountedMemory> descriptor_bytes(
      GetContentClient()->GetDataResourceBytes(
          chrome_track_event_extension_descriptor));
  if (!descriptor_bytes)
    return;
  extension_descriptor->AppendBytes(
      perfetto::protos::pbzero::ExtensionDescriptor::kExtensionSetFieldNumber,
      descriptor_bytes->data(), descriptor_bytes->size());
}

TracingControllerImpl* TracingControllerImpl::GetInstance() {
  DCHECK(g_tracing_controller);
  return g_tracing_controller;
}

bool TracingControllerImpl::GetCategories(GetCategoriesDoneCallback callback) {
  std::set<std::string> category_set;

  AddCategoriesToSet(base::perfetto_track_event::internal::kCategoryRegistry,
                     category_set);
  AddCategoriesToSet(v8::GetTrackEventCategoryRegistry(), category_set);
  AddCategoriesToSet(GetWebRtcTrackEventCategoryRegistry(), category_set);

  std::move(callback).Run(category_set);
  return true;
}

bool TracingControllerImpl::StartTracing(
    const base::trace_event::TraceConfig& trace_config,
    StartTracingDoneCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(chiniforooshan): The actual value should be received by callback and
  // this function should return void.
  if (IsTracing()) {
    // Do not allow updating trace config when process filter is not used.
    if (trace_config.process_filter_config().empty() ||
        trace_config_->process_filter_config().empty()) {
      return false;
    }
    // Make sure other parts of trace_config (besides process filter)
    // did not change.
    base::trace_event::TraceConfig old_config_copy(*trace_config_);
    base::trace_event::TraceConfig new_config_copy(trace_config);
    old_config_copy.SetProcessFilterConfig(
        base::trace_event::TraceConfig::ProcessFilterConfig());
    new_config_copy.SetProcessFilterConfig(
        base::trace_event::TraceConfig::ProcessFilterConfig());
    if (old_config_copy.ToString() != new_config_copy.ToString())
      return false;
  }
  trace_config_ =
      std::make_unique<base::trace_event::TraceConfig>(trace_config);

  DCHECK(!tracing_session_host_);
  ConnectToServiceIfNeeded();

  perfetto::TraceConfig perfetto_config =
      tracing::GetDefaultPerfettoConfig(trace_config,
                                        /*privacy_filtering_enabled=*/false,
                                        /*convert_to_legacy_json=*/true);

  consumer_host_->EnableTracing(
      tracing_session_host_.BindNewPipeAndPassReceiver(),
      receiver_.BindNewPipeAndPassRemote(), std::move(perfetto_config),
      base::File());
  receiver_.set_disconnect_handler(base::BindOnce(
      &TracingControllerImpl::OnTracingFailed, base::Unretained(this)));
  tracing_session_host_.set_disconnect_handler(base::BindOnce(
      &TracingControllerImpl::OnTracingFailed, base::Unretained(this)));

  start_tracing_callback_ = std::move(callback);

  // TODO(chiniforooshan): The actual success value should be sent by the
  // callback asynchronously.
  return true;
}

bool TracingControllerImpl::StopTracing(
    const scoped_refptr<TraceDataEndpoint>& trace_data_endpoint) {
  return StopTracing(std::move(trace_data_endpoint), "");
}

bool TracingControllerImpl::StopTracing(
    const scoped_refptr<TraceDataEndpoint>& trace_data_endpoint,
    const std::string& agent_label,
    bool privacy_filtering_enabled) {
  if (!IsTracing() || drainer_ || !tracing_session_host_)
    return false;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Setting the argument filter is no longer supported just in the TraceConfig;
  // clients of the TracingController that need filtering need to pass that
  // option to StopTracing directly as an argument. This is due to Perfetto-
  // based tracing requiring this filtering to be done during serialization
  // time and not during tracing time.
  // TODO(oysteine): Remove the config option once the legacy IPC layer is
  // removed.
  CHECK(privacy_filtering_enabled || !trace_config_->IsArgumentFilterEnabled());

  trace_data_endpoint_ = std::move(trace_data_endpoint);
  is_data_complete_ = false;
  read_buffers_complete_ = false;

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult result =
      mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle);
  if (result != MOJO_RESULT_OK) {
    CompleteFlush();
    return true;
  }

  drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(consumer_handle));

  tracing_session_host_->DisableTracingAndEmitJson(
      agent_label, std::move(producer_handle), privacy_filtering_enabled,
      base::BindOnce(&TracingControllerImpl::OnReadBuffersComplete,
                     base::Unretained(this)));

  // TODO(chiniforooshan): Is the return value used anywhere?
  return true;
}

bool TracingControllerImpl::GetTraceBufferUsage(
    GetTraceBufferUsageCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!tracing_session_host_) {
    std::move(callback).Run(0.0, 0);
    return true;
  }

  tracing_session_host_->RequestBufferUsage(base::BindOnce(
      [](GetTraceBufferUsageCallback callback, bool success, float percent_full,
         bool data_loss) { std::move(callback).Run(percent_full, 0); },
      std::move(callback)));
  // TODO(chiniforooshan): The actual success value should be sent by the
  // callback asynchronously.
  return true;
}

bool TracingControllerImpl::IsTracing() {
  return trace_config_ != nullptr;
}

void TracingControllerImpl::OnTracingEnabled() {
  if (start_tracing_callback_)
    std::move(start_tracing_callback_).Run();
}

void TracingControllerImpl::OnTracingDisabled(bool) {}

void TracingControllerImpl::OnTracingFailed() {
  CompleteFlush();
}

void TracingControllerImpl::OnDataAvailable(base::span<const uint8_t> data) {
  if (trace_data_endpoint_) {
    const std::string chunk(base::as_string_view(data));
    trace_data_endpoint_->ReceiveTraceChunk(
        std::make_unique<std::string>(chunk));
  }
}

void TracingControllerImpl::CompleteFlush() {
  if (trace_data_endpoint_)
    trace_data_endpoint_->ReceivedTraceFinalContents();

  trace_data_endpoint_ = nullptr;
  trace_config_ = nullptr;
  drainer_ = nullptr;
  tracing_session_host_.reset();
  receiver_.reset();
}

void TracingControllerImpl::OnDataComplete() {
  is_data_complete_ = true;
  if (read_buffers_complete_)
    CompleteFlush();
}

void TracingControllerImpl::OnReadBuffersComplete() {
  read_buffers_complete_ = true;
  if (is_data_complete_)
    CompleteFlush();
}

#if BUILDFLAG(IS_CHROMEOS)
void TracingControllerImpl::OnMachineStatisticsLoaded() {
  if (const std::optional<std::string_view> hardware_class =
          ash::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
              ash::system::kHardwareClassKey)) {
    hardware_class_ = std::string(hardware_class.value());
  }
  are_statistics_loaded_ = true;
}
#endif

}  // namespace content
