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
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/buildflags.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/profile_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_thread_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.pbzero.h"

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
#include "base/profiler/thread_delegate_posix.h"
#define INITIALIZE_THREAD_DELEGATE_POSIX 1
#else  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
#define INITIALIZE_THREAD_DELEGATE_POSIX 0
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)

#if !BUILDFLAG(IS_ANDROID)
#include "base/profiler/core_unwinders.h"
#endif

#if ANDROID_ARM64_UNWINDING_SUPPORTED || ANDROID_CFI_UNWINDING_SUPPORTED
#include <dlfcn.h>

#include "base/debug/elf_reader.h"
#endif  // ANDROID_ARM64_UNWINDING_SUPPORTED || ANDROID_CFI_UNWINDING_SUPPORTED

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
#include "services/tracing/public/cpp/stack_sampling/loader_lock_sampling_thread_win.h"
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

// A random UUID for the track used to emit streaming profile packets, to avoid
// collisions with other tracks.
constexpr uint64_t kStreamingProfileTrackUuid = 0x6A7E8A54C18B7778ul;

class TracingSamplerProfilerManager {
 public:
  static TracingSamplerProfilerManager* Get() {
    static base::NoDestructor<TracingSamplerProfilerManager> instance;
    return instance.get();
  }

  void OnDataSourceStart(TracingSamplerProfiler::DataSource* data_source) {
    base::AutoLock lock(lock_);
    DCHECK_EQ(data_source_, nullptr);
    data_source_ = data_source;
    for (TracingSamplerProfiler* profiler : profilers_) {
      profiler->StartTracing(data_source_->CreateTraceWriter(),
                             data_source_->privacy_filtering_enabled());
    }
  }
  void OnDataSourceStop(TracingSamplerProfiler::DataSource* data_source) {
    base::AutoLock lock(lock_);
    DCHECK_EQ(data_source_, data_source);
    data_source_ = nullptr;
    for (TracingSamplerProfiler* profiler : profilers_) {
      profiler->StopTracing();
    }
  }

  void WillClearIncrementalState() {
    incremental_state_reset_id_.fetch_add(1u, std::memory_order_relaxed);
  }

  uint32_t GetIncrementalStateResetID() {
    return incremental_state_reset_id_.load(std::memory_order_relaxed);
  }

  void RegisterProfiler(TracingSamplerProfiler* profiler) {
    base::AutoLock lock(lock_);
    if (!profilers_.insert(profiler).second) {
      return;
    }

    if (data_source_) {
      profiler->StartTracing(data_source_->CreateTraceWriter(),
                             data_source_->privacy_filtering_enabled());
    }
  }

  void UnregisterProfiler(TracingSamplerProfiler* profiler) {
    base::AutoLock lock(lock_);
    if (!profilers_.erase(profiler)) {
      return;
    }

    if (data_source_) {
      profiler->StopTracing();
    }
  }

 private:
  friend class base::NoDestructor<TracingSamplerProfilerManager>;

  TracingSamplerProfilerManager() = default;
  ~TracingSamplerProfilerManager() = default;

  base::Lock lock_;  // Protects subsequent members.
  std::set<raw_ptr<TracingSamplerProfiler, SetExperimental>> profilers_
      GUARDED_BY(lock_);
  raw_ptr<TracingSamplerProfiler::DataSource> data_source_ GUARDED_BY(lock_);
  std::atomic<uint32_t> incremental_state_reset_id_{1};
};

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
    (BUILDFLAG(IS_CHROMEOS) &&                                              \
     (defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64))) ||              \
    BUILDFLAG(IS_LINUX)
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

TracingSamplerProfiler::DataSource::DataSource() = default;

TracingSamplerProfiler::DataSource::~DataSource() = default;

void TracingSamplerProfiler::DataSource::OnSetup(const SetupArgs& args) {
  privacy_filtering_enabled_ =
      args.config->chrome_config().privacy_filtering_enabled();
}

void TracingSamplerProfiler::DataSource::OnStart(const StartArgs&) {
  TracingSamplerProfilerManager::Get()->OnDataSourceStart(this);
}

void TracingSamplerProfiler::DataSource::OnStop(const StopArgs&) {
  TracingSamplerProfilerManager::Get()->OnDataSourceStop(this);
}

void TracingSamplerProfiler::DataSource::WillClearIncrementalState(
    const ClearIncrementalStateArgs& args) {
  TracingSamplerProfilerManager::Get()->WillClearIncrementalState();
}

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
    base::ThreadPool::CreateSequencedTaskRunner({})->DeleteSoon(
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
  WriteSampleToTrace(std::move(frames), sample_timestamp);
  if (sample_callback_for_testing_) {
    sample_callback_for_testing_.Run();
  }
}

void TracingSamplerProfiler::TracingProfileBuilder::WriteSampleToTrace(
    std::vector<base::Frame> frames,
    base::TimeTicks sample_timestamp) {
  uint32_t previous_incremental_state_reset_id = std::exchange(
      last_incremental_state_reset_id_,
      TracingSamplerProfilerManager::Get()->GetIncrementalStateResetID());
  bool reset_incremental_state =
      (previous_incremental_state_reset_id != last_incremental_state_reset_id_);

  if (reset_incremental_state) {
    stack_profile_writer_.ResetEmittedState();

    TracePacketHandle trace_packet = trace_writer_->NewTracePacket();
    trace_packet->set_sequence_flags(
        perfetto::protos::pbzero::TracePacket::SEQ_INCREMENTAL_STATE_CLEARED);

    // Note: Make sure ThreadDescriptors we emit here won't cause
    // metadata events to be emitted from the JSON exporter which conflict
    // with the metadata events emitted by the regular TrackEventDataSource.
    auto thread_track =
        perfetto::ThreadTrack::ForThread(sampled_thread_id_.raw());
    perfetto::Track profile_track(kStreamingProfileTrackUuid, thread_track);
    auto* track_descriptor = trace_packet->set_track_descriptor();
    track_descriptor->set_uuid(profile_track.uuid);
    auto* thread_descriptor = track_descriptor->set_thread();
    thread_descriptor->set_pid(thread_track.pid);
    thread_descriptor->set_tid(thread_track.tid);

    last_timestamp_ = sample_timestamp;
    thread_descriptor->set_reference_timestamp_us(
        last_timestamp_.since_origin().InMicroseconds());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_AIX)
    if (base::GetCurrentProcId() !=
        base::trace_event::TraceLog::GetInstance()->process_id()) {
      auto* chrome_thread = track_descriptor->set_chrome_thread();
      chrome_thread->set_is_sandboxed_tid(true);
    }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_AIX)

    TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
                        UnwinderTypeToString(unwinder_type_));
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
      (sample_timestamp - last_timestamp_).InMicroseconds());
  last_timestamp_ = sample_timestamp;
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
    TracePacketHandle* trace_packet) {
  size_t ip_hash = 0;
  for (const auto& frame : frames) {
    ip_hash = base::HashInts(ip_hash, frame.instruction_pointer);
  }

  InterningIndexEntry interned_callstack =
      interned_callstacks_.LookupOrAdd(ip_hash);

  if (interned_callstack.was_emitted) {
    return interned_callstack.id;
  }

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
  if (slot) {
    return;
  }

  slot.emplace(base::GetSamplingProfilerCurrentThreadToken(),
               std::move(core_unwinders_factory_function));
}

// static
void TracingSamplerProfiler::DeleteOnChildThreadForTesting() {
  GetSequenceLocalStorageProfilerSlot().reset();
}

// static
void TracingSamplerProfiler::RegisterDataSource() {
  perfetto::DataSourceDescriptor dsd;
  dsd.set_name(mojom::kSamplerProfilerSourceName);
  perfetto::DataSource<TracingSamplerProfiler::DataSource>::Register(dsd);
}

// static
bool TracingSamplerProfiler::IsStackUnwindingSupportedForTesting() {
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN) && defined(_WIN64) ||          \
    ANDROID_ARM64_UNWINDING_SUPPORTED || ANDROID_CFI_UNWINDING_SUPPORTED || \
    (BUILDFLAG(IS_CHROMEOS) &&                                              \
     (defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64))) ||              \
    BUILDFLAG(IS_LINUX)
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

void TracingSamplerProfiler::SetSampleCallbackForTesting(
    const base::RepeatingClosure& sample_callback_for_testing) {
  base::AutoLock lock(lock_);
  sample_callback_for_testing_ = sample_callback_for_testing;
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
  TracingSamplerProfilerManager::Get()->RegisterProfiler(this);
}

TracingSamplerProfiler::~TracingSamplerProfiler() {
  TracingSamplerProfilerManager::Get()->UnregisterProfiler(this);
  if (g_main_thread_instance == this) {
    g_main_thread_instance = nullptr;
  }
}

void TracingSamplerProfiler::SetAuxUnwinderFactory(
    const base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>& factory) {
  base::AutoLock lock(lock_);
  aux_unwinder_factory_ = factory;
  if (profiler_) {
    profiler_->AddAuxUnwinder(aux_unwinder_factory_.Run());
  }
}

void TracingSamplerProfiler::StartTracing(
    std::unique_ptr<perfetto::TraceWriterBase> trace_writer,
    bool should_enable_filtering) {
  base::AutoLock lock(lock_);
  DCHECK_EQ(profiler_, nullptr);

  if (!base::StackSamplingProfiler::IsSupportedForCurrentPlatform()) {
    return;
  }

  base::StackSamplingProfiler::SamplingParams params;
  params.samples_per_profile = std::numeric_limits<int>::max();
  params.sampling_interval = base::Milliseconds(50);

  auto profile_builder = std::make_unique<TracingProfileBuilder>(
      sampled_thread_token_.id, std::move(trace_writer),
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
      sampled_thread_token_, params, std::move(profile_builder),
      base::CreateCoreUnwindersFactory());
#endif  // BUILDFLAG(IS_ANDROID)
  if (profiler_ != nullptr) {
    if (aux_unwinder_factory_) {
      profiler_->AddAuxUnwinder(aux_unwinder_factory_.Run());
    }
    profiler_->Start();
  }

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
  if (loader_lock_sampling_thread_) {
    loader_lock_sampling_thread_->StartSampling();
  }
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
  if (loader_lock_sampling_thread_) {
    loader_lock_sampling_thread_->StopSampling();
  }
#endif
}

}  // namespace tracing

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS(
    COMPONENT_EXPORT(TRACING_CPP),
    tracing::TracingSamplerProfiler::DataSource);

// This should go after PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS
// to avoid instantiation of type() template method before specialization.
std::unique_ptr<perfetto::TraceWriterBase>
tracing::TracingSamplerProfiler::DataSource::CreateTraceWriter() {
  perfetto::internal::DataSourceStaticState* static_state =
      perfetto::DataSourceHelper<TracingSamplerProfiler::DataSource>::type()
          .static_state();
  // DataSourceProxy disallows multiple instances, so our instance will always
  // have index 0.
  perfetto::internal::DataSourceState* instance_state = static_state->TryGet(0);
  CHECK(instance_state);
  return perfetto::internal::TracingMuxer::Get()->CreateTraceWriter(
      static_state, 0, instance_state, perfetto::BufferExhaustedPolicy::kDrop);
}
