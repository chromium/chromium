// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/tracing_controller_impl.h"

#include <inttypes.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/dcheck_is_on.h"
#include "base/files/file_tracing.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/tracing/common/trace_startup_config.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/tracing/file_tracing_provider_impl.h"
#include "content/browser/tracing/perfetto_file_tracer.h"
#include "content/browser/tracing/tracing_ui.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/browser/tracing_service.h"
#include "content/public/common/content_client.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/network_change_notifier.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "services/tracing/public/cpp/traced_process_impl.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "v8/include/v8-version-string.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/system/statistics_provider.h"
#include "content/browser/tracing/cros_tracing_agent.h"
#endif

#if defined(CAST_TRACING_AGENT)
#include "content/browser/tracing/cast_tracing_agent.h"
#endif

#if defined(OS_WIN)
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#endif

#if defined(OS_ANDROID)
#include <sys/time.h>
#include "base/debug/elf_reader.h"
#include "content/browser/android/tracing_controller_android.h"
#include "services/tracing/public/cpp/perfetto/java_heap_profiler/java_heap_profiler_android.h"

// Symbol with virtual address of the start of ELF header of the current binary.
extern char __ehdr_start;
#endif  // defined(OS_ANDROID)

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

  NOTREACHED();
  return std::string();
}

#if defined(OS_ANDROID)
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
  return base::StringPrintf("%" PRId64,
                            ConvertTimespecToMicros(realtime_before) / 2 +
                                ConvertTimespecToMicros(realtime_after) / 2 -
                                ConvertTimespecToMicros(monotonic));
}
#endif

void OnStoppedStartupTracing(const base::FilePath& trace_file) {
  VLOG(0) << "Completed startup tracing to " << trace_file.value();
  tracing::TraceStartupConfig::GetInstance()->OnTraceToResultFileFinished();
}

}  // namespace

TracingController* TracingController::GetInstance() {
  return TracingControllerImpl::GetInstance();
}

TracingControllerImpl::TracingControllerImpl()
    : delegate_(GetContentClient()->browser()->GetTracingDelegate()) {
  DCHECK(!g_tracing_controller);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Deliberately leaked, like this class.
  base::FileTracing::SetProvider(new FileTracingProviderImpl);
  AddAgents();
  g_tracing_controller = this;

  // TODO(oysteine): Startup tracing using Perfetto
  // is enabled by the Mojo consumer in content/browser
  // for now; this is too late in the browser startup
  // process however.
  if (PerfettoFileTracer::ShouldEnable())
    perfetto_file_tracer_ = std::make_unique<PerfettoFileTracer>();

#if defined(OS_CHROMEOS)
  // Bind hwclass once the statistics are available.
  chromeos::system::StatisticsProvider::GetInstance()
      ->ScheduleOnMachineStatisticsLoaded(
          base::BindOnce(&TracingControllerImpl::OnMachineStatisticsLoaded,
                         weak_ptr_factory_.GetWeakPtr()));
#endif

  tracing::PerfettoTracedProcess::Get()->SetConsumerConnectionFactory(
      &GetTracingService, base::ThreadTaskRunnerHandle::Get());
}

TracingControllerImpl::~TracingControllerImpl() = default;

void TracingControllerImpl::AddAgents() {
  tracing::TracedProcessImpl::GetInstance()->SetTaskRunner(
      base::SequencedTaskRunnerHandle::Get());

#if defined(OS_CHROMEOS)
  agents_.push_back(std::make_unique<CrOSTracingAgent>());
#elif defined(CAST_TRACING_AGENT)
  agents_.push_back(std::make_unique<CastTracingAgent>());
#endif

  // For adding general CPU, network, OS, and other system information to the
  // metadata.
  auto* trace_event_agent = tracing::TraceEventAgent::GetInstance();
  trace_event_agent->AddMetadataGeneratorFunction(base::BindRepeating(
      &TracingControllerImpl::GenerateMetadataDict, base::Unretained(this)));
  if (delegate_) {
    trace_event_agent->AddMetadataGeneratorFunction(
        base::BindRepeating(&TracingDelegate::GenerateMetadataDict,
                            base::Unretained(delegate_.get())));
  }
#if defined(OS_ANDROID)
  tracing::PerfettoTracedProcess::Get()->AddDataSource(
      tracing::JavaHeapProfiler::GetInstance());
#endif
}

void TracingControllerImpl::ConnectToServiceIfNeeded() {
  if (!consumer_host_) {
    GetTracingService().BindConsumerHost(
        consumer_host_.BindNewPipeAndPassReceiver());
    consumer_host_.reset_on_disconnect();
  }
}

// Can be called on any thread.
std::unique_ptr<base::DictionaryValue>
TracingControllerImpl::GenerateMetadataDict() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto metadata_dict = std::make_unique<base::DictionaryValue>();

  metadata_dict->SetString("network-type", GetNetworkTypeString());
  metadata_dict->SetString("product-version",
                           GetContentClient()->browser()->GetProduct());
  metadata_dict->SetString("v8-version", V8_VERSION_STRING);
  metadata_dict->SetString("user-agent",
                           GetContentClient()->browser()->GetUserAgent());

#if defined(OS_ANDROID)
  // The library name is used for symbolizing heap profiles. This cannot be
  // obtained from process maps since library can be mapped from apk directly.
  // This is not added as part of memory-infra os dumps since it is special case
  // only for chrome library.
  base::Optional<base::StringPiece> soname =
      base::debug::ReadElfLibraryName(&__ehdr_start);
  if (soname)
    metadata_dict->SetString("chrome-library-name", *soname);
  metadata_dict->SetString("clock-offset-since-epoch",
                           GetClockOffsetSinceEpoch());
#endif  // defined(OS_ANDROID)
  metadata_dict->SetInteger("chrome-bitness", 8 * sizeof(uintptr_t));

#if DCHECK_IS_ON()
  metadata_dict->SetInteger("chrome-dcheck-on", 1);
#endif

  // OS
#if defined(OS_CHROMEOS)
  metadata_dict->SetString("os-name", "CrOS");
  if (are_statistics_loaded_)
    metadata_dict->SetString("hardware-class", hardware_class_);
#else
  metadata_dict->SetString("os-name", base::SysInfo::OperatingSystemName());
#endif  // defined(OS_CHROMEOS)
  metadata_dict->SetString("os-version",
                           base::SysInfo::OperatingSystemVersion());
#if defined(OS_WIN)
  if (base::win::OSInfo::GetArchitecture() ==
      base::win::OSInfo::X64_ARCHITECTURE) {
    if (base::win::OSInfo::GetInstance()->wow64_status() ==
        base::win::OSInfo::WOW64_ENABLED) {
      metadata_dict->SetString("os-wow64", "enabled");
    } else {
      metadata_dict->SetString("os-wow64", "disabled");
    }
  }

  metadata_dict->SetString(
      "os-session", base::win::IsCurrentSessionRemote() ? "remote" : "local");
#endif

  metadata_dict->SetString("os-arch",
                           base::SysInfo::OperatingSystemArchitecture());

  // CPU
  base::CPU cpu;
  metadata_dict->SetInteger("cpu-family", cpu.family());
  metadata_dict->SetInteger("cpu-model", cpu.model());
  metadata_dict->SetInteger("cpu-stepping", cpu.stepping());
  metadata_dict->SetInteger("num-cpus", base::SysInfo::NumberOfProcessors());
  metadata_dict->SetInteger("physical-memory",
                            base::SysInfo::AmountOfPhysicalMemoryMB());

  metadata_dict->SetString("cpu-brand", cpu.cpu_brand());

  // GPU
  const gpu::GPUInfo gpu_info =
      content::GpuDataManagerImpl::GetInstance()->GetGPUInfo();
  const gpu::GPUInfo::GPUDevice& active_gpu = gpu_info.active_gpu();

#if !defined(OS_ANDROID)
  metadata_dict->SetInteger("gpu-venid", active_gpu.vendor_id);
  metadata_dict->SetInteger("gpu-devid", active_gpu.device_id);
#endif

  metadata_dict->SetString("gpu-driver", active_gpu.driver_version);
  metadata_dict->SetString("gpu-psver", gpu_info.pixel_shader_version);
  metadata_dict->SetString("gpu-vsver", gpu_info.vertex_shader_version);

#if defined(OS_MAC)
  metadata_dict->SetString("gpu-glver", gpu_info.gl_version);
#elif defined(OS_POSIX)
  metadata_dict->SetString("gpu-gl-vendor", gpu_info.gl_vendor);
  metadata_dict->SetString("gpu-gl-renderer", gpu_info.gl_renderer);
#endif

  metadata_dict->SetString("clock-domain", GetClockString());
  metadata_dict->SetBoolean("highres-ticks",
                            base::TimeTicks::IsHighResolution());

  metadata_dict->SetString(
      "command_line",
      base::CommandLine::ForCurrentProcess()->GetCommandLineString());

  base::Time::Exploded ctime;
  TRACE_TIME_NOW().UTCExplode(&ctime);
  std::string time_string = base::StringPrintf(
      "%u-%u-%u %d:%d:%d", ctime.year, ctime.month, ctime.day_of_month,
      ctime.hour, ctime.minute, ctime.second);
  metadata_dict->SetString("trace-capture-datetime", time_string);

  // TODO(crbug.com/737049): The central controller doesn't know about
  // metadata filters, so we temporarily filter here as the controller is
  // what assembles the full trace data.
  base::trace_event::MetadataFilterPredicate metadata_filter;
  if (trace_config_ && trace_config_->IsArgumentFilterEnabled()) {
    metadata_filter = base::trace_event::TraceLog::GetInstance()
                          ->GetMetadataFilterPredicate();
  }

  if (!metadata_filter.is_null()) {
    for (base::DictionaryValue::Iterator it(*metadata_dict); !it.IsAtEnd();
         it.Advance()) {
      if (!metadata_filter.Run(it.key())) {
        metadata_dict->SetString(it.key(), "__stripped__");
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
  tracing::TracedProcessImpl::GetInstance()->GetCategories(&category_set);

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
      trace_config, /*requires_anonymized_data=*/false);

  consumer_host_->EnableTracing(
      tracing_session_host_.BindNewPipeAndPassReceiver(),
      receiver_.BindNewPipeAndPassRemote(), std::move(perfetto_config),
      tracing::mojom::TracingClientPriority::kUserInitiated);
  receiver_.set_disconnect_handler(base::BindOnce(
      &TracingControllerImpl::OnTracingFailed, base::Unretained(this)));
  tracing_session_host_.set_disconnect_handler(base::BindOnce(
      &TracingControllerImpl::OnTracingFailed, base::Unretained(this)));

  start_tracing_callback_ = std::move(callback);

  // TODO(chiniforooshan): The actual success value should be sent by the
  // callback asynchronously.
  return true;
}

void TracingControllerImpl::StartStartupTracingIfNeeded() {
  auto* trace_startup_config = tracing::TraceStartupConfig::GetInstance();
  if (trace_startup_config->AttemptAdoptBySessionOwner(
          tracing::TraceStartupConfig::SessionOwner::kTracingController)) {
    StartTracing(trace_startup_config->GetTraceConfig(),
                 StartTracingDoneCallback());
  } else if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kTraceToConsole)) {
    StartTracing(tracing::GetConfigForTraceToConsole(),
                 StartTracingDoneCallback());
  }

  if (trace_startup_config->IsTracingStartupForDuration()) {
    TRACE_EVENT0("startup",
                 "TracingControllerImpl::InitStartupTracingForDuration");
    InitStartupTracingForDuration();
  }
}

base::FilePath TracingControllerImpl::GetStartupTraceFileName() const {
  base::FilePath trace_file;

  trace_file = tracing::TraceStartupConfig::GetInstance()->GetResultFile();
  if (trace_file.empty()) {
#if defined(OS_ANDROID)
    TracingControllerAndroid::GenerateTracingFilePath(&trace_file);
#else
    // Default to saving the startup trace into the current dir.
    trace_file = base::FilePath().AppendASCII("chrometrace.log");
#endif
  }

  return trace_file;
}

void TracingControllerImpl::InitStartupTracingForDuration() {
  DCHECK(tracing::TraceStartupConfig::GetInstance()
             ->IsTracingStartupForDuration());

  startup_trace_file_ = GetStartupTraceFileName();

  startup_trace_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(
          tracing::TraceStartupConfig::GetInstance()->GetStartupDuration()),
      this, &TracingControllerImpl::EndStartupTracing);
}

void TracingControllerImpl::EndStartupTracing() {
  // Do nothing if startup tracing is already stopped.
  if (!tracing::TraceStartupConfig::GetInstance()->IsEnabled())
    return;

  // Use USER_VISIBLE priority because BEST_EFFORT tasks are not run at startup
  // and we want the trace file to be written soon.
  StopTracing(CreateFileEndpoint(
      startup_trace_file_,
      base::BindOnce(OnStoppedStartupTracing, startup_trace_file_),
      base::TaskPriority::USER_VISIBLE));
}

void TracingControllerImpl::FinalizeStartupTracingIfNeeded() {
  // There are two cases:
  // 1. Startup duration is not reached.
  // 2. Or if the trace should be saved to file for --trace-config-file flag.
  base::Optional<base::FilePath> startup_trace_file;
  if (startup_trace_timer_.IsRunning()) {
    startup_trace_timer_.Stop();
    if (startup_trace_file_ != base::FilePath().AppendASCII("none")) {
      startup_trace_file = startup_trace_file_;
    }
  } else if (tracing::TraceStartupConfig::GetInstance()
                 ->ShouldTraceToResultFile()) {
    startup_trace_file = GetStartupTraceFileName();
  }
  if (!startup_trace_file)
    return;
  // Perfetto currently doesn't support tracing during shutdown as the trace
  // buffer is lost when the service is shut down, so we wait until the trace is
  // complete. See also crbug.com/944107.
  // TODO(eseckler): Avoid the nestedRunLoop here somehow.
  base::RunLoop run_loop;
  // We may not have completed startup yet when we attempt to write the trace,
  // and thus tasks with BEST_EFFORT may not be run. Choose a non-background
  // priority to avoid blocking forever.
  const base::TaskPriority kWritePriority = base::TaskPriority::USER_VISIBLE;
  bool success = StopTracing(CreateFileEndpoint(
      startup_trace_file.value(),
      base::BindOnce(
          [](base::FilePath trace_file, base::OnceClosure quit_closure) {
            OnStoppedStartupTracing(trace_file);
            std::move(quit_closure).Run();
          },
          startup_trace_file.value(), run_loop.QuitClosure()),
      kWritePriority));
  if (!success)
    return;
  run_loop.Run();
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

#if defined(OS_ANDROID)
  base::trace_event::TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  // Setting the argument filter is no longer supported just in the TraceConfig;
  // clients of the TracingController that need filtering need to pass that
  // option to StopTracing directly as an argument. This is due to Perfetto-
  // based tracing requiring this filtering to be done during serialization
  // time and not during tracing time.
  // TODO(oysteine): Remove the config option once the legacy IPC layer is
  // removed.
  CHECK(privacy_filtering_enabled || !trace_config_->IsArgumentFilterEnabled());

  tracing::TraceStartupConfig::GetInstance()->SetDisabled();
  trace_data_endpoint_ = std::move(trace_data_endpoint);
  is_data_complete_ = false;
  read_buffers_complete_ = false;

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult result =
      mojo::CreateDataPipe(nullptr, &producer_handle, &consumer_handle);
  if (result != MOJO_RESULT_OK) {
    CompleteFlush();
    return true;
  }

  drainer_.reset(new mojo::DataPipeDrainer(this, std::move(consumer_handle)));

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

void TracingControllerImpl::OnTracingDisabled() {}

void TracingControllerImpl::OnTracingFailed() {
  CompleteFlush();
}

void TracingControllerImpl::OnDataAvailable(const void* data,
                                            size_t num_bytes) {
  if (trace_data_endpoint_) {
    const std::string chunk(static_cast<const char*>(data), num_bytes);
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

#if defined(OS_CHROMEOS)
void TracingControllerImpl::OnMachineStatisticsLoaded() {
  chromeos::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
      chromeos::system::kHardwareClassKey, &hardware_class_);
  are_statistics_loaded_ = true;
}
#endif

void TracingControllerImpl::SetTracingDelegateForTesting(
    std::unique_ptr<TracingDelegate> delegate) {
  if (!delegate) {
    delegate_.reset(GetContentClient()->browser()->GetTracingDelegate());
  } else {
    delegate_ = std::move(delegate);
  }
}

}  // namespace content
