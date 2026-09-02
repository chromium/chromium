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
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_data_source_names.h"
#include "third_party/perfetto/include/perfetto/tracing/core/chrome_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_descriptor.h"
#include "third_party/perfetto/include/perfetto/tracing/platform.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/stack_sampling_profiler.gen.h"
#include "third_party/perfetto/protos/perfetto/config/data_source_config.gen.h"
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

constexpr base::TimeDelta kDefaultSamplingInterval = base::Milliseconds(50);

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
    bool inserted =
        data_sources_.emplace(data_source->instance_index(), data_source)
            .second;
    DCHECK(inserted);
    for (auto& [profiler, aux_factory] : profilers_) {
      data_source->StartTracing(profiler, aux_factory);
    }
  }

  void OnDataSourceStop(TracingSamplerProfiler::DataSource* data_source) {
    base::AutoLock lock(lock_);
    size_t erased = data_sources_.erase(data_source->instance_index());
    DCHECK_EQ(erased, 1u);
  }

  void RegisterProfiler(TracingSamplerProfiler* profiler) {
    base::AutoLock lock(lock_);
    auto [it, inserted] = profilers_.try_emplace(profiler);
    if (!inserted) {
      return;
    }
    for (auto& [instance_index, data_source] : data_sources_) {
      data_source->StartTracing(profiler, it->second);
    }
  }

  void UnregisterProfiler(TracingSamplerProfiler* profiler) {
    base::AutoLock lock(lock_);
    if (!profilers_.erase(profiler)) {
      return;
    }
    for (auto& [instance_index, data_source] : data_sources_) {
      data_source->StopTracing(profiler);
    }
  }

  void SetAuxUnwinderFactory(
      TracingSamplerProfiler* profiler,
      const base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>&
          factory) {
    base::AutoLock lock(lock_);
    auto it = profilers_.find(profiler);
    if (it != profilers_.end()) {
      it->second = factory;
    }
    for (auto& [instance_index, data_source] : data_sources_) {
      data_source->SetAuxUnwinderFactory(profiler, factory);
    }
  }

 private:
  friend class base::NoDestructor<TracingSamplerProfilerManager>;

  TracingSamplerProfilerManager() = default;
  ~TracingSamplerProfilerManager() = default;

  base::Lock lock_;  // Protects subsequent members.
  base::flat_map<raw_ptr<TracingSamplerProfiler>,
                 base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>>
      profilers_ GUARDED_BY(lock_);
  base::flat_map<uint32_t, raw_ptr<TracingSamplerProfiler::DataSource>>
      data_sources_ GUARDED_BY(lock_);
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
  const std::string& raw_config =
      args.config->chromium_stack_sampling_profiler_raw();
  if (!raw_config.empty()) {
    perfetto::protos::gen::ChromiumStackSamplingProfilerConfig config;
    if (config.ParseFromArray(raw_config.data(), raw_config.size()) &&
        config.has_sampling_interval_ms() &&
        config.sampling_interval_ms() > 0) {
      sampling_interval_ = base::Milliseconds(config.sampling_interval_ms());
    }
  }
}

void TracingSamplerProfiler::DataSource::OnStart(const StartArgs& args) {
  instance_index_ = args.internal_instance_index;
  TracingSamplerProfilerManager::Get()->OnDataSourceStart(this);
}

void TracingSamplerProfiler::DataSource::OnStop(const StopArgs&) {
  TracingSamplerProfilerManager::Get()->OnDataSourceStop(this);
  base::AutoLock lock(lock_);
  for (auto& [thread_id, session] : sessions_) {
    session.profile_builder = nullptr;
    if (session.profiler) {
      session.profiler->Stop();
    }
  }
  sessions_.clear();
}

void TracingSamplerProfiler::DataSource::WillClearIncrementalState(
    const ClearIncrementalStateArgs& args) {
  base::AutoLock lock(lock_);
  for (auto& [thread_id, session] : sessions_) {
    if (session.profile_builder) {
      session.profile_builder->ResetIncrementalState();
    }
  }
}

void TracingSamplerProfiler::DataSource::StartTracing(
    TracingSamplerProfiler* profiler,
    const base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>&
        aux_unwinder_factory) {
  base::AutoLock lock(lock_);
  DCHECK(profiler);
  base::PlatformThreadId thread_id = profiler->sampled_thread_token_.id;
  if (sessions_.contains(thread_id)) {
    return;
  }

  if (!base::StackSamplingProfiler::IsSupportedForCurrentPlatform()) {
    return;
  }

  base::StackSamplingProfiler::SamplingParams params;
  params.samples_per_profile = std::numeric_limits<int>::max();
  params.sampling_interval = sampling_interval_.is_zero()
                                 ? kDefaultSamplingInterval
                                 : sampling_interval_;

  auto profile_builder = std::make_unique<TracingProfileBuilder>(
      thread_id, CreateTraceWriter(), privacy_filtering_enabled_,
      profiler->sample_callback_for_testing_);

  TracingProfileBuilder* profile_builder_ptr = profile_builder.get();
  auto unwinder_type = profiler->unwinder_type_;
  std::unique_ptr<base::StackSamplingProfiler> sampling_profiler;

#if BUILDFLAG(IS_ANDROID)
  base::StackSamplingProfiler::UnwindersFactory core_unwinders_factory;
  if (profiler->core_unwinders_factory_function_) {
    core_unwinders_factory = profiler->core_unwinders_factory_function_.Run();
  }
  if (core_unwinders_factory) {
    if (unwinder_type == UnwinderType::kUnknown) {
      unwinder_type = UnwinderType::kCustomAndroid;
    }
    profile_builder_ptr->SetUnwinderType(unwinder_type);
    sampling_profiler = std::make_unique<base::StackSamplingProfiler>(
        profiler->sampled_thread_token_, params, std::move(profile_builder),
        std::move(core_unwinders_factory));
  }
#else   // BUILDFLAG(IS_ANDROID)
  if (unwinder_type == UnwinderType::kUnknown) {
    unwinder_type = UnwinderType::kDefault;
  }
  profile_builder_ptr->SetUnwinderType(unwinder_type);
  sampling_profiler = std::make_unique<base::StackSamplingProfiler>(
      profiler->sampled_thread_token_, params, std::move(profile_builder),
      base::CreateCoreUnwindersFactory());
#endif  // BUILDFLAG(IS_ANDROID)

  if (sampling_profiler != nullptr) {
    if (aux_unwinder_factory) {
      sampling_profiler->AddAuxUnwinder(aux_unwinder_factory.Run());
    }
    sampling_profiler->Start();
    sessions_.emplace(
        thread_id, Session{std::move(sampling_profiler), profile_builder_ptr});
  }
}

void TracingSamplerProfiler::DataSource::StopTracing(
    TracingSamplerProfiler* profiler) {
  base::AutoLock lock(lock_);
  DCHECK(profiler);
  auto it = sessions_.find(profiler->sampled_thread_token_.id);
  if (it == sessions_.end()) {
    return;
  }
  it->second.profile_builder = nullptr;
  if (it->second.profiler) {
    it->second.profiler->Stop();
  }
  sessions_.erase(it);
}

void TracingSamplerProfiler::DataSource::SetAuxUnwinderFactory(
    TracingSamplerProfiler* profiler,
    const base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>& factory) {
  base::AutoLock lock(lock_);
  DCHECK(profiler);
  auto it = sessions_.find(profiler->sampled_thread_token_.id);
  if (it != sessions_.end() && it->second.profiler) {
    it->second.profiler->AddAuxUnwinder(factory.Run());
  }
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

void TracingSamplerProfiler::TracingProfileBuilder::ResetIncrementalState() {
  reset_incremental_state_.store(true, std::memory_order_relaxed);
}

void TracingSamplerProfiler::TracingProfileBuilder::WriteSampleToTrace(
    std::vector<base::Frame> frames,
    base::TimeTicks sample_timestamp) {
  bool reset_incremental_state =
      reset_incremental_state_.exchange(false, std::memory_order_relaxed);

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
    if (base::GetCurrentProcId() != perfetto::Platform::GetCurrentProcessId()) {
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
  dsd.set_name(kSamplerProfilerSourceName);
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

void TracingSamplerProfiler::SetAuxUnwinderFactory(
    const base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>& factory) {
  TracingSamplerProfilerManager::Get()->SetAuxUnwinderFactory(this, factory);
}

void TracingSamplerProfiler::SetSampleCallbackForTesting(
    const base::RepeatingClosure& sample_callback_for_testing) {
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

}  // namespace tracing

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS(
    COMPONENT_EXPORT(TRACING_CPP),
    tracing::TracingSamplerProfiler::DataSource);

// This should go after PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS
// to avoid instantiation of type() template method before specialization.
// static
std::unique_ptr<perfetto::TraceWriterBase>
tracing::TracingSamplerProfiler::DataSource::CreateTraceWriter(
    uint32_t instance_index) {
  perfetto::internal::DataSourceStaticState* static_state =
      perfetto::DataSourceHelper<TracingSamplerProfiler::DataSource>::type()
          .static_state();
  perfetto::internal::DataSourceState* instance_state =
      static_state->TryGet(instance_index);
  CHECK(instance_state);
  return perfetto::internal::TracingMuxer::Get()->CreateTraceWriter(
      static_state, instance_index, instance_state,
      perfetto::BufferExhaustedPolicy::kDrop);
}

std::unique_ptr<perfetto::TraceWriterBase>
tracing::TracingSamplerProfiler::DataSource::CreateTraceWriter() {
  return CreateTraceWriter(instance_index_);
}
