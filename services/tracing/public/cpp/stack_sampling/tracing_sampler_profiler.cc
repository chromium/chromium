// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"

#include <limits>
#include <set>

#include "base/debug/leak_annotations.h"
#include "base/hash/hash.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/profile_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.pbzero.h"

#if defined(OS_ANDROID)
#include "base/android/reached_code_profiler.h"
#endif

#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
#include <dlfcn.h>

#include "base/trace_event/cfi_backtrace_android.h"
#include "services/tracing/public/cpp/stack_sampling/stack_sampler_android.h"
#endif

using StreamingProfilePacketHandle =
    protozero::MessageHandle<perfetto::protos::pbzero::StreamingProfilePacket>;

namespace tracing {

namespace {

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

  void SetupStartupTracing() {
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

base::ThreadLocalStorage::Slot* GetThreadLocalStorageProfilerSlot() {
  static base::NoDestructor<base::ThreadLocalStorage::Slot>
      thread_local_profiler_tls([](void* profiler) {
        delete static_cast<TracingSamplerProfiler*>(profiler);
      });

  return thread_local_profiler_tls.get();
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
    std::unique_ptr<perfetto::TraceWriter> trace_writer,
    bool should_enable_filtering)
    : sampled_thread_id_(sampled_thread_id),
      trace_writer_(std::move(trace_writer)),
      should_enable_filtering_(should_enable_filtering) {}

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
    std::vector<base::Frame> frames) {
  base::AutoLock l(trace_writer_lock_);
  if (!trace_writer_) {
    if (buffered_samples_.size() < kMaxBufferedSamples) {
      buffered_samples_.emplace_back(
          BufferedSample(TRACE_TIME_TICKS_NOW(), std::move(frames)));
    }
    return;
  }
  if (!buffered_samples_.empty()) {
    for (const auto& sample : buffered_samples_) {
      WriteSampleToTrace(sample);
    }
    buffered_samples_.clear();
  }
  WriteSampleToTrace(BufferedSample(TRACE_TIME_TICKS_NOW(), std::move(frames)));
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
    trace_packet->set_incremental_state_cleared(true);

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

  int32_t current_process_priority = base::Process::Current().GetPriority();
  if (current_process_priority != last_emitted_process_priority_) {
    last_emitted_process_priority_ = current_process_priority;
    auto trace_packet = trace_writer_->NewTracePacket();
    auto* process_descriptor = trace_packet->set_process_descriptor();
    process_descriptor->set_pid(base::GetCurrentProcId());
    process_descriptor->set_process_priority(current_process_priority);
  }

  auto trace_packet = trace_writer_->NewTracePacket();
  auto callstack_id = GetCallstackIDAndMaybeEmit(frames, &trace_packet);
  auto* streaming_profile_packet = trace_packet->set_streaming_profile_packet();
  streaming_profile_packet->add_callstack_iid(callstack_id);
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
    std::string frame_name;
    std::string module_name;
    std::string module_id;
    uintptr_t rel_pc = 0;

#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
    Dl_info info = {};
    // For chrome address we do not have symbols on the binary. So, just write
    // the offset address. For addresses on framework libraries, symbolize
    // and write the function name.
    if (frame.instruction_pointer == 0) {
      frame_name = "Scanned";
    } else if (base::trace_event::CFIBacktraceAndroid::is_chrome_address(
                   frame.instruction_pointer)) {
      rel_pc = frame.instruction_pointer -
               base::trace_event::CFIBacktraceAndroid::executable_start_addr();
    } else if (dladdr(reinterpret_cast<void*>(frame.instruction_pointer),
                      &info) != 0) {
      // TODO(ssid): Add offset and module debug id if symbol was not resolved
      // in case this might be useful to send report to vendors.
      if (info.dli_sname)
        frame_name = info.dli_sname;
      if (info.dli_fname)
        module_name = info.dli_fname;
    }

    if (frame.module) {
      module_id = frame.module->GetId();
      if (module_name.empty())
        module_name = frame.module->GetDebugBasename().MaybeAsASCII();
    }

    // If no module is available, then name it unknown. Adding PC would be
    // useless anyway.
    if (module_name.empty()) {
      DCHECK(!base::trace_event::CFIBacktraceAndroid::is_chrome_address(
          frame.instruction_pointer));
      frame_name = "Unknown";
      rel_pc = 0;
    }
#else
    if (frame.module) {
      module_name = frame.module->GetDebugBasename().MaybeAsASCII();
      module_id = frame.module->GetId();
      rel_pc = frame.instruction_pointer - frame.module->GetBaseAddress();
    } else {
      module_name = module_id = "";
      frame_name = "Unknown";
    }
#endif

#if defined(OS_ANDROID) || defined(OS_LINUX)
    // Linux ELF module IDs are 160bit integers, which we need to mangle
    // down to 128bit integers to match the id that Breakpad outputs.
    // Example on version '66.0.3359.170' x64:
    //   Build-ID: "7f0715c2 86f8 b16c 10e4ad349cda3b9b 56c7a773
    //   Debug-ID  "C215077F F886 6CB1 10E4AD349CDA3B9B 0"
    if (module_id.size() >= 32) {
      module_id =
          base::StrCat({module_id.substr(6, 2), module_id.substr(4, 2),
                        module_id.substr(2, 2), module_id.substr(0, 2),
                        module_id.substr(10, 2), module_id.substr(8, 2),
                        module_id.substr(14, 2), module_id.substr(12, 2),
                        module_id.substr(16, 16), "0"});
    }
#endif

    // We never emit frame names in privacy filtered mode.
    bool should_emit_frame_names =
        !frame_name.empty() && !should_enable_filtering_;

    if (should_enable_filtering_ && !rel_pc && frame.module) {
      rel_pc = frame.instruction_pointer - frame.module->GetBaseAddress();
    }

    InterningIndexEntry interned_frame;
    if (should_emit_frame_names) {
      interned_frame =
          interned_frames_.LookupOrAdd(std::make_pair(frame_name, module_id));
    } else {
      interned_frame =
          interned_frames_.LookupOrAdd(std::make_pair(rel_pc, module_id));
    }

    if (!interned_frame.was_emitted) {
      InterningIndexEntry interned_frame_name;
      if (should_emit_frame_names) {
        interned_frame_name = interned_frame_names_.LookupOrAdd(frame_name);
        if (!interned_frame_name.was_emitted) {
          auto* frame_name_entry = interned_data->add_function_names();
          frame_name_entry->set_iid(interned_frame_name.id);
          frame_name_entry->set_str(
              reinterpret_cast<const uint8_t*>(frame_name.data()),
              frame_name.length());
        }
      }

      InterningIndexEntry interned_module;
      if (frame.module) {
        interned_module =
            interned_modules_.LookupOrAdd(frame.module->GetBaseAddress());
        if (!interned_module.was_emitted) {
          InterningIndexEntry interned_module_id =
              interned_module_ids_.LookupOrAdd(module_id);
          if (!interned_module_id.was_emitted) {
            auto* module_id_entry = interned_data->add_build_ids();
            module_id_entry->set_iid(interned_module_id.id);
            module_id_entry->set_str(
                reinterpret_cast<const uint8_t*>(module_id.data()),
                module_id.length());
          }

          InterningIndexEntry interned_module_name =
              interned_module_names_.LookupOrAdd(module_name);
          if (!interned_module_name.was_emitted) {
            auto* module_name_entry = interned_data->add_mapping_paths();
            module_name_entry->set_iid(interned_module_name.id);
            module_name_entry->set_str(
                reinterpret_cast<const uint8_t*>(module_name.data()),
                module_name.length());
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
        frame_entry->set_rel_pc(rel_pc);
      }
      if (interned_module.id) {
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

// static
std::unique_ptr<TracingSamplerProfiler>
TracingSamplerProfiler::CreateOnMainThread() {
  return std::make_unique<TracingSamplerProfiler>(
      base::GetSamplingProfilerCurrentThreadToken());
}

// static
void TracingSamplerProfiler::CreateOnChildThread() {
  auto* slot = GetThreadLocalStorageProfilerSlot();
  if (slot->Get())
    return;

  auto* profiler =
      new TracingSamplerProfiler(base::GetSamplingProfilerCurrentThreadToken());
  slot->Set(profiler);
}

// static
void TracingSamplerProfiler::DeleteOnChildThreadForTesting() {
  auto* profiler = GetThreadLocalStorageProfilerSlot()->Get();
  if (profiler) {
    delete static_cast<TracingSamplerProfiler*>(profiler);
    GetThreadLocalStorageProfilerSlot()->Set(nullptr);
  }
}

// static
void TracingSamplerProfiler::RegisterDataSource() {
  PerfettoTracedProcess::Get()->AddDataSource(
      TracingSamplerProfilerDataSource::Get());
}

// static
void TracingSamplerProfiler::StartTracingForTesting(
    PerfettoProducer* producer) {
  TracingSamplerProfilerDataSource::Get()->StartTracingWithID(
      1, producer, perfetto::DataSourceConfig());
}

// static
void TracingSamplerProfiler::SetupStartupTracing() {
  TracingSamplerProfilerDataSource::Get()->SetupStartupTracing();
}

// static
void TracingSamplerProfiler::StopTracingForTesting() {
  TracingSamplerProfilerDataSource::Get()->StopTracing(base::DoNothing());
}

TracingSamplerProfiler::TracingSamplerProfiler(
    base::SamplingProfilerThreadToken sampled_thread_token)
    : sampled_thread_token_(sampled_thread_token) {
  DCHECK_NE(sampled_thread_token_.id, base::kInvalidThreadId);
  TracingSamplerProfilerDataSource::Get()->RegisterProfiler(this);
}

TracingSamplerProfiler::~TracingSamplerProfiler() {
  TracingSamplerProfilerDataSource::Get()->UnregisterProfiler(this);
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

#if defined(OS_ANDROID)
  // The sampler profiler would conflict with the reached code profiler if they
  // run at the same time because they use the same signal to suspend threads.
  if (base::android::IsReachedCodeProfilerEnabled())
    return;
#endif

  base::StackSamplingProfiler::SamplingParams params;
  params.samples_per_profile = std::numeric_limits<int>::max();
  params.sampling_interval = base::TimeDelta::FromMilliseconds(50);
  // If the sampled thread is stopped for too long for sampling then it is ok to
  // get next sample at a later point of time. We do not want very accurate
  // metrics when looking at traces.
  params.keep_consistent_sampling_interval = false;

  auto profile_builder = std::make_unique<TracingProfileBuilder>(
      sampled_thread_token_.id, std::move(trace_writer),
      should_enable_filtering);
  profile_builder_ = profile_builder.get();
  // Create and start the stack sampling profiler.
#if defined(OS_ANDROID)
#if BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && defined(OFFICIAL_BUILD)
  auto* module_cache = profile_builder->GetModuleCache();
  profiler_ = std::make_unique<base::StackSamplingProfiler>(
      sampled_thread_token_, params, std::move(profile_builder),
      std::make_unique<StackSamplerAndroid>(sampled_thread_token_,
                                            module_cache));
  profiler_->Start();
#endif  // BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && defined(OFFICIAL_BUILD)
#else   // defined(OS_ANDROID)
  profiler_ = std::make_unique<base::StackSamplingProfiler>(
      sampled_thread_token_, params, std::move(profile_builder));
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
