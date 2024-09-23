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
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "base/tracing/protos/grit/tracing_proto_resources.h"
#include "base/values.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/tracing/common/trace_to_console.h"
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
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "services/tracing/public/cpp/traced_process_impl.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/perfetto/include/perfetto/protozero/message.h"
#include "third_party/perfetto/protos/perfetto/trace/extension_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "v8/include/v8-version-string.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#include "base/debug/elf_reader.h"
#include "content/browser/android/tracing_controller_android.h"
#include "services/tracing/public/cpp/perfetto/java_heap_profiler/java_heap_profiler_android.h"

// Symbol with virtual address of the start of ELF header of the current binary.
extern char __ehdr_start;
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {

namespace {

TracingControllerImpl* g_tracing_controller = nullptr;

std::string GetNetworkTypeString() {
  switch (net::NetworkChangeNotifier::GetConnectionType()) {
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return "Ethernet";
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return "WiFi";
    case net::NetworkChangeNotifier::CONNECTION_2G:
      return "2G";
    case net::NetworkChangeNotifier::CONNECTION_3G:
      return "3G";
    case net::NetworkChangeNotifier::CONNECTION_4G:
      return "4G";
    case net::NetworkChangeNotifier::CONNECTION_5G:
      return "5G";
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      return "None";
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return "Bluetooth";
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
    default:
      break;
  }
  return "Unknown";
}

std::string GetClockString() {
  switch (base::TimeTicks::GetClock()) {
    case base::TimeTicks::Clock::FUCHSIA_ZX_CLOCK_MONOTONIC:
      return "FUCHSIA_ZX_CLOCK_MONOTONIC";
    case base::TimeTicks::Clock::LINUX_CLOCK_MONOTONIC:
      return "LINUX_CLOCK_MONOTONIC";
    case base::TimeTicks::Clock::IOS_CF_ABSOLUTE_TIME_MINUS_KERN_BOOTTIME:
      return "IOS_CF_ABSOLUTE_TIME_MINUS_KERN_BOOTTIME";
    case base::TimeTicks::Clock::MAC_MACH_ABSOLUTE_TIME:
      return "MAC_MACH_ABSOLUTE_TIME";
    case base::TimeTicks::Clock::WIN_QPC:
      return "WIN_QPC";
    case base::TimeTicks::Clock::WIN_ROLLOVER_PROTECTED_TIME_GET_TIME:
      return "WIN_ROLLOVER_PROTECTED_TIME_GET_TIME";
  }

  NOTREACHED_IN_MIGRATION();
  return std::string();
}

#if BUILDFLAG(IS_ANDROID)
int64_t ConvertTimespecToMicros(const struct timespec& ts) {
  // On 32-bit systems, the calculation cannot overflow int64_t.
  // 2**32 * 1000000 + 2**64 / 1000 < 2**63
  if (sizeof(ts.tv_sec) <= 4 && sizeof(ts.tv_nsec) <= 8) {
    int64_t result = ts.tv_sec;
    result *= base::Time::kMicrosecondsPerSecond;
    result += (ts.tv_nsec / base::Time::kNanosecondsPerMicrosecond);
    return result;
  }
  base::CheckedNumeric<int64_t> result(ts.tv_sec);
  result *= base::Time::kMicrosecondsPerSecond;
  result += (ts.tv_nsec / base::Time::kNanosecondsPerMicrosecond);
  return result.ValueOrDie();
}

// This returns the offset between the monotonic clock and the realtime clock.
// We could read btime from /proc/status files; however, btime can be off by
// around 1s, which is too much. The following method should give us a better
// approximation of the offset.
std::string GetClockOffsetSinceEpoch() {
  struct timespec realtime_before, monotonic, realtime_after;
  clock_gettime(CLOCK_REALTIME, &realtime_before);
  clock_gettime(CLOCK_MONOTONIC, &monotonic);
  clock_gettime(CLOCK_REALTIME, &realtime_after);
  return base::NumberToString(ConvertTimespecToMicros(realtime_before) / 2 +
                              ConvertTimespecToMicros(realtime_after) / 2 -
                              ConvertTimespecToMicros(monotonic));
}
#endif

bool IsSpecialCategory(const std::string& name) {
  return name == "__metadata" || name == "tracing_already_shutdown" ||
         name == "tracing_categories_exhausted._must_increase_kMaxCategories";
}

}  // namespace

TracingController* TracingController::GetInstance() {
  return TracingControllerImpl::GetInstance();
}

TracingControllerImpl::TracingControllerImpl() {
  DCHECK(!g_tracing_controller);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Deliberately leaked, like this class.
  base::FileTracing::SetProvider(new FileTracingProviderImpl);
  AddAgents();
  g_tracing_controller = this;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Bind hwclass once the statistics are available.
  ash::system::StatisticsProvider::GetInstance()
      ->ScheduleOnMachineStatisticsLoaded(
          base::BindOnce(&TracingControllerImpl::OnMachineStatisticsLoaded,
                         weak_ptr_factory_.GetWeakPtr()));
#endif

  tracing::PerfettoTracedProcess::Get()->SetConsumerConnectionFactory(
      &GetTracingService, base::SingleThreadTaskRunner::GetCurrentDefault());
}

TracingControllerImpl::~TracingControllerImpl() = default;

void TracingControllerImpl::AddAgents() {
  tracing::TracedProcessImpl::GetInstance()->SetTaskRunner(
      base::SequencedTaskRunner::GetCurrentDefault());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  agents_.push_back(std::make_unique<CrOSTracingAgent>());
#elif defined(CAST_TRACING_AGENT)
  agents_.push_back(std::make_unique<CastTracingAgent>());
#endif

  // Ensure the TraceEventAgent has been created.
  tracing::TraceEventAgent::GetInstance();

  // For adding general CPU, network, OS, and other system information to the
  // metadata.
  auto* metadata_source = tracing::TraceEventMetadataSource::GetInstance();
  metadata_source->AddGeneratorFunction(base::BindRepeating(
      &TracingControllerImpl::GenerateMetadataDict, base::Unretained(this)));
  metadata_source->AddGeneratorFunction(base::BindRepeating(
      &TracingControllerImpl::GenerateMetadataPacketFieldTrials,
      base::Unretained(this)));
  metadata_source->AddGeneratorFunction(base::BindRepeating(
      &TracingControllerImpl::GenerateMetadataPacket, base::Unretained(this)));
#if BUILDFLAG(IS_ANDROID)
  tracing::PerfettoTracedProcess::Get()->AddDataSource(
      tracing::JavaHeapProfiler::GetInstance());
#endif
}

void TracingControllerImpl::GenerateMetadataPacketFieldTrials(
    perfetto::protos::pbzero::ChromeMetadataPacket* metadata_proto,
    bool privacy_filtering_enabled) {
  // Do not include low anonymity field trials, to prevent them from being
  // included in chrometto reports.
  std::vector<variations::ActiveGroupId> active_group_ids;
  variations::GetFieldTrialActiveGroupIds(std::string_view(),
                                          &active_group_ids);

  for (const auto& active_group_id : active_group_ids) {
    perfetto::protos::pbzero::ChromeMetadataPacket::FinchHash* finch_hash =
        metadata_proto->add_field_trial_hashes();
    finch_hash->set_name(active_group_id.name);
    finch_hash->set_group(active_group_id.group);
  }
}

void TracingControllerImpl::ConnectToServiceIfNeeded() {
  if (!consumer_host_) {
    GetTracingService().BindConsumerHost(
        consumer_host_.BindNewPipeAndPassReceiver());
    consumer_host_.reset_on_disconnect();
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
      GetContentClient()->GetDataResourceBytes(chrome_track_event_descriptor));
  if (!descriptor_bytes)
    return;
  extension_descriptor->AppendBytes(
      perfetto::protos::pbzero::ExtensionDescriptor::kExtensionSetFieldNumber,
      descriptor_bytes->data(), descriptor_bytes->size());
}

// Can be called on any thread.
std::optional<base::Value::Dict> TracingControllerImpl::GenerateMetadataDict() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto metadata_dict =
      base::Value::Dict()
          .Set("network-type", GetNetworkTypeString())
          .Set("product-version", GetContentClient()->browser()->GetProduct())
          .Set("v8-version", V8_VERSION_STRING)
          .Set("user-agent", GetContentClient()->browser()->GetUserAgent())
          .Set("revision", version_info::GetLastChange());

#if BUILDFLAG(IS_ANDROID)
  // The library name is used for symbolizing heap profiles. This cannot be
  // obtained from process maps since library can be mapped from apk directly.
  // This is not added as part of memory-infra os dumps since it is special case
  // only for chrome library.
  std::optional<std::string_view> soname =
      base::debug::ReadElfLibraryName(&__ehdr_start);
  if (soname)
    metadata_dict.Set("chrome-library-name", *soname);
  metadata_dict.Set("clock-offset-since-epoch", GetClockOffsetSinceEpoch());
#endif  // BUILDFLAG(IS_ANDROID)
  metadata_dict.Set("chrome-bitness", static_cast<int>(8 * sizeof(uintptr_t)));

#if DCHECK_IS_ON()
  metadata_dict.Set("chrome-dcheck-on", 1);
#endif

  // OS
#if BUILDFLAG(IS_CHROMEOS_ASH)
  metadata_dict.Set("os-name", "CrOS");
  if (are_statistics_loaded_)
    metadata_dict.Set("hardware-class", hardware_class_);
#else
  metadata_dict.Set("os-name", base::SysInfo::OperatingSystemName());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  metadata_dict.Set("os-version", base::SysInfo::OperatingSystemVersion());
#if BUILDFLAG(IS_WIN)
  if (base::win::OSInfo::GetArchitecture() ==
      base::win::OSInfo::X64_ARCHITECTURE) {
    if (base::win::OSInfo::GetInstance()->IsWowX86OnAMD64()) {
      metadata_dict.Set("os-wow64", "enabled");
    } else {
      metadata_dict.Set("os-wow64", "disabled");
    }
  }

  metadata_dict.Set("module-apphelp", (::GetModuleHandle(L"apphelp.dll"))
                                          ? "Loaded"
                                          : "NotLoaded");

  metadata_dict.Set("os-session",
                    base::win::IsCurrentSessionRemote() ? "remote" : "local");
#endif

  metadata_dict.Set("os-arch", base::SysInfo::OperatingSystemArchitecture());

  // CPU
  base::CPU cpu;
  metadata_dict.Set("cpu-family", cpu.family());
  metadata_dict.Set("cpu-model", cpu.model());
  metadata_dict.Set("cpu-stepping", cpu.stepping());
  metadata_dict.Set("num-cpus", base::SysInfo::NumberOfProcessors());
  metadata_dict.Set("physical-memory",
                    base::SysInfo::AmountOfPhysicalMemoryMB());

  metadata_dict.Set("cpu-brand", cpu.cpu_brand());

#if BUILDFLAG(IS_WIN)
  base::GenerateCpuInfoForTracingMetadata(&metadata_dict);
#endif

  // GPU
  const gpu::GPUInfo gpu_info =
      content::GpuDataManagerImpl::GetInstance()->GetGPUInfo();
  const gpu::GPUInfo::GPUDevice& active_gpu = gpu_info.active_gpu();

#if !BUILDFLAG(IS_ANDROID)
  metadata_dict.Set("gpu-venid", static_cast<int>(active_gpu.vendor_id));
  metadata_dict.Set("gpu-devid", static_cast<int>(active_gpu.device_id));
#endif

  metadata_dict.Set("gpu-driver", active_gpu.driver_version);
  metadata_dict.Set("gpu-psver", gpu_info.pixel_shader_version);
  metadata_dict.Set("gpu-vsver", gpu_info.vertex_shader_version);

#if BUILDFLAG(IS_MAC)
  metadata_dict.Set("gpu-glver", gpu_info.gl_version);
#elif BUILDFLAG(IS_POSIX)
  metadata_dict.Set("gpu-gl-vendor", gpu_info.gl_vendor);
  metadata_dict.Set("gpu-gl-renderer", gpu_info.gl_renderer);
#endif
  metadata_dict.Set("gpu-features", GetFeatureStatus());

  metadata_dict.Set("clock-domain", GetClockString());
  metadata_dict.Set("highres-ticks", base::TimeTicks::IsHighResolution());

  base::CommandLine::StringType command_line =
      base::CommandLine::ForCurrentProcess()->GetCommandLineString();
#if BUILDFLAG(IS_WIN)
  metadata_dict.Set("command_line", base::WideToUTF16(command_line));
#else
  metadata_dict.Set("command_line", command_line);
#endif

  metadata_dict.Set(
      "trace-capture-datetime",
      base::UnlocalizedTimeFormatWithPattern(TRACE_TIME_NOW(), "y-M-d H:m:s",
                                             icu::TimeZone::getGMT()));

  // TODO(crbug.com/40527661): The central controller doesn't know about
  // metadata filters, so we temporarily filter here as the controller is
  // what assembles the full trace data.
  base::trace_event::MetadataFilterPredicate metadata_filter;
  if (trace_config_ && trace_config_->IsArgumentFilterEnabled()) {
    metadata_filter = base::trace_event::TraceLog::GetInstance()
                          ->GetMetadataFilterPredicate();
  }

  if (!metadata_filter.is_null()) {
    for (auto it : metadata_dict) {
      if (!metadata_filter.Run(it.first)) {
        it.second = base::Value("__stripped__");
      }
    }
  }

  return metadata_dict;
}

TracingControllerImpl* TracingControllerImpl::GetInstance() {
  DCHECK(g_tracing_controller);
  return g_tracing_controller;
}

bool TracingControllerImpl::GetCategories(GetCategoriesDoneCallback callback) {
  std::set<std::string> category_set;

  using base::perfetto_track_event::internal::kCategoryRegistry;
  for (size_t i = 0; i < kCategoryRegistry.category_count(); ++i) {
    std::string category_name = kCategoryRegistry.GetCategory(i)->name;
    // Only add single categories, not groups. Also exclude special categories.
    if (category_name.find(',') == std::string::npos &&
        !IsSpecialCategory(category_name)) {
      category_set.insert(category_name);
    }
  }

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

  perfetto::TraceConfig perfetto_config = tracing::GetDefaultPerfettoConfig(
      trace_config,
      /*privacy_filtering_enabled=*/false,
      /*convert_to_legacy_json=*/true,
      perfetto::protos::gen::ChromeConfig::USER_INITIATED);

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
