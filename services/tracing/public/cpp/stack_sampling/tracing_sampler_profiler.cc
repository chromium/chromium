// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"

#include <limits>
#include <set>
#include <string_view>

#include "base/android/library_loader/anchor_functions.h"
#include "base/debug/leak_annotations.h"
#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/buildflags.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/profile_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.pbzero.h"

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_APPLE)
#include "base/profiler/thread_delegate_posix.h"
#define INITIALIZE_THREAD_DELEGATE_POSIX 1
#else  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_APPLE)
#define INITIALIZE_THREAD_DELEGATE_POSIX 0
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_APPLE)

#if ANDROID_ARM64_UNWINDING_SUPPORTED || ANDROID_CFI_UNWINDING_SUPPORTED
#include <dlfcn.h>
#include "base/debug/elf_reader.h"
#endif  // ANDROID_ARM64_UNWINDING_SUPPORTED || ANDROID_CFI_UNWINDING_SUPPORTED

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
#include "services/tracing/public/cpp/stack_sampling/loader_lock_sampling_thread_win.h"
#endif

using StreamingProfilePacketHandle =
    protozero::MessageHandle<perfetto::protos::pbzero::StreamingProfilePacket>;
using TracePacketHandle = perfetto::TraceWriter::TracePacketHandle;

namespace tracing {

namespace {

#if ANDROID_ARM64_UNWINDING_SUPPORTED || ANDROID_CFI_UNWINDING_SUPPORTED
extern "C" {

// The address of |__executable_start| gives the start address of the
// executable or shared library. This value is used to find the offset address
// of the instruction in binary from PC.
extern char __executable_start;

}  // extern "C"

bool is_chrome_address(uintptr_t pc) {
  return pc >= base::android::kStartOfText && pc < base::android::kEndOfText;
}

uintptr_t executable_start_addr() {
  return reinterpret_cast<uintptr_t>(&__executable_start);
}
#endif  // ANDROID_ARM64_UNWINDING_SUPPORTED || ANDROID_CFI_UNWINDING_SUPPORTED

// Pointer to the main thread instance, if any.
TracingSamplerProfiler* g_main_thread_instance = nullptr;

class TracingSamplerProfilerDataSource;

TracingSamplerProfilerDataSource* g_sampler_profiler_ds_for_test = nullptr;

class TracingSamplerProfilerDataSource
    : public PerfettoTracedProcess::DataSourceBase {
 public:
  static TracingSamplerProfilerDataSource* Get() {
    static base::NoDestructor<TracingSamplerProfilerDataSource> instance;
    return instance.get();
  }

  void RegisterProfiler(TracingSamplerProfiler* profiler) {
    base::AutoLock lock(lock_);
    if (!profilers_.insert(profiler).second) {
      return;
    }

    if (is_started_) {
      profiler->StartTracing(
          CreateTraceWriter(),
          data_source_config_.chrome_config().privacy_filtering_enabled());
    } else if (is_startup_tracing_) {
      profiler->StartTracing(
          nullptr,
          /*should_enable_filtering=*/true);
    }
  }

  void UnregisterProfiler(TracingSamplerProfiler* profiler) {
    base::AutoLock lock(lock_);
    if (!profilers_.erase(profiler) || !(is_started_ || is_startup_tracing_)) {
      return;
    }

    profiler->StopTracing();
  }

  // PerfettoTracedProcess::DataSourceBase implementation, called by
  // ProducerClient.
  void StartTracingImpl(
      PerfettoProducer* producer,
      const perfetto::DataSourceConfig& data_source_config) override {
    base::AutoLock lock(lock_);
    DCHECK(!producer_);
    DCHECK(!is_started_);
    producer_ = producer;
    is_started_ = true;
    is_startup_tracing_ = false;
    data_source_config_ = data_source_config;

    bool should_enable_filtering =
        data_source_config.chrome_config().privacy_filtering_enabled();

    for (TracingSamplerProfiler* profiler : profilers_) {
      profiler->StartTracing(CreateTraceWriter(), should_enable_filtering);
    }
  }

  void StopTracingImpl(base::OnceClosure stop_complete_callback) override {
    base::AutoLock lock(lock_);
    DCHECK(is_started_);
    is_started_ = false;
    is_startup_tracing_ = false;
    producer_ = nullptr;

    for (TracingSamplerProfiler* profiler : profilers_) {
      profiler->StopTracing();
    }

    std::move(stop_complete_callback).Run();
  }

  void Flush(base::RepeatingClosure flush_complete_callback) override {
    flush_complete_callback.Run();
  }

  void SetupStartupTracing(PerfettoProducer* producer,
                           const base::trace_event::TraceConfig& trace_config,
                           bool privacy_filtering_enabled) override {
    bool enable_sampler_profiler = trace_config.IsCategoryGroupEnabled(
        TRACE_DISABLED_BY_DEFAULT("cpu_profiler"));
    if (!enable_sampler_profiler)
      return;

    base::AutoLock lock(lock_);
    if (is_started_) {
      return;
    }
    is_startup_tracing_ = true;
    for (TracingSamplerProfiler* profiler : profilers_) {
      // Enable filtering for startup tracing always to be safe.
      profiler->StartTracing(
          nullptr,
          /*should_enable_filtering=*/true);
    }
  }

  void AbortStartupTracing() override {
    base::AutoLock lock(lock_);
    if (!is_startup_tracing_) {
      return;
    }
    for (TracingSamplerProfiler* profiler : profilers_) {
      // Enable filtering for startup tracing always to be safe.
      profiler->StartTracing(
          nullptr,
          /*should_enable_filtering=*/true);
    }
    is_startup_tracing_ = false;
  }

  void ClearIncrementalState() override {
    incremental_state_reset_id_.fetch_add(1u, std::memory_order_relaxed);
  }

  static uint32_t GetIncrementalStateResetID() {
    return incremental_state_reset_id_.load(std::memory_order_relaxed);
  }
  using DataSourceProxy =
      PerfettoTracedProcess::DataSourceProxy<TracingSamplerProfilerDataSource>;

  static void ResetForTesting() {
    if (!g_sampler_profiler_ds_for_test)
      return;
    g_sampler_profiler_ds_for_test->~TracingSamplerProfilerDataSource();
    new (g_sampler_profiler_ds_for_test) TracingSamplerProfilerDataSource;
  }

  void RegisterDataSource() {
    perfetto::DataSourceDescriptor dsd;
    dsd.set_name(mojom::kSamplerProfilerSourceName);
    DataSourceProxy::Register(dsd, this);
  }

 private:
  friend class base::NoDestructor<TracingSamplerProfilerDataSource>;

  TracingSamplerProfilerDataSource()
      : DataSourceBase(mojom::kSamplerProfilerSourceName) {
    PerfettoTracedProcess::Get()->AddDataSource(this);
    g_sampler_profiler_ds_for_test = this;
  }

  ~TracingSamplerProfilerDataSource() override {
    // Unreachable because of static instance of type `base::NoDestructor<>`
    // and private ctr.
    // Reachable only in case of test mode. See `ResetForTesting()`.
  }

  // We create one trace writer per profiled thread both in SDK and non-SDK
  // build. This is necessary because each profiler keeps its own interned data
  // index, so to avoid collisions interned data should go into different
  // writer sequences.
  std::unique_ptr<perfetto::TraceWriterBase> CreateTraceWriter();

  // TODO(eseckler): Use GUARDED_BY annotations for all members below.
  base::Lock lock_;  // Protects subsequent members.
  raw_ptr<tracing::PerfettoProducer> producer_ GUARDED_BY(lock_) = nullptr;
  std::set<raw_ptr<TracingSamplerProfiler, SetExperimental>> profilers_;
  bool is_startup_tracing_ = false;
  bool is_started_ = false;
  perfetto::DataSourceConfig data_source_config_;

  static std::atomic<uint32_t> incremental_state_reset_id_;
};

using DataSourceProxy = TracingSamplerProfilerDataSource::DataSourceProxy;

// static
std::atomic<uint32_t>
    TracingSamplerProfilerDataSource::incremental_state_reset_id_{0};

base::SequenceLocalStorageSlot<TracingSamplerProfiler>&
GetSequenceLocalStorageProfilerSlot() {
  static base::SequenceLocalStorageSlot<TracingSamplerProfiler> storage;
  return storage;
}

// Stores information about the StackFrame, to emit to the trace.
struct FrameDetails {
  std::string frame_name;
  std::string module_name;
  std::string module_id;
  uintptr_t module_base_address = 0;
  uintptr_t rel_pc = 0;

  // True if the module of the stack frame will be considered valid by the trace
  // processor.
  bool has_valid_module() const {
    return !module_name.empty() && module_base_address > 0;
  }

  bool has_valid_frame() const {
    // Valid only if |rel_pc|, since filtering mode does not record frame names.
    return rel_pc > 0;
  }

  // Gets module from the frame's module cache.
  void SetModule(const base::ModuleCache::Module& module) {
    module_base_address = module.GetBaseAddress();
    module_id = module.GetId();
    if (module_name.empty()) {
      module_name = module.GetDebugBasename().MaybeAsASCII();
    }
  }

  // Leaves the valid fields as is and fills in dummy values for invalid fields.
  // Useful to observe errors in traces.
  void FillWithDummyFields(uintptr_t frame_ip) {
    if (rel_pc == 0) {
      // Record the |frame_ip| as |rel_pc| if available, might be useful to
      // debug.
      rel_pc = frame_ip > 0 ? frame_ip : 1;
    }
    if (module_base_address == 0) {
      module_base_address = 1;
    }
    // TODO(crbug.com/40248195): Investigate and maybe cleanup this logic.
    if (module_name.empty()) {
      module_name = "missing";
    }
    DCHECK(has_valid_frame());
    DCHECK(has_valid_module());
  }

#if ANDROID_ARM64_UNWINDING_SUPPORTED || ANDROID_CFI_UNWINDING_SUPPORTED
  // Sets Chrome's module info for the frame.
  void SetChromeModuleInfo() {
    module_base_address = executable_start_addr();
    static const std::optional<std::string_view> library_name =
        base::debug::ReadElfLibraryName(
            reinterpret_cast<void*>(executable_start_addr()));
    static const base::NoDestructor<std::string> chrome_debug_id([] {
      base::debug::ElfBuildIdBuffer build_id;
      size_t build_id_length = base::debug::ReadElfBuildId(
          reinterpret_cast<void*>(executable_start_addr()), true, build_id);
      return std::string(build_id, build_id_length);
    }());
    if (library_name) {
      module_name = std::string(*library_name);
    }
    module_id = *chrome_debug_id;
  }

  // Sets system library module info for the frame.
  void SetSystemModuleInfo(uintptr_t frame_ip) {
    Dl_info info = {};
    // For addresses in framework libraries, symbolize and write the function
    // name.
    if (dladdr(reinterpret_cast<void*>(frame_ip), &info) == 0) {
      return;
    }
    if (info.dli_sname) {
      frame_name = info.dli_sname;
    }
    if (info.dli_fname) {
      module_name = info.dli_fname;
    }
    module_base_address = reinterpret_cast<uintptr_t>(info.dli_fbase);
    rel_pc = frame_ip - module_base_address;

    DCHECK(has_valid_frame());
    DCHECK(has_valid_module());
  }
#endif
};

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN) && defined(_WIN64) ||          \
    ANDROID_ARM64_UNWINDING_SUPPORTED || ANDROID_CFI_UNWINDING_SUPPORTED || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
// Returns whether stack sampling is supported on the current platform.
bool IsStackSamplingSupported() {
  return base::StackSamplingProfiler::IsSupportedForCurrentPlatform();
}
#endif

perfetto::StaticString UnwinderTypeToString(
    const TracingSamplerProfiler::UnwinderType unwinder_type) {
  switch (unwinder_type) {
    case TracingSamplerProfiler::UnwinderType::kUnknown:
      return "TracingSamplerProfiler (unknown unwinder)";
    case TracingSamplerProfiler::UnwinderType::kCustomAndroid:
      return "TracingSamplerProfiler (custom android unwinder)";
    case TracingSamplerProfiler::UnwinderType::kDefault:
      return "TracingSamplerProfiler (default unwinder)";
    case TracingSamplerProfiler::UnwinderType::kLibunwindstackUnwinderAndroid:
      return "TracingSamplerProfiler (libunwindstack unwinder android)";
  }
}

}  // namespace

TracingSamplerProfiler::TracingProfileBuilder::BufferedSample::BufferedSample(
    base::TimeTicks ts,
    std::vector<base::Frame>&& s)
    : timestamp(ts), sample(std::move(s)) {}

TracingSamplerProfiler::TracingProfileBuilder::BufferedSample::
    ~BufferedSample() = default;

TracingSamplerProfiler::TracingProfileBuilder::BufferedSample::BufferedSample(
    TracingSamplerProfiler::TracingProfileBuilder::BufferedSample&& other)
    : BufferedSample(other.timestamp, std::move(other.sample)) {}

TracingSamplerProfiler::TracingProfileBuilder::TracingProfileBuilder(
    base::PlatformThreadId sampled_thread_id,
    std::unique_ptr<perfetto::TraceWriterBase> trace_writer,
    bool should_enable_filtering,
    const base::RepeatingClosure& sample_callback_for_testing)
    : sampled_thread_id_(sampled_thread_id),
      trace_writer_(std::move(trace_writer)),
      stack_profile_writer_(should_enable_filtering),
      sample_callback_for_testing_(sample_callback_for_testing) {}

TracingSamplerProfiler::TracingProfileBuilder::~TracingProfileBuilder() {
  // Deleting a TraceWriter can end up triggering a Mojo call which calls
  // task runner GetCurrentDefault() and isn't safe on thread shutdown, which is
  // when TracingProfileBuilder gets destructed, so we make sure this happens on
  // a different sequence.
  if (base::ThreadPoolInstance::Get()) {
    PerfettoTracedProcess::GetTaskRunner()->GetOrCreateTaskRunner()->DeleteSoon(
        FROM_HERE, std::move(trace_writer_));
  } else {
    // Intentionally leak; we have no way of safely destroying this at this
    // point.
    ANNOTATE_LEAKING_OBJECT_PTR(trace_writer_.get());
    trace_writer_.release();
  }
}

base::ModuleCache*
TracingSamplerProfiler::TracingProfileBuilder::GetModuleCache() {
  return &module_cache_;
}

using SampleDebugProto =
    perfetto::protos::pbzero::ChromeSamplingProfilerSampleCollected;

void TracingSamplerProfiler::TracingProfileBuilder::OnSampleCompleted(
    std::vector<base::Frame> frames,
    base::TimeTicks sample_timestamp) {
  base::AutoLock l(trace_writer_lock_);
  bool is_startup_tracing = (trace_writer_ == nullptr);

  if (is_startup_tracing) {
    if (buffered_samples_.size() < kMaxBufferedSamples) {
      buffered_samples_.emplace_back(
          BufferedSample(sample_timestamp, std::move(frames)));
    }
    return;
  }
  if (!buffered_samples_.empty()) {
    for (auto& sample : buffered_samples_) {
      WriteSampleToTrace(std::move(sample));
    }
    buffered_samples_.clear();
  }

  BufferedSample sample(sample_timestamp, std::move(frames));
  WriteSampleToTrace(std::move(sample));
  if (sample_callback_for_testing_) {
    sample_callback_for_testing_.Run();
  }
}

void TracingSamplerProfiler::TracingProfileBuilder::WriteSampleToTrace(
    TracingSamplerProfiler::TracingProfileBuilder::BufferedSample sample) {
  auto& frames = sample.sample;
  auto reset_id =
      TracingSamplerProfilerDataSource::GetIncrementalStateResetID();
  if (reset_id != last_incremental_state_reset_id_) {
    reset_incremental_state_ = true;
    last_incremental_state_reset_id_ = reset_id;
  }

  if (reset_incremental_state_) {
    stack_profile_writer_.ResetEmittedState();

    TracePacketHandle trace_packet = trace_writer_->NewTracePacket();
    trace_packet->set_sequence_flags(
        perfetto::protos::pbzero::TracePacket::SEQ_INCREMENTAL_STATE_CLEARED);

    // Note: Make sure ThreadDescriptors we emit here won't cause
    // metadata events to be emitted from the JSON exporter which conflict
    // with the metadata events emitted by the regular TrackEventDataSource.
    auto* thread_descriptor = trace_packet->set_thread_descriptor();
    thread_descriptor->set_pid(
        base::trace_event::TraceLog::GetInstance()->process_id());
    thread_descriptor->set_tid(sampled_thread_id_);
    last_timestamp_ = sample.timestamp;
    thread_descriptor->set_reference_timestamp_us(
        last_timestamp_.since_origin().InMicroseconds());

    TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
                        UnwinderTypeToString(unwinder_type_));
    reset_incremental_state_ = false;
  }

  TracePacketHandle trace_packet = trace_writer_->NewTracePacket();
  // Delta encoded timestamps and interned data require incremental state.
  trace_packet->set_sequence_flags(
      perfetto::protos::pbzero::TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
  auto callstack_id =
      stack_profile_writer_.GetCallstackIDAndMaybeEmit(frames, &trace_packet);
  auto* streaming_profile_packet = trace_packet->set_streaming_profile_packet();
  streaming_profile_packet->add_callstack_iid(callstack_id);

  int32_t current_process_priority = base::Process::Current().GetOSPriority();
  if (current_process_priority != 0) {
    streaming_profile_packet->set_process_priority(current_process_priority);
  }

  streaming_profile_packet->add_timestamp_delta_us(
      (sample.timestamp - last_timestamp_).InMicroseconds());
  last_timestamp_ = sample.timestamp;
}

void TracingSamplerProfiler::TracingProfileBuilder::SetTraceWriter(
    std::unique_ptr<perfetto::TraceWriterBase> writer) {
  base::AutoLock l(trace_writer_lock_);
  trace_writer_ = std::move(writer);
}

void TracingSamplerProfiler::TracingProfileBuilder::SetUnwinderType(
    const TracingSamplerProfiler::UnwinderType unwinder_type) {
  unwinder_type_ = unwinder_type;
}

TracingSamplerProfiler::StackProfileWriter::StackProfileWriter(
    bool enable_filtering)
    : should_enable_filtering_(enable_filtering) {}
TracingSamplerProfiler::StackProfileWriter::~StackProfileWriter() = default;

InterningID
TracingSamplerProfiler::StackProfileWriter::GetCallstackIDAndMaybeEmit(
    std::vector<base::Frame>& frames,
    perfetto::TraceWriter::TracePacketHandle* trace_packet) {
  size_t ip_hash = 0;
  for (const auto& frame : frames) {
    ip_hash = base::HashInts(ip_hash, frame.instruction_pointer);
  }

  InterningIndexEntry interned_callstack =
      interned_callstacks_.LookupOrAdd(ip_hash);

  if (interned_callstack.was_emitted)
    return interned_callstack.id;

  auto* interned_data = (*trace_packet)->set_interned_data();

  std::vector<InterningID> frame_ids;
  for (auto& frame : frames) {
    bool is_unwinder_provided_function_name = false;
    FrameDetails frame_details;
    if (frame.module) {
      frame_details.SetModule(*frame.module);
      frame_details.rel_pc =
          frame.instruction_pointer - frame_details.module_base_address;
    }

#if ANDROID_ARM64_UNWINDING_SUPPORTED || ANDROID_CFI_UNWINDING_SUPPORTED
    if (is_chrome_address(frame.instruction_pointer)) {
      frame_details.rel_pc =
          frame.instruction_pointer - executable_start_addr();
      if (!frame_details.has_valid_module()) {
        frame_details.SetChromeModuleInfo();
      }
    } else {
      if (!frame.function_name.empty()) {
        // Set function names for modules other than native chrome.
        // This includes Java frames and native Android system frames.
        // Chrome native frames can already be symbolized server side.
        // Currently only libunwindstack_unwinder fills function_names in
        // frames.
        is_unwinder_provided_function_name = true;
        frame_details.frame_name = std::move(frame.function_name);
      }
      if (frame.instruction_pointer == 0) {
        // TODO(ssid): This frame is currently skipped from inserting. Find a
        // way to specify that this frame is scanned in the trace.
        frame_details.frame_name = "Scanned";
      } else if (frame_details.module_id.empty() ||
                 !frame_details.has_valid_module()) {
        // For AOT modules the build id is empty. Set full pathname for these
        // modules, so that deobfuscation logic can work, since it depends on
        // getting full path name to extract package name.
        frame_details.SetSystemModuleInfo(frame.instruction_pointer);
      }
    }
#endif  // !(ANDROID_ARM64_UNWINDING_SUPPORTED ||
        // ANDROID_CFI_UNWINDING_SUPPORTED)

    // If we do not have a valid module and a valid frame, add a frame with
    // dummy details. Adding invalid frame would make trace processor invalidate
    // the whole sample.
    if (!frame_details.has_valid_module() || !frame_details.has_valid_frame()) {
      frame_details.FillWithDummyFields(frame.instruction_pointer);
    }

    if (!frame_details.module_id.empty()) {
      // TODO(b/270470700): Remove this on all platforms once tools/tracing is
      // fixed.
      frame_details.module_id =
          base::TransformModuleIDToSymbolServerFormat(frame_details.module_id);
    }

    // Allow uploading function names passed from unwinder, which would be
    // coming from static compile time strings.
    bool should_emit_frame_names =
        !frame_details.frame_name.empty() &&
        (is_unwinder_provided_function_name || !should_enable_filtering_);

    InterningIndexEntry interned_frame;
    if (should_emit_frame_names) {
      interned_frame = interned_frames_.LookupOrAdd(
          std::make_pair(frame_details.frame_name, frame_details.module_id));
    } else {
      interned_frame = interned_frames_.LookupOrAdd(
          std::make_pair(frame_details.rel_pc, frame_details.module_id));
    }

    if (!interned_frame.was_emitted) {
      InterningIndexEntry interned_frame_name;
      if (should_emit_frame_names) {
        interned_frame_name =
            interned_frame_names_.LookupOrAdd(frame_details.frame_name);
        if (!interned_frame_name.was_emitted) {
          auto* frame_name_entry = interned_data->add_function_names();
          frame_name_entry->set_iid(interned_frame_name.id);
          frame_name_entry->set_str(
              reinterpret_cast<const uint8_t*>(frame_details.frame_name.data()),
              frame_details.frame_name.length());
        }
      }

      InterningIndexEntry interned_module;
      if (frame_details.has_valid_module()) {
        interned_module =
            interned_modules_.LookupOrAdd(frame_details.module_base_address);
        if (!interned_module.was_emitted) {
          InterningIndexEntry interned_module_id =
              interned_module_ids_.LookupOrAdd(frame_details.module_id);
          if (!interned_module_id.was_emitted) {
            auto* module_id_entry = interned_data->add_build_ids();
            module_id_entry->set_iid(interned_module_id.id);
            module_id_entry->set_str(reinterpret_cast<const uint8_t*>(
                                         frame_details.module_id.data()),
                                     frame_details.module_id.length());
          }

          InterningIndexEntry interned_module_name =
              interned_module_names_.LookupOrAdd(frame_details.module_name);
          if (!interned_module_name.was_emitted) {
            auto* module_name_entry = interned_data->add_mapping_paths();
            module_name_entry->set_iid(interned_module_name.id);
            module_name_entry->set_str(reinterpret_cast<const uint8_t*>(
                                           frame_details.module_name.data()),
                                       frame_details.module_name.length());
          }
          auto* module_entry = interned_data->add_mappings();
          module_entry->set_iid(interned_module.id);
          module_entry->set_build_id(interned_module_id.id);
          module_entry->add_path_string_ids(interned_module_name.id);
        }
      }

      auto* frame_entry = interned_data->add_frames();
      frame_entry->set_iid(interned_frame.id);
      if (should_emit_frame_names) {
        frame_entry->set_function_name_id(interned_frame_name.id);
      } else {
        frame_entry->set_rel_pc(frame_details.rel_pc);
      }
      if (frame_details.has_valid_module()) {
        frame_entry->set_mapping_id(interned_module.id);
      }
    }

    frame_ids.push_back(interned_frame.id);
  }

  auto* callstack_entry = interned_data->add_callstacks();
  callstack_entry->set_iid(interned_callstack.id);
  // base::Unwinder starts from stack top and works to the bottom, but our
  // Callstack proto wants bottom first to stack top, so we iterate in reverse.
  // See b/241357440 for context.
  for (auto it = frame_ids.rbegin(); it != frame_ids.rend(); ++it) {
    callstack_entry->add_frame_ids(*it);
  }

  return interned_callstack.id;
}

void TracingSamplerProfiler::StackProfileWriter::ResetEmittedState() {
  interned_callstacks_.ResetEmittedState();
  interned_frames_.ResetEmittedState();
  interned_frame_names_.ResetEmittedState();
  interned_module_names_.ResetEmittedState();
  interned_module_ids_.ResetEmittedState();
  interned_modules_.ResetEmittedState();
}

// static
std::unique_ptr<TracingSamplerProfiler>
TracingSamplerProfiler::CreateOnMainThread(
    CoreUnwindersCallback core_unwinders_factory_function,
    UnwinderType unwinder_type) {
  auto profiler = std::make_unique<TracingSamplerProfiler>(
      base::GetSamplingProfilerCurrentThreadToken(),
      std::move(core_unwinders_factory_function), unwinder_type);
  // If running in single process mode, there may be multiple "main thread"
  // profilers created. In this case, we assume the first created one is the
  // browser one.
  if (!g_main_thread_instance) {
    g_main_thread_instance = profiler.get();

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
    // The loader lock is process-wide so should only be sampled on a single
    // thread. So only one TracingSamplerProfiler should create a
    // LoaderLockSamplingThread.
    profiler->loader_lock_sampling_thread_ =
        std::make_unique<LoaderLockSamplingThread>();
#endif
  }
  return profiler;
}

// static
void TracingSamplerProfiler::CreateOnChildThread() {
  CreateOnChildThreadWithCustomUnwinders(CoreUnwindersCallback());
}

// static
void TracingSamplerProfiler::CreateOnChildThreadWithCustomUnwinders(
    CoreUnwindersCallback core_unwinders_factory_function) {
  base::SequenceLocalStorageSlot<TracingSamplerProfiler>& slot =
      GetSequenceLocalStorageProfilerSlot();
  if (slot)
    return;

  slot.emplace(base::GetSamplingProfilerCurrentThreadToken(),
               std::move(core_unwinders_factory_function));
}

// static
void TracingSamplerProfiler::DeleteOnChildThreadForTesting() {
  GetSequenceLocalStorageProfilerSlot().reset();
}

// static
void TracingSamplerProfiler::ResetDataSourceForTesting() {
  TracingSamplerProfilerDataSource::Get()->ResetForTesting();
  RegisterDataSource();
}

// static
void TracingSamplerProfiler::RegisterDataSource() {
  TracingSamplerProfilerDataSource::Get()->RegisterDataSource();
  PerfettoTracedProcess::Get()->AddDataSource(
      TracingSamplerProfilerDataSource::Get());
}

// static
bool TracingSamplerProfiler::IsStackUnwindingSupportedForTesting() {
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN) && defined(_WIN64) ||          \
    ANDROID_ARM64_UNWINDING_SUPPORTED || ANDROID_CFI_UNWINDING_SUPPORTED || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  return IsStackSamplingSupported();
#else
  return false;
#endif
}

void TracingSamplerProfiler::SetAuxUnwinderFactoryOnMainThread(
    const base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>& factory) {
  DCHECK(g_main_thread_instance);
  g_main_thread_instance->SetAuxUnwinderFactory(factory);
}

// TODO(b/336718643): Remove unused code after removing use_perfetto_client_library build
// flag.
// static
void TracingSamplerProfiler::StartTracingForTesting(
    PerfettoProducer* producer) {
  TracingSamplerProfilerDataSource::Get()->StartTracing(
      1, producer, perfetto::DataSourceConfig());
}

// static
void TracingSamplerProfiler::SetupStartupTracingForTesting() {
  base::trace_event::TraceConfig config(
      TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
      base::trace_event::TraceRecordMode::RECORD_UNTIL_FULL);
  TracingSamplerProfilerDataSource::Get()->SetupStartupTracing(
      /*producer=*/nullptr, config, /*privacy_filtering_enabled=*/false);
}

// static
void TracingSamplerProfiler::StopTracingForTesting() {
  TracingSamplerProfilerDataSource::Get()->StopTracing(base::DoNothing());
}

TracingSamplerProfiler::TracingSamplerProfiler(
    base::SamplingProfilerThreadToken sampled_thread_token,
    CoreUnwindersCallback core_unwinders_factory_function,
    UnwinderType unwinder_type)
    : sampled_thread_token_(sampled_thread_token),
      core_unwinders_factory_function_(
          std::move(core_unwinders_factory_function)),
      unwinder_type_(unwinder_type) {
  DCHECK_NE(sampled_thread_token_.id, base::kInvalidThreadId);
#if INITIALIZE_THREAD_DELEGATE_POSIX
  // Since StackSamplingProfiler is scoped to a tracing session and lives on the
  // thread where `StartTracing` is called, we use `ThreadDelegatePosix` to
  // initialize global data, like the thread stack base address, that has to be
  // created on the profiled thread. See crbug.com/1392158#c26 for details.
  base::ThreadDelegatePosix::Create(sampled_thread_token_);
#endif  // INITIALIZE_THREAD_DELEGATE_POSIX
  TracingSamplerProfilerDataSource::Get()->RegisterProfiler(this);
}

TracingSamplerProfiler::~TracingSamplerProfiler() {
  TracingSamplerProfilerDataSource::Get()->UnregisterProfiler(this);
  if (g_main_thread_instance == this)
    g_main_thread_instance = nullptr;
}

void TracingSamplerProfiler::SetAuxUnwinderFactory(
    const base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>& factory) {
  base::AutoLock lock(lock_);
  aux_unwinder_factory_ = factory;
  if (profiler_)
    profiler_->AddAuxUnwinder(aux_unwinder_factory_.Run());
}

void TracingSamplerProfiler::SetSampleCallbackForTesting(
    const base::RepeatingClosure& sample_callback_for_testing) {
  base::AutoLock lock(lock_);
  sample_callback_for_testing_ = sample_callback_for_testing;
}

void TracingSamplerProfiler::StartTracing(
    std::unique_ptr<perfetto::TraceWriterBase> trace_writer,
    bool should_enable_filtering) {
  base::AutoLock lock(lock_);
  if (profiler_) {
    if (trace_writer) {
      profile_builder_->SetTraceWriter(std::move(trace_writer));
    }
    return;
  }

  if (!base::StackSamplingProfiler::IsSupportedForCurrentPlatform()) {
    return;
  }

  base::StackSamplingProfiler::SamplingParams params;
  params.samples_per_profile = std::numeric_limits<int>::max();
  params.sampling_interval = base::Milliseconds(50);

  auto profile_builder = std::make_unique<TracingProfileBuilder>(
      sampled_thread_token_.id,
      std::move(trace_writer),
      should_enable_filtering, sample_callback_for_testing_);

  profile_builder_ = profile_builder.get();
  // There is a dichotomy between stack samplers for Android and other
  // platforms. While Android explicitly needs a factory to provide "core"
  // unwinders, other platforms explicitly check that no such factory is
  // provided.
#if BUILDFLAG(IS_ANDROID)
  base::StackSamplingProfiler::UnwindersFactory core_unwinders_factory;
  if (core_unwinders_factory_function_) {
    core_unwinders_factory = core_unwinders_factory_function_.Run();
  }
  if (core_unwinders_factory) {
    if (unwinder_type_ == UnwinderType::kUnknown) {
      unwinder_type_ = UnwinderType::kCustomAndroid;
    }
    profile_builder->SetUnwinderType(unwinder_type_);
    profiler_ = std::make_unique<base::StackSamplingProfiler>(
        sampled_thread_token_, params, std::move(profile_builder),
        std::move(core_unwinders_factory));
  }
#else   // BUILDFLAG(IS_ANDROID)
  if (unwinder_type_ == UnwinderType::kUnknown) {
    unwinder_type_ = UnwinderType::kDefault;
  }
  profile_builder->SetUnwinderType(unwinder_type_);
  profiler_ = std::make_unique<base::StackSamplingProfiler>(
      sampled_thread_token_, params, std::move(profile_builder));
#endif  // BUILDFLAG(IS_ANDROID)
  if (profiler_ != nullptr) {
    if (aux_unwinder_factory_) {
      profiler_->AddAuxUnwinder(aux_unwinder_factory_.Run());
    }
    profiler_->Start();
  }

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
  if (loader_lock_sampling_thread_)
    loader_lock_sampling_thread_->StartSampling();
#endif
}

void TracingSamplerProfiler::StopTracing() {
  base::AutoLock lock(lock_);
  if (!profiler_) {
    return;
  }

  // Stop and release the stack sampling profiler.
  profiler_->Stop();
  profile_builder_ = nullptr;
  profiler_.reset();

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
  if (loader_lock_sampling_thread_)
    loader_lock_sampling_thread_->StopSampling();
#endif
}

}  // namespace tracing

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS(
    COMPONENT_EXPORT(TRACING_CPP),
    tracing::TracingSamplerProfilerDataSource::DataSourceProxy);

// This should go after PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS
// to avoid instantiation of type() template method before specialization.
std::unique_ptr<perfetto::TraceWriterBase>
tracing::TracingSamplerProfilerDataSource::CreateTraceWriter() {
  perfetto::internal::DataSourceStaticState* static_state =
      perfetto::DataSourceHelper<DataSourceProxy>::type().static_state();
  // DataSourceProxy disallows multiple instances, so our instance will always
  // have index 0.
  perfetto::internal::DataSourceState* instance_state = static_state->TryGet(0);
  CHECK(instance_state);
  return perfetto::internal::TracingMuxer::Get()->CreateTraceWriter(
      static_state, data_source_config_.target_buffer(), instance_state,
      perfetto::BufferExhaustedPolicy::kDrop);
}
