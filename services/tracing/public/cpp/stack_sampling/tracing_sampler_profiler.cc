// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"

#include <limits>
#include <set>

#include "base/android/library_loader/anchor_functions.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/leak_annotations.h"
#include "base/debug/stack_trace.h"
#include "base/hash/hash.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/trace_event/trace_event.h"
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

#if ANDROID_ARM64_UNWINDING_SUPPORTED || ANDROID_CFI_UNWINDING_SUPPORTED
#include <dlfcn.h>

#include "base/android/reached_code_profiler.h"
#include "base/debug/elf_reader.h"

#if ANDROID_ARM64_UNWINDING_SUPPORTED
#include "services/tracing/public/cpp/stack_sampling/stack_unwinder_arm64_android.h"

#elif ANDROID_CFI_UNWINDING_SUPPORTED
#include "base/trace_event/cfi_backtrace_android.h"
#include "services/tracing/public/cpp/stack_sampling/stack_sampler_android.h"

#endif  // ANDROID_ARM64_UNWINDING_SUPPORTED

#endif  // ANDROID_ARM64_UNWINDING_SUPPORTED || ANDROID_CFI_UNWINDING_SUPPORTED

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
#include "services/tracing/public/cpp/stack_sampling/loader_lock_sampler_win.h"
#endif

using StreamingProfilePacketHandle =
    protozero::MessageHandle<perfetto::protos::pbzero::StreamingProfilePacket>;

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

class TracingSamplerProfilerDataSource
    : public PerfettoTracedProcess::DataSourceBase {
 public:
  static TracingSamplerProfilerDataSource* Get() {
    static base::NoDestructor<TracingSamplerProfilerDataSource> instance;
    return instance.get();
  }

  TracingSamplerProfilerDataSource()
      : DataSourceBase(mojom::kSamplerProfilerSourceName) {}

  ~TracingSamplerProfilerDataSource() override { NOTREACHED(); }

  void RegisterProfiler(TracingSamplerProfiler* profiler) {
    base::AutoLock lock(lock_);
    if (!profilers_.insert(profiler).second) {
      return;
    }

    if (is_started_) {
      profiler->StartTracing(
          producer_->CreateTraceWriter(data_source_config_.target_buffer()),
          data_source_config_.chrome_config().privacy_filtering_enabled());
    } else if (is_startup_tracing_) {
      profiler->StartTracing(nullptr, /*should_enable_filtering=*/true);
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
  void StartTracing(
      PerfettoProducer* producer,
      const perfetto::DataSourceConfig& data_source_config) override {
    base::AutoLock lock(lock_);
    DCHECK(!is_started_);
    is_started_ = true;
    is_startup_tracing_ = false;
    data_source_config_ = data_source_config;

    bool should_enable_filtering =
        data_source_config.chrome_config().privacy_filtering_enabled();

    for (auto* profiler : profilers_) {
      profiler->StartTracing(
          producer->CreateTraceWriter(data_source_config.target_buffer()),
          should_enable_filtering);
    }
  }

  void StopTracing(base::OnceClosure stop_complete_callback) override {
    base::AutoLock lock(lock_);
    DCHECK(is_started_);
    is_started_ = false;
    is_startup_tracing_ = false;
    producer_ = nullptr;

    for (auto* profiler : profilers_) {
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
    for (auto* profiler : profilers_) {
      // Enable filtering for startup tracing always to be safe.
      profiler->StartTracing(nullptr, /*should_enable_filtering=*/true);
    }
  }

  void AbortStartupTracing() override {
    base::AutoLock lock(lock_);
    if (!is_startup_tracing_) {
      return;
    }
    for (auto* profiler : profilers_) {
      // Enable filtering for startup tracing always to be safe.
      profiler->StartTracing(nullptr, /*should_enable_filtering=*/true);
    }
    is_startup_tracing_ = false;
  }

  void ClearIncrementalState() override {
    incremental_state_reset_id_.fetch_add(1u, std::memory_order_relaxed);
  }

  static uint32_t GetIncrementalStateResetID() {
    return incremental_state_reset_id_.load(std::memory_order_relaxed);
  }

 private:
  base::Lock lock_;  // Protects subsequent members.
  std::set<TracingSamplerProfiler*> profilers_;
  bool is_startup_tracing_ = false;
  bool is_started_ = false;
  perfetto::DataSourceConfig data_source_config_;

  static std::atomic<uint32_t> incremental_state_reset_id_;
};

// static
std::atomic<uint32_t>
    TracingSamplerProfilerDataSource::incremental_state_reset_id_{0};

base::SequenceLocalStorageSlot<TracingSamplerProfiler>&
GetSequenceLocalStorageProfilerSlot() {
  static base::NoDestructor<
      base::SequenceLocalStorageSlot<TracingSamplerProfiler>>
      storage;
  return *storage;
}

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
TracingSamplerProfiler::LoaderLockSampler* g_test_loader_lock_sampler = nullptr;
#endif

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
    return !module_name.empty() && !module_id.empty() &&
           module_base_address > 0;
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
    if (module_id.empty()) {
      module_id = "missing";
    }
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
    static const base::Optional<base::StringPiece> library_name =
        base::debug::ReadElfLibraryName(
            reinterpret_cast<void*>(executable_start_addr()));
    static const base::NoDestructor<std::string> chrome_debug_id([] {
      base::debug::ElfBuildIdBuffer build_id;
      size_t build_id_length = base::debug::ReadElfBuildId(
          reinterpret_cast<void*>(executable_start_addr()), true, build_id);
      return std::string(build_id, build_id_length);
    }());
    if (library_name) {
      module_name = library_name->as_string();
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
    // We have already symbolized these frames, so module ID is not necessary.
    // Reading the real ID can cause crashes and we can't symbolize these
    // server-side anyways.
    // TODO(ssid): Remove this once perfetto can keep the frames without module
    // ID.
    module_id = "system";

    DCHECK(has_valid_frame());
    DCHECK(has_valid_module());
  }
#endif
};

}  // namespace

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
const char TracingSamplerProfiler::kLoaderLockHeldEventName[] =
    "LoaderLockHeld (sampled)";
#endif

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
    std::unique_ptr<perfetto::TraceWriter> trace_writer,
    bool should_enable_filtering,
    const base::RepeatingClosure& sample_callback_for_testing)
    : sampled_thread_id_(sampled_thread_id),
      trace_writer_(std::move(trace_writer)),
      should_enable_filtering_(should_enable_filtering),
      sample_callback_for_testing_(sample_callback_for_testing) {}

TracingSamplerProfiler::TracingProfileBuilder::~TracingProfileBuilder() {
  // Deleting a TraceWriter can end up triggering a Mojo call which calls
  // TaskRunnerHandle::Get() and isn't safe on thread shutdown, which is when
  // TracingProfileBuilder gets destructed, so we make sure this happens on
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

void TracingSamplerProfiler::TracingProfileBuilder::OnSampleCompleted(
    std::vector<base::Frame> frames,
    base::TimeTicks sample_timestamp) {
#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
  SampleLoaderLock();
#endif

  base::AutoLock l(trace_writer_lock_);
  if (!trace_writer_) {
    if (buffered_samples_.size() < kMaxBufferedSamples) {
      buffered_samples_.emplace_back(
          BufferedSample(sample_timestamp, std::move(frames)));
    }
    return;
  }
  if (!buffered_samples_.empty()) {
    for (const auto& sample : buffered_samples_) {
      WriteSampleToTrace(sample);
    }
    buffered_samples_.clear();
  }
  WriteSampleToTrace(BufferedSample(sample_timestamp, std::move(frames)));

  if (sample_callback_for_testing_) {
    sample_callback_for_testing_.Run();
  }
}

void TracingSamplerProfiler::TracingProfileBuilder::WriteSampleToTrace(
    const TracingSamplerProfiler::TracingProfileBuilder::BufferedSample&
        sample) {
  const auto& frames = sample.sample;
  auto reset_id =
      TracingSamplerProfilerDataSource::GetIncrementalStateResetID();
  if (reset_id != last_incremental_state_reset_id_) {
    reset_incremental_state_ = true;
    last_incremental_state_reset_id_ = reset_id;
  }

  if (reset_incremental_state_) {
    interned_callstacks_.ResetEmittedState();
    interned_frames_.ResetEmittedState();
    interned_frame_names_.ResetEmittedState();
    interned_module_names_.ResetEmittedState();
    interned_module_ids_.ResetEmittedState();
    interned_modules_.ResetEmittedState();

    auto trace_packet = trace_writer_->NewTracePacket();
    trace_packet->set_sequence_flags(
        perfetto::protos::pbzero::TracePacket::SEQ_INCREMENTAL_STATE_CLEARED);

    // Note: Make sure ThreadDescriptors we emit here won't cause
    // metadata events to be emitted from the JSON exporter which conflict
    // with the metadata events emitted by the regular TrackEventDataSource.
    auto* thread_descriptor = trace_packet->set_thread_descriptor();
    thread_descriptor->set_pid(base::GetCurrentProcId());
    thread_descriptor->set_tid(sampled_thread_id_);
    last_timestamp_ = sample.timestamp;
    thread_descriptor->set_reference_timestamp_us(
        last_timestamp_.since_origin().InMicroseconds());
    reset_incremental_state_ = false;
  }


  auto trace_packet = trace_writer_->NewTracePacket();
  // Delta encoded timestamps and interned data require incremental state.
  trace_packet->set_sequence_flags(
      perfetto::protos::pbzero::TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
  auto callstack_id = GetCallstackIDAndMaybeEmit(frames, &trace_packet);
  auto* streaming_profile_packet = trace_packet->set_streaming_profile_packet();
  streaming_profile_packet->add_callstack_iid(callstack_id);

  int32_t current_process_priority = base::Process::Current().GetPriority();
  if (current_process_priority != 0) {
    streaming_profile_packet->set_process_priority(current_process_priority);
  }

  streaming_profile_packet->add_timestamp_delta_us(
      (sample.timestamp - last_timestamp_).InMicroseconds());
  last_timestamp_ = sample.timestamp;
}

void TracingSamplerProfiler::TracingProfileBuilder::SetTraceWriter(
    std::unique_ptr<perfetto::TraceWriter> writer) {
  base::AutoLock l(trace_writer_lock_);
  trace_writer_ = std::move(writer);
}

InterningID
TracingSamplerProfiler::TracingProfileBuilder::GetCallstackIDAndMaybeEmit(
    const std::vector<base::Frame>& frames,
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
  for (const auto& frame : frames) {
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
    } else if (frame.instruction_pointer == 0) {
      // TODO(ssid): This frame is currently skipped from inserting. Find a way
      // to specify that this frame is scanned in the trace.
      frame_details.frame_name = "Scanned";
    } else if (!frame_details.has_valid_module()) {
      frame_details.SetSystemModuleInfo(frame.instruction_pointer);
    }
#endif  // !(ANDROID_ARM64_UNWINDING_SUPPORTED ||
        // ANDROID_CFI_UNWINDING_SUPPORTED)

    // If we do not have a valid module and a valid frame, add a frame with
    // dummy details. Adding invalid frame would make trace processor invalidate
    // the whole sample.
    if (!frame_details.has_valid_module() || !frame_details.has_valid_frame()) {
      frame_details.FillWithDummyFields(frame.instruction_pointer);
    }

    MangleModuleIDIfNeeded(&frame_details.module_id);

    // We never emit frame names in privacy filtered mode.
    bool should_emit_frame_names =
        !frame_details.frame_name.empty() && !should_enable_filtering_;

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
  for (auto& frame_id : frame_ids)
    callstack_entry->add_frame_ids(frame_id);

  return interned_callstack.id;
}

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
void TracingSamplerProfiler::TracingProfileBuilder::SampleLoaderLock() {
  if (!should_sample_loader_lock_)
    return;

  bool loader_lock_now_held =
      g_test_loader_lock_sampler
          ? g_test_loader_lock_sampler->IsLoaderLockHeld()
          : IsLoaderLockHeld();

  // TODO(crbug.com/1065077): It would be cleaner to save the loader lock state
  // alongside buffered_samples_ and then add it to the ProcessDescriptor
  // packet in
  // TracingSamplerProfiler::TracingProfileBuilder::WriteSampleToTrace. But
  // ProcessDescriptor is currently not being collected correctly. See the full
  // discussion in the linked crbug.
  if (loader_lock_now_held && !loader_lock_is_held_) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
        TracingSamplerProfiler::kLoaderLockHeldEventName, TRACE_ID_LOCAL(this));
  } else if (!loader_lock_now_held && loader_lock_is_held_) {
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
        TracingSamplerProfiler::kLoaderLockHeldEventName, TRACE_ID_LOCAL(this));
  }
  loader_lock_is_held_ = loader_lock_now_held;
}
#endif

// static
void TracingSamplerProfiler::MangleModuleIDIfNeeded(std::string* module_id) {
#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Linux ELF module IDs are 160bit integers, which we need to mangle
  // down to 128bit integers to match the id that Breakpad outputs.
  // Example on version '66.0.3359.170' x64:
  //   Build-ID: "7f0715c2 86f8 b16c 10e4ad349cda3b9b 56c7a773
  //   Debug-ID  "C215077F F886 6CB1 10E4AD349CDA3B9B 0"
  if (module_id->size() >= 32) {
    *module_id =
        base::StrCat({module_id->substr(6, 2), module_id->substr(4, 2),
                      module_id->substr(2, 2), module_id->substr(0, 2),
                      module_id->substr(10, 2), module_id->substr(8, 2),
                      module_id->substr(14, 2), module_id->substr(12, 2),
                      module_id->substr(16, 16), "0"});
  }
#endif
}

// static
std::unique_ptr<TracingSamplerProfiler>
TracingSamplerProfiler::CreateOnMainThread() {
  auto profiler = std::make_unique<TracingSamplerProfiler>(
      base::GetSamplingProfilerCurrentThreadToken());
#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
  // The loader lock is process-wide so should only be sampled on a single
  // thread. The main thread is convenient.
  InitializeLoaderLockSampling();
  profiler->EnableLoaderLockSampling();
#endif
  // If running in single process mode, there may be multiple "main thread"
  // profilers created. In this case, we assume the first created one is the
  // browser one.
  if (!g_main_thread_instance)
    g_main_thread_instance = profiler.get();
  return profiler;
}

// static
void TracingSamplerProfiler::CreateOnChildThread() {
  base::SequenceLocalStorageSlot<TracingSamplerProfiler>& slot =
      GetSequenceLocalStorageProfilerSlot();
  if (slot)
    return;

  slot.emplace(base::GetSamplingProfilerCurrentThreadToken());
}

// static
void TracingSamplerProfiler::DeleteOnChildThreadForTesting() {
  GetSequenceLocalStorageProfilerSlot().reset();
}

// static
void TracingSamplerProfiler::RegisterDataSource() {
  PerfettoTracedProcess::Get()->AddDataSource(
      TracingSamplerProfilerDataSource::Get());
}

void TracingSamplerProfiler::SetAuxUnwinderFactoryOnMainThread(
    const base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>& factory) {
  DCHECK(g_main_thread_instance);
  g_main_thread_instance->SetAuxUnwinderFactory(factory);
}

// static
void TracingSamplerProfiler::StartTracingForTesting(
    PerfettoProducer* producer) {
  TracingSamplerProfilerDataSource::Get()->StartTracingWithID(
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

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
// static
void TracingSamplerProfiler::SetLoaderLockSamplerForTesting(
    LoaderLockSampler* sampler) {
  g_test_loader_lock_sampler = sampler;
}
#endif

TracingSamplerProfiler::TracingSamplerProfiler(
    base::SamplingProfilerThreadToken sampled_thread_token)
    : sampled_thread_token_(sampled_thread_token) {
  DCHECK_NE(sampled_thread_token_.id, base::kInvalidThreadId);
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
    std::unique_ptr<perfetto::TraceWriter> trace_writer,
    bool should_enable_filtering) {
  base::AutoLock lock(lock_);
  if (profiler_) {
    if (trace_writer) {
      profile_builder_->SetTraceWriter(std::move(trace_writer));
    }
    return;
  }

#if ANDROID_ARM64_UNWINDING_SUPPORTED || ANDROID_CFI_UNWINDING_SUPPORTED
  // The sampler profiler would conflict with the reached code profiler if they
  // run at the same time because they use the same signal to suspend threads.
  if (base::android::IsReachedCodeProfilerEnabled())
    return;
#else   // ANDROID_ARM64_UNWINDING_SUPPORTED || ANDROID_CFI_UNWINDING_SUPPORTED

  // On Android the sampling profiler is implemented by tracing service and is
  // not yet supported by base::StackSamplingProfiler. So, only check this if
  // service does not support unwinding in current platform.
  if (!base::StackSamplingProfiler::IsSupportedForCurrentPlatform())
    return;
#endif  // !(ANDROID_ARM64_UNWINDING_SUPPORTED ||
        // ANDROID_CFI_UNWINDING_SUPPORTED)

  base::StackSamplingProfiler::SamplingParams params;
  params.samples_per_profile = std::numeric_limits<int>::max();
  params.sampling_interval = base::TimeDelta::FromMilliseconds(50);

  auto profile_builder = std::make_unique<TracingProfileBuilder>(
      sampled_thread_token_.id, std::move(trace_writer),
      should_enable_filtering, sample_callback_for_testing_);
#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
  if (should_sample_loader_lock_)
    profile_builder->EnableLoaderLockSampling();
#endif

  profile_builder_ = profile_builder.get();
  // Create and start the stack sampling profiler.
#if defined(OS_ANDROID)
#if ANDROID_ARM64_UNWINDING_SUPPORTED
  const auto create_unwinders = []() {
    std::vector<std::unique_ptr<base::Unwinder>> unwinders;
    unwinders.push_back(std::make_unique<UnwinderArm64>());
    return unwinders;
  };
  profiler_ = std::make_unique<base::StackSamplingProfiler>(
      sampled_thread_token_, params, std::move(profile_builder),
      base::BindOnce(create_unwinders));
  profiler_->Start();

#elif ANDROID_CFI_UNWINDING_SUPPORTED
  auto* module_cache = profile_builder->GetModuleCache();
  profiler_ = std::make_unique<base::StackSamplingProfiler>(
      params, std::move(profile_builder),
      std::make_unique<StackSamplerAndroid>(sampled_thread_token_,
                                            module_cache));
  profiler_->Start();
#endif
#else   // defined(OS_ANDROID)
  profiler_ = std::make_unique<base::StackSamplingProfiler>(
      sampled_thread_token_, params, std::move(profile_builder));
  if (aux_unwinder_factory_)
    profiler_->AddAuxUnwinder(aux_unwinder_factory_.Run());
  profiler_->Start();
#endif  // defined(OS_ANDROID)
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
}

}  // namespace tracing
