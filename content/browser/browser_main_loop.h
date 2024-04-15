// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_MAIN_LOOP_H_
#define CONTENT_BROWSER_BROWSER_MAIN_LOOP_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/browser_process_io_thread.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_main_runner.h"
#include "media/media_buildflags.h"
#include "services/viz/public/mojom/compositing/compositing_mode_watcher.mojom.h"
#include "ui/base/buildflags.h"
#include "ui/base/ozone_buildflags.h"

#if defined(USE_AURA)
namespace aura {
class Env;
}
#endif

namespace base {
class CommandLine;
class HighResolutionTimerManager;
class MemoryPressureMonitor;
class SingleThreadTaskRunner;
class SystemMonitor;
}  // namespace base

namespace data_decoder {
class ServiceProvider;
}

namespace gpu {
class GpuChannelEstablishFactory;
}

namespace media {
class AudioManager;
class AudioSystem;
#if BUILDFLAG(IS_WIN)
class SystemMessageWindowWin;
#elif (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(USE_UDEV)
class DeviceMonitorLinux;
#endif
class UserInputMonitor;
}  // namespace media

namespace midi {
class MidiService;
}  // namespace midi

namespace mojo {
namespace core {
class ScopedIPCSupport;
}  // namespace core
}  // namespace mojo

namespace net {
class NetworkChangeNotifier;
}  // namespace net

namespace viz {
class CompositingModeReporterImpl;
class HostFrameSinkManager;
}  // namespace viz

namespace content {
class BrowserAccessibilityStateImpl;
class BrowserMainParts;
class BackgroundTracingManager;
class BrowserOnlineStateObserver;
class BrowserThreadImpl;
class MediaKeysListenerManagerImpl;
class MediaStreamManager;
class SaveFileManager;
class ScreenlockMonitor;
class SmsProvider;
class SpeechRecognitionManagerImpl;
class StartupTaskRunner;
class TracingControllerImpl;
struct MainFunctionParams;

namespace responsiveness {
class Watcher;
}  // namespace responsiveness

#if BUILDFLAG(IS_ANDROID)
class ScreenOrientationDelegate;
#endif

// Implements the main browser loop stages called from BrowserMainRunner.
// See comments in browser_main_parts.h for additional info.
class CONTENT_EXPORT BrowserMainLoop {
 public:
  // Returns the current instance. This is used to get access to the getters
  // that return objects which are owned by this class.
  static BrowserMainLoop* GetInstance();

  static media::AudioManager* GetAudioManager();

  // The ThreadPoolInstance must exist but not to be started when building
  // BrowserMainLoop.
  explicit BrowserMainLoop(
      MainFunctionParams parameters,
      std::unique_ptr<base::ThreadPoolInstance::ScopedExecutionFence> fence);

  BrowserMainLoop(const BrowserMainLoop&) = delete;
  BrowserMainLoop& operator=(const BrowserMainLoop&) = delete;

  virtual ~BrowserMainLoop();

  void Init();

  // Return value is exit status. Anything other than RESULT_CODE_NORMAL_EXIT
  // is considered an error.
  int EarlyInitialization();

  // Initializes the toolkit. Returns whether the toolkit initialization was
  // successful or not.
  bool InitializeToolkit();

  void PreCreateMainMessageLoop();
  // Creates the main message loop, bringing APIs like
  // SingleThreadTaskRunner::GetCurrentDefault() online.
  void CreateMainMessageLoop();
  void PostCreateMainMessageLoop();

  // Creates a "bare" message loop that is required to exit gracefully at the
  // early stage if the toolkit failed to initialise.
  void CreateMessageLoopForEarlyShutdown();

  // Create and start running the tasks we need to complete startup. Note that
  // this can be called more than once (currently only on Android) if we get a
  // request for synchronous startup while the tasks created by asynchronous
  // startup are still running. Completes tasks synchronously as part of this
  // method on non-Android platforms.
  void CreateStartupTasks();

  // Performs the default message loop run logic.
  void RunMainMessageLoop();

  // Performs the pre-shutdown steps.
  void PreShutdown();

  // Performs the shutdown sequence, starting with PostMainMessageLoopRun
  // through stopping threads to PostDestroyThreads.
  void ShutdownThreadsAndCleanUp();

  int GetResultCode() const { return result_code_; }

  // Needed by some embedders.
  void SetResultCode(int code) { result_code_ = code; }

  media::AudioManager* audio_manager() const;
  bool AudioServiceOutOfProcess() const;
  media::AudioSystem* audio_system() const { return audio_system_.get(); }
  MediaStreamManager* media_stream_manager() const {
    return media_stream_manager_.get();
  }
  media::UserInputMonitor* user_input_monitor() const {
    return user_input_monitor_.get();
  }
  MediaKeysListenerManagerImpl* media_keys_listener_manager() const {
    return media_keys_listener_manager_.get();
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Only expose this on ChromeOS since it's only needed there. On Android this
  // be null if this process started in reduced mode.
  net::NetworkChangeNotifier* network_change_notifier() const {
    return network_change_notifier_.get();
  }
#endif

  midi::MidiService* midi_service() const { return midi_service_.get(); }

  // Returns the task runner for tasks that that are critical to producing a new
  // CompositorFrame on resize. On Mac this will be the task runner provided by
  // WindowResizeHelperMac, on other platforms it will just be the thread task
  // runner.
  scoped_refptr<base::SingleThreadTaskRunner> GetResizeTaskRunner();

  gpu::GpuChannelEstablishFactory* gpu_channel_establish_factory() const;

#if BUILDFLAG(IS_ANDROID)
  void SynchronouslyFlushStartupTasks();

  // |enabled| Whether or not CreateStartupTasks() posts any tasks. This is
  // useful because some javatests want to test native task posting without the
  // whole browser loaded. In that scenario tasks posted by CreateStartupTasks()
  // may crash if run.
  static void EnableStartupTasks(bool enabled);
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
  // TODO(fsamuel): We should find an object to own HostFrameSinkManager on all
  // platforms including Android. See http://crbug.com/732507.
  viz::HostFrameSinkManager* host_frame_sink_manager() const {
    return host_frame_sink_manager_.get();
  }
#endif

  // Binds a receiver to the singleton CompositingModeReporter.
  void GetCompositingModeReporter(
      mojo::PendingReceiver<viz::mojom::CompositingModeReporter> receiver);

  SmsProvider* GetSmsProvider();
  void SetSmsProviderForTesting(std::unique_ptr<SmsProvider>);

  BrowserMainParts* parts() { return parts_.get(); }

  // This should only be called after the IO thread has been started (and will
  // crash otherwise). May block on the thread ID being initialized if the IO
  // thread ThreadMain has not yet run.
  base::PlatformThreadId GetIOThreadId();

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserMainLoopTest, CreateThreadsInSingleProcess);
  FRIEND_TEST_ALL_PREFIXES(
      BrowserMainLoopTest,
      PostTaskToIOThreadBeforeThreadCreationDoesNotRunTask);

  // Called just before creating the threads
  int PreCreateThreads();

  // Create all secondary threads.
  int CreateThreads();

  // Called just after creating the threads.
  int PostCreateThreads();
  void PostCreateThreadsImpl();

  int PreMainMessageLoopRun();

  // One last opportunity to intercept the upcoming MainMessageLoopRun (or
  // before yielding to the native loop on Android). Returns false iff the run
  // should proceed after this call.
  using ProceedWithMainMessageLoopRun =
      base::StrongAlias<class ProceedWithMainMessageLoopRunTag, bool>;
  ProceedWithMainMessageLoopRun InterceptMainMessageLoopRun();

  void MainMessageLoopRun();

  void InitializeMojo();

  void InitializeAudio();

  bool UsingInProcessGpu() const;

  void InitializeMemoryManagementComponent();

  // Quick reference for initialization order:
  // Constructor
  // Init()
  // EarlyInitialization()
  // InitializeToolkit()
  // PreCreateMainMessageLoop()
  // CreateMainMessageLoop()
  // PostCreateMainMessageLoop()
  // CreateStartupTasks()
  //   PreCreateThreads()
  //     InitializeMemoryManagementComponent()
  //   CreateThreads()
  //   PostCreateThreads()
  //     PostCreateThreadsImpl()
  //       InitializeMojo()
  //       InitializeAudio()
  // PreMainMessageLoopRun()
  // MainMessageLoopRun()
  //   OnFirstIdle()

  // Members initialized on construction ---------------------------------------
  MainFunctionParams parameters_;
  const raw_ref<const base::CommandLine> parsed_command_line_;
  int result_code_;
  bool created_threads_;  // True if the non-UI threads were created.
  // //content must be initialized single-threaded until
  // BrowserMainLoop::CreateThreads() as things initialized before it require an
  // initialize-once happens-before relationship with all eventual content tasks
  // running on other threads. This ScopedExecutionFence ensures that no tasks
  // posted to ThreadPool gets to run before CreateThreads(); satisfying this
  // requirement even though the ThreadPoolInstance is created and started
  // before content is entered.
  std::unique_ptr<base::ThreadPoolInstance::ScopedExecutionFence>
      scoped_execution_fence_;

  // BEST_EFFORT tasks are not allowed to run between //content initialization
  // and startup completion.
  //
  // TODO(fdoray): Move this to a more elaborate class that prevents BEST_EFFORT
  // tasks from running when resources are needed to respond to user actions.
  std::optional<base::ThreadPoolInstance::ScopedBestEffortExecutionFence>
      scoped_best_effort_execution_fence_;

  // Members initialized in |Init()| -------------------------------------------
  std::unique_ptr<mojo::core::ScopedIPCSupport> mojo_ipc_support_;

  // Members initialized in |InitializeToolkit()| ------------------------------
#if defined(USE_AURA)
  std::unique_ptr<aura::Env> env_;
#endif

  // Members initialized in |PostCreateMainMessageLoop()| ----------------------
  std::unique_ptr<base::SystemMonitor> system_monitor_;
  std::unique_ptr<base::HighResolutionTimerManager> hi_res_timer_manager_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  std::unique_ptr<ScreenlockMonitor> screenlock_monitor_;
  // Per-process listener for online state changes.
  std::unique_ptr<BrowserOnlineStateObserver> online_state_observer_;
#if BUILDFLAG(IS_ANDROID)
  // Android implementation of ScreenOrientationDelegate
  std::unique_ptr<ScreenOrientationDelegate> screen_orientation_delegate_;
#endif
  std::unique_ptr<BrowserAccessibilityStateImpl> browser_accessibility_state_;

  // Destroy |parts_| before above members (except the ones that are explicitly
  // reset() on shutdown) but after |main_thread_| and services below.
  std::unique_ptr<BrowserMainParts> parts_;

  // Members initialized in |CreateMainMessageLoop()| --------------------------
  // This must get destroyed before other threads that are created in |parts_|.
  std::unique_ptr<BrowserThreadImpl> main_thread_;

  // Members initialized in |CreateStartupTasks()| -----------------------------
  std::unique_ptr<StartupTaskRunner> startup_task_runner_;

  // Members initialized in |PreCreateThreads()| -------------------------------
  // Torn down in ShutdownThreadsAndCleanUp.
  std::unique_ptr<base::MemoryPressureMonitor> memory_pressure_monitor_;

  // Members initialized in |CreateThreads()| ----------------------------------
  std::unique_ptr<BrowserProcessIOThread> io_thread_;

  // BEGIN Members initialized in |PostCreateThreads()| ------------------------
  // ***************************************************************************
  std::unique_ptr<MediaKeysListenerManagerImpl> media_keys_listener_manager_;

  // |user_input_monitor_| has to outlive |audio_manager_|, so declared first.
  std::unique_ptr<media::UserInputMonitor> user_input_monitor_;

  // Support for out-of-process Data Decoder.
  std::unique_ptr<data_decoder::ServiceProvider> data_decoder_service_provider_;

  // |audio_manager_| is not instantiated when the audio service runs out of
  // process.
  std::unique_ptr<media::AudioManager> audio_manager_;

  std::unique_ptr<media::AudioSystem> audio_system_;

  std::unique_ptr<midi::MidiService> midi_service_;

  // Must be deleted on the IO thread.
  std::unique_ptr<SpeechRecognitionManagerImpl> speech_recognition_manager_;

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<media::SystemMessageWindowWin> system_message_window_;
#elif (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(USE_UDEV)
  std::unique_ptr<media::DeviceMonitorLinux> device_monitor_linux_;
#endif

  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  scoped_refptr<SaveFileManager> save_file_manager_;
  std::unique_ptr<content::TracingControllerImpl> tracing_controller_;
  std::unique_ptr<BackgroundTracingManager> background_tracing_manager_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<viz::HostFrameSinkManager> host_frame_sink_manager_;

  // Reports on the compositing mode in the system for clients to submit
  // resources of the right type. This is null if the display compositor
  // is not in this process.
  std::unique_ptr<viz::CompositingModeReporterImpl>
      compositing_mode_reporter_impl_;
#endif
  // ***************************************************************************
  // END Members initialized in |PostCreateThreads()| --------------------------

  // Members initialized in |PreMainMessageLoopRun()| --------------------------
  scoped_refptr<responsiveness::Watcher> responsiveness_watcher_;
  base::CallbackListSubscription idle_callback_subscription_;

  // Members not associated with a specific phase.
  std::unique_ptr<SmsProvider> sms_provider_;

  // DO NOT add members here. Add them to the right categories above.
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_MAIN_LOOP_H_
