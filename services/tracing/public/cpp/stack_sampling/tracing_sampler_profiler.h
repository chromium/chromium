// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_TRACING_SAMPLER_PROFILER_H_
#define SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_TRACING_SAMPLER_PROFILER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/debug/debugging_buildflags.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/profiler/unwinder.h"
#include "base/sequence_checker.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/buildflags.h"
#include "services/tracing/public/cpp/perfetto/interning_index.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"

#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARM64) && \
    BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
#define ANDROID_ARM64_UNWINDING_SUPPORTED 1
#else
#define ANDROID_ARM64_UNWINDING_SUPPORTED 0
#endif

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
#define ANDROID_CFI_UNWINDING_SUPPORTED 1
#else
#define ANDROID_CFI_UNWINDING_SUPPORTED 0
#endif

namespace tracing {

class PerfettoProducer;

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
class LoaderLockSamplingThread;
#endif

// This class is a bridge between the base stack sampling profiler and chrome
// tracing. It's listening to TraceLog enabled/disabled events and it's starting
// a stack profiler on the current thread if needed. The sampling profiler is
// lazily instantiated when tracing is activated and released when tracing is
// disabled.
//
// The TracingSamplerProfiler must be created and destroyed on the sampled
// thread. The tracelog observers can be called on any thread which force the
// field |profiler_| to be thread-safe.
class COMPONENT_EXPORT(TRACING_CPP) TracingSamplerProfiler {
 public:
  class COMPONENT_EXPORT(TRACING_CPP) StackProfileWriter {
   public:
    explicit StackProfileWriter(bool should_enable_filtering);
    ~StackProfileWriter();

    StackProfileWriter(const StackProfileWriter&) = delete;
    StackProfileWriter& operator=(const StackProfileWriter&) = delete;

    // This function receives stack sample from profiler and returns InterningID
    // corresponding to the callstack. Meanwhile it could emit extra entries
    // to intern data. |function_name| member in Frame could be std::move(ed) by
    // this method to reduce number of copies we have for function names.
    InterningID GetCallstackIDAndMaybeEmit(
        std::vector<base::Frame>& frames,
        perfetto::TraceWriter::TracePacketHandle* trace_packet);

    void ResetEmittedState();

   private:
    const bool should_enable_filtering_;
    InterningIndex<TypeList<size_t>, SizeList<1024>> interned_callstacks_{};
    InterningIndex<TypeList<std::pair<std::string, std::string>,
                            std::pair<uintptr_t, std::string>>,
                   SizeList<1024, 1024>>
        interned_frames_{};
    InterningIndex<TypeList<std::string>, SizeList<1024>>
        interned_frame_names_{};
    InterningIndex<TypeList<std::string>, SizeList<1024>>
        interned_module_names_{};
    InterningIndex<TypeList<std::string>, SizeList<1024>>
        interned_module_ids_{};
    InterningIndex<TypeList<uintptr_t>, SizeList<1024>> interned_modules_{};
  };

  // Different kinds of unwinders that are used for stack sampling.
  enum class UnwinderType {
    kUnknown,
    kCustomAndroid,
    kDefault,
    kLibunwindstackUnwinderAndroid
  };

  // This class will receive the sampling profiler stackframes and output them
  // to the chrome trace via an event. Exposed for testing.
  class COMPONENT_EXPORT(TRACING_CPP) TracingProfileBuilder
      : public base::ProfileBuilder {
   public:
    TracingProfileBuilder(
        base::PlatformThreadId sampled_thread_id,
        std::unique_ptr<perfetto::TraceWriterBase> trace_writer,
        bool should_enable_filtering,
        const base::RepeatingClosure& sample_callback_for_testing =
            base::RepeatingClosure());
    ~TracingProfileBuilder() override;

    // base::ProfileBuilder
    base::ModuleCache* GetModuleCache() override;
    void OnSampleCompleted(std::vector<base::Frame> frames,
                           base::TimeTicks sample_timestamp) override;
    void OnProfileCompleted(base::TimeDelta profile_duration,
                            base::TimeDelta sampling_period) override {}

    void SetTraceWriter(
        std::unique_ptr<perfetto::TraceWriterBase> trace_writer);

    void SetUnwinderType(TracingSamplerProfiler::UnwinderType unwinder_type);

   private:
    struct BufferedSample {
      BufferedSample(base::TimeTicks, std::vector<base::Frame>&&);

      BufferedSample(const BufferedSample&) = delete;
      BufferedSample& operator=(const BufferedSample&) = delete;

      BufferedSample(BufferedSample&& other);

      ~BufferedSample();

      base::TimeTicks timestamp;
      std::vector<base::Frame> sample;
    };

    void WriteSampleToTrace(BufferedSample sample);

    // TODO(ssid): Consider using an interning scheme to reduce memory usage
    // and increase the sample size.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    // We usually sample at 50ms, and expect that tracing should have started in
    // 10s (5s for 2 threads). Approximately 100 frames and 200 samples would use
    // 300KiB.
    constexpr static size_t kMaxBufferedSamples = 200;
#else
    // 2000 samples are enough to store samples for 100 seconds (50s for 2
    // threads), and consumes about 3MiB of memory.
    constexpr static size_t kMaxBufferedSamples = 2000;
#endif
    std::vector<BufferedSample> buffered_samples_;

    base::ModuleCache module_cache_;
    const base::PlatformThreadId sampled_thread_id_;
    base::Lock trace_writer_lock_;
    std::unique_ptr<perfetto::TraceWriterBase> trace_writer_;
    StackProfileWriter stack_profile_writer_;
    bool reset_incremental_state_ = true;
    uint32_t last_incremental_state_reset_id_ = 0;
    base::TimeTicks last_timestamp_;
    base::RepeatingClosure sample_callback_for_testing_;
    // Which type of unwinder is being used for stack sampling?
    UnwinderType unwinder_type_ = UnwinderType::kUnknown;
  };

  using CoreUnwindersCallback =
      base::RepeatingCallback<base::StackSamplingProfiler::UnwindersFactory()>;

  // Creates sampling profiler on main thread. The profiler *must* be
  // destroyed prior to process shutdown. `core_unwinders_factory_function` can
  // be used to supply custom unwinders to be used during stack sampling.
  static std::unique_ptr<TracingSamplerProfiler> CreateOnMainThread(
      CoreUnwindersCallback core_unwinders_factory_function =
          CoreUnwindersCallback(),
      UnwinderType unwinder_type = UnwinderType::kUnknown);

  TracingSamplerProfiler(const TracingSamplerProfiler&) = delete;
  TracingSamplerProfiler& operator=(const TracingSamplerProfiler&) = delete;

  // Sets up tracing sampling profiler on a child thread. The profiler will be
  // stored in SequencedLocalStorageSlot and will be destroyed with the thread
  // task runner.
  static void CreateOnChildThread();

  // Same as CreateOnChildThread above, but this can additionally accept a
  // callback for supplying custom unwinder(s) to be used during stack sampling.
  static void CreateOnChildThreadWithCustomUnwinders(
      CoreUnwindersCallback core_unwinders_factory_function);

  // Registers the TracingSamplerProfiler as a Perfetto data source
  static void RegisterDataSource();

  // Sets a callback to create auxiliary unwinders on the main thread profiler,
  // for handling additional, non-native-code unwind scenarios.
  static void SetAuxUnwinderFactoryOnMainThread(
      const base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>&
          factory);

  // For tests.
  static void SetupStartupTracingForTesting();
  static void DeleteOnChildThreadForTesting();
  static void StartTracingForTesting(tracing::PerfettoProducer* producer);
  static void StopTracingForTesting();
  static void ResetDataSourceForTesting();
  // Returns whether of not the sampler profiling is able to unwind the stack
  // on this platform, ignoring any CoreUnwindersCallback provided.
  static bool IsStackUnwindingSupportedForTesting();

  explicit TracingSamplerProfiler(
      base::SamplingProfilerThreadToken sampled_thread_token,
      CoreUnwindersCallback core_unwinders_factory_function,
      UnwinderType unwinder_type = UnwinderType::kUnknown);
  virtual ~TracingSamplerProfiler();

  // Sets a callback to create auxiliary unwinders, for handling additional,
  // non-native-code unwind scenarios. Currently used to support
  // unwinding V8 JavaScript frames.
  void SetAuxUnwinderFactory(
      const base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>&
          factory);

  // The given callback will be called for every received sample, and can be
  // called on any thread. Must be called before tracing is started.
  void SetSampleCallbackForTesting(
      const base::RepeatingClosure& sample_callback_for_testing);

  void StartTracing(std::unique_ptr<perfetto::TraceWriterBase> trace_writer,
                    bool should_enable_filtering);

  void StopTracing();

 private:
  const base::SamplingProfilerThreadToken sampled_thread_token_;

  CoreUnwindersCallback core_unwinders_factory_function_;
  base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>
      aux_unwinder_factory_;
  // To differentiate b/w different unwinders used for browser main
  // thread sampling.
  // TODO(crbug.com/40243562): Remove once we have single unwinder for browser
  // main.
  UnwinderType unwinder_type_;

  base::Lock lock_;
  std::unique_ptr<base::StackSamplingProfiler> profiler_;  // under |lock_|
  // This dangling raw_ptr occurred in:
  // services_unittests: TracingSampleProfilerTest.SamplingChildThread
  // https://ci.chromium.org/ui/p/chromium/builders/try/win-rel/237204/test-results?q=ExactID%3Aninja%3A%2F%2Fservices%3Aservices_unittests%2FTracingSampleProfilerTest.SamplingChildThread+VHash%3A83af393c6a76b581
  raw_ptr<TracingProfileBuilder, FlakyDanglingUntriaged> profile_builder_ =
      nullptr;
  base::RepeatingClosure sample_callback_for_testing_;

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
  // A thread that periodically samples the loader lock. Sampling will start
  // and stop at the same time that stack sampling does.
  std::unique_ptr<LoaderLockSamplingThread> loader_lock_sampling_thread_;
#endif
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_TRACING_SAMPLER_PROFILER_H_
