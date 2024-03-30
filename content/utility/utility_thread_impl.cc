// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_thread_impl.h"

#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/process/current_process.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/public/utility/content_utility_client.h"
#include "content/utility/browser_exposed_utility_interfaces.h"
#include "content/utility/services.h"
#include "content/utility/utility_blink_platform_with_sandbox_support_impl.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/service_factory.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "content/child/sandboxed_process_thread_type_handler.h"
#endif

namespace content {

namespace {

class ServiceBinderImpl {
 public:
  explicit ServiceBinderImpl(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner)
      : main_thread_task_runner_(std::move(main_thread_task_runner)) {}

  ServiceBinderImpl(const ServiceBinderImpl&) = delete;
  ServiceBinderImpl& operator=(const ServiceBinderImpl&) = delete;

  ~ServiceBinderImpl() = default;

  void BindServiceInterface(mojo::GenericPendingReceiver* receiver) {
    // Set a crash key so utility process crash reports indicate which service
    // was running in the process.
    static auto* const service_name_crash_key =
        base::debug::AllocateCrashKeyString("service-name",
                                            base::debug::CrashKeySize::Size32);
    const std::string& service_name = receiver->interface_name().value();
    base::debug::SetCrashKeyString(service_name_crash_key, service_name);

    // Traces should also indicate the service name.
    if (base::CurrentProcess::GetInstance().IsProcessNameEmpty()) {
      base::CurrentProcess::GetInstance().SetProcessType(
          GetCurrentProcessType(service_name));
    }

    // Ensure the ServiceFactory is (lazily) initialized.
    if (!io_thread_services_) {
      io_thread_services_ = std::make_unique<mojo::ServiceFactory>();
      RegisterIOThreadServices(*io_thread_services_);
    }

    // Note that this is balanced by `termination_callback` below, which is
    // always eventually run as long as the process does not begin shutting
    // down beforehand.
    ++num_service_instances_;

    auto termination_callback =
        base::BindOnce(&ServiceBinderImpl::OnServiceTerminated,
                       weak_ptr_factory_.GetWeakPtr());
    if (io_thread_services_->CanRunService(*receiver)) {
      io_thread_services_->RunService(std::move(*receiver),
                                      std::move(termination_callback));
      return;
    }

    termination_callback =
        base::BindOnce(base::IgnoreResult(&base::SequencedTaskRunner::PostTask),
                       base::SingleThreadTaskRunner::GetCurrentDefault(),
                       FROM_HERE, std::move(termination_callback));
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceBinderImpl::TryRunMainThreadService,
                       std::move(*receiver), std::move(termination_callback)));
  }

  static std::optional<ServiceBinderImpl>& GetInstanceStorage() {
    static base::NoDestructor<std::optional<ServiceBinderImpl>> storage;
    return *storage;
  }

 private:
  static void TryRunMainThreadService(mojo::GenericPendingReceiver receiver,
                                      base::OnceClosure termination_callback) {
    // NOTE: UtilityThreadImpl is the only defined subclass of UtilityThread, so
    // this cast is safe.
    auto* thread = static_cast<UtilityThreadImpl*>(UtilityThread::Get());
    thread->HandleServiceRequest(std::move(receiver),
                                 std::move(termination_callback));
  }

  void OnServiceTerminated() {
    if (--num_service_instances_ > 0)
      return;

    // There are no more services running in this process. Time to terminate.
    //
    // First ensure that shutdown also tears down |this|. This is necessary to
    // support multiple tests in the same test suite using out-of-process
    // services via the InProcessUtilityThreadHelper, and it must be done on the
    // current thread to avoid data races.
    auto main_thread_task_runner = main_thread_task_runner_;
    GetInstanceStorage().reset();
    main_thread_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&ServiceBinderImpl::ShutDownProcess));
  }

  static void ShutDownProcess() {
    UtilityThread::Get()->ReleaseProcess();
  }

  const scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;

  // Tracks the number of service instances currently running (or pending
  // creation) in this process. When the number transitions from non-zero to
  // zero, the process will self-terminate.
  int num_service_instances_ = 0;

  // Handles service requests for services that must run on the IO thread.
  std::unique_ptr<mojo::ServiceFactory> io_thread_services_;

  base::WeakPtrFactory<ServiceBinderImpl> weak_ptr_factory_{this};
};

ChildThreadImpl::Options::ServiceBinder GetServiceBinder() {
  auto& storage = ServiceBinderImpl::GetInstanceStorage();
  // NOTE: This may already be initialized from a previous call if we're in
  // single-process mode.
  if (!storage)
    storage.emplace(base::SingleThreadTaskRunner::GetCurrentDefault());
  return base::BindRepeating(&ServiceBinderImpl::BindServiceInterface,
                             base::Unretained(&storage.value()));
}

}  // namespace

UtilityThreadImpl::UtilityThreadImpl(base::RepeatingClosure quit_closure)
    : ChildThreadImpl(std::move(quit_closure),
                      ChildThreadImpl::Options::Builder()
                          .WithLegacyIPCChannel(false)
                          .ServiceBinder(GetServiceBinder())
                          .ExposesInterfacesToBrowser()
                          .Build()) {
  Init();
}

UtilityThreadImpl::UtilityThreadImpl(const InProcessChildThreadParams& params)
    : ChildThreadImpl(base::DoNothing(),
                      ChildThreadImpl::Options::Builder()
                          .WithLegacyIPCChannel(false)
                          .InBrowserProcess(params)
                          .ServiceBinder(GetServiceBinder())
                          .ExposesInterfacesToBrowser()
                          .Build()) {
  Init();
}

UtilityThreadImpl::~UtilityThreadImpl() = default;

void UtilityThreadImpl::Shutdown() {
  ChildThreadImpl::Shutdown();
}

void UtilityThreadImpl::ReleaseProcess() {
  // Ensure all main-thread services are destroyed before releasing the process.
  // This limits the risk of services incorrectly attempting to post
  // shutdown-blocking tasks once shutdown has already begun.
  main_thread_services_.reset();

  if (!IsInBrowserProcess()) {
    ChildProcess::current()->ReleaseProcess();
    return;
  }

  // Disconnect from the UtilityProcessHost to cause it to be deleted. We need
  // to take a different code path than the multi-process case because that case
  // depends on the child process going away to close the channel, but that
  // can't happen when we're in single process mode.
  DisconnectChildProcessHost();
}

void UtilityThreadImpl::EnsureBlinkInitialized() {
  EnsureBlinkInitializedInternal(/*sandbox_support=*/false);
}

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
void UtilityThreadImpl::EnsureBlinkInitializedWithSandboxSupport() {
  EnsureBlinkInitializedInternal(/*sandbox_support=*/true);
}
#endif

void UtilityThreadImpl::HandleServiceRequest(
    mojo::GenericPendingReceiver receiver,
    base::OnceClosure termination_callback) {
  if (!main_thread_services_) {
    main_thread_services_ = std::make_unique<mojo::ServiceFactory>();
    RegisterMainThreadServices(*main_thread_services_);
  }

  if (main_thread_services_->CanRunService(receiver)) {
    main_thread_services_->RunService(std::move(receiver),
                                      std::move(termination_callback));
    return;
  }

  DLOG(ERROR) << "Cannot run unknown service: " << *receiver.interface_name();
  std::move(termination_callback).Run();
}

void UtilityThreadImpl::EnsureBlinkInitializedInternal(bool sandbox_support) {
  if (blink_platform_impl_)
    return;

  // We can only initialize Blink on one thread, and in single process mode
  // we run the utility thread on a separate thread. This means that if any
  // code needs Blink initialized in the utility process, they need to have
  // another path to support single process mode.
  if (IsInBrowserProcess())
    return;

  blink_platform_impl_ =
      sandbox_support
          ? std::make_unique<UtilityBlinkPlatformWithSandboxSupportImpl>()
          : std::make_unique<blink::Platform>();
  blink::Platform::CreateMainThreadAndInitialize(blink_platform_impl_.get());
}

void UtilityThreadImpl::Init() {
  ChildProcess::current()->AddRefProcess();

  GetContentClient()->utility()->UtilityThreadStarted();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  SandboxedProcessThreadTypeHandler::NotifyMainChildThreadCreated();
#endif

  // NOTE: Do not add new interfaces directly within this method. Instead,
  // modify the definition of |ExposeUtilityInterfacesToBrowser()| to ensure
  // security review coverage.
  mojo::BinderMap binders;
  content::ExposeUtilityInterfacesToBrowser(&binders);
  ExposeInterfacesToBrowser(std::move(binders));
}

constexpr ServiceCurrentProcessType kCurrentProcessTypes[] = {
    {"network.mojom.NetworkService",
     CurrentProcessType::PROCESS_SERVICE_NETWORK},
    {"tracing.mojom.TracingService",
     CurrentProcessType::PROCESS_SERVICE_TRACING},
    {"storage.mojom.StorageService",
     CurrentProcessType::PROCESS_SERVICE_STORAGE},
    {"audio.mojom.AudioService", CurrentProcessType::PROCESS_SERVICE_AUDIO},
    {"data_decoder.mojom.DataDecoderService",
     CurrentProcessType::PROCESS_SERVICE_DATA_DECODER},
    {"chrome.mojom.UtilWin", CurrentProcessType::PROCESS_SERVICE_UTIL_WIN},
    {"proxy_resolver.mojom.ProxyResolverFactory",
     CurrentProcessType::PROCESS_SERVICE_PROXY_RESOLVER},
    {"media.mojom.CdmServiceBroker", CurrentProcessType::PROCESS_SERVICE_CDM},
    {"media.mojom.MediaFoundationServiceBroker",
     CurrentProcessType::PROCESS_SERVICE_MEDIA_FOUNDATION},
    {"video_capture.mojom.VideoCaptureService",
     CurrentProcessType::PROCESS_SERVICE_VIDEO_CAPTURE},
    {"unzip.mojom.Unzipper", CurrentProcessType::PROCESS_SERVICE_UNZIPPER},
    {"mirroring.mojom.MirroringService",
     CurrentProcessType::PROCESS_SERVICE_MIRRORING},
    {"patch.mojom.FilePatcher",
     CurrentProcessType::PROCESS_SERVICE_FILEPATCHER},
    {"chromeos.tts.mojom.TtsService", CurrentProcessType::PROCESS_SERVICE_TTS},
    {"printing.mojom.PrintingService",
     CurrentProcessType::PROCESS_SERVICE_PRINTING},
    {"quarantine.mojom.Quarantine",
     CurrentProcessType::PROCESS_SERVICE_QUARANTINE},
    {"ash.local_search_service.mojom.LocalSearchService",
     CurrentProcessType::PROCESS_SERVICE_CROS_LOCALSEARCH},
    {"ash.assistant.mojom.AssistantAudioDecoderFactory",
     CurrentProcessType::PROCESS_SERVICE_CROS_ASSISTANT_AUDIO_DECODER},
    {"chrome.mojom.FileUtilService",
     CurrentProcessType::PROCESS_SERVICE_FILEUTIL},
    {"printing.mojom.PrintCompositor",
     CurrentProcessType::PROCESS_SERVICE_PRINTCOMPOSITOR},
    {"paint_preview.mojom.PaintPreviewCompositorCollection",
     CurrentProcessType::PROCESS_SERVICE_PAINTPREVIEW},
    {"media.mojom.SpeechRecognitionService",
     CurrentProcessType::PROCESS_SERVICE_SPEECHRECOGNITION},
    {"device.mojom.XRDeviceService",
     CurrentProcessType::PROCESS_SERVICE_XRDEVICE},
    {"chrome.mojom.UtilReadIcon", CurrentProcessType::PROCESS_SERVICE_READICON},
    {"language_detection.mojom.LanguageDetectionService",
     CurrentProcessType::PROCESS_SERVICE_LANGUAGEDETECTION},
    {"sharing.mojom.Sharing", CurrentProcessType::PROCESS_SERVICE_SHARING},
    {"chrome.mojom.MediaParserFactory",
     CurrentProcessType::PROCESS_SERVICE_MEDIAPARSER},
    {"qrcode_generator.mojom.QRCodeGeneratorService",
     CurrentProcessType::PROCESS_SERVICE_QRCODEGENERATOR},
    {"chrome.mojom.ProfileImport",
     CurrentProcessType::PROCESS_SERVICE_PROFILEIMPORT},
    {"ash.ime.mojom.ImeService", CurrentProcessType::PROCESS_SERVICE_IME},
    {"recording.mojom.RecordingService",
     CurrentProcessType::PROCESS_SERVICE_RECORDING},
    {"shape_detection.mojom.ShapeDetectionService",
     CurrentProcessType::PROCESS_SERVICE_SHAPEDETECTION},
};

CurrentProcessType GetCurrentProcessType(const std::string& name) {
  for (auto kCurrentProcessType : kCurrentProcessTypes) {
    if (name == kCurrentProcessType.name) {
      return kCurrentProcessType.type;
    }
  }
  return CurrentProcessType::PROCESS_UTILITY;
}

}  // namespace content
