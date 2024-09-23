// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/webrtc/thread_wrapper.h"
#include "crypto/openssl_util.h"
#include "media/base/media_permission.h"
#include "media/media_buildflags.h"
#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "net/net_buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/modules/peerconnection/intercepting_network_controller.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_sink_bundle.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/p2p/empty_network_manager.h"
#include "third_party/blink/renderer/platform/p2p/filtering_network_manager.h"
#include "third_party/blink/renderer/platform/p2p/ipc_network_manager.h"
#include "third_party/blink/renderer/platform/p2p/ipc_socket_factory.h"
#include "third_party/blink/renderer/platform/p2p/mdns_responder_adapter.h"
#include "third_party/blink/renderer/platform/p2p/port_allocator.h"
#include "third_party/blink/renderer/platform/p2p/socket_dispatcher.h"
#include "third_party/blink/renderer/platform/peerconnection/audio_codec_factory.h"
#include "third_party/blink/renderer/platform/peerconnection/video_codec_factory.h"
#include "third_party/blink/renderer/platform/peerconnection/vsync_provider.h"
#include "third_party/blink/renderer/platform/peerconnection/vsync_tick_provider.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_gfx.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/webrtc/api/enable_media.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/rtc_event_log/rtc_event_log_factory.h"
#include "third_party/webrtc/api/transport/goog_cc_factory.h"
#include "third_party/webrtc/api/video_track_source_proxy_factory.h"
#include "third_party/webrtc/media/engine/fake_video_codec_factory.h"
#include "third_party/webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"
#include "third_party/webrtc/rtc_base/ssl_adapter.h"
#include "third_party/webrtc_overrides/metronome_source.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"
#include "third_party/webrtc_overrides/timer_based_tick_provider.h"

namespace WTF {
template <>
struct CrossThreadCopier<base::RepeatingCallback<void(base::TimeDelta)>>
    : public CrossThreadCopierPassThrough<
          base::RepeatingCallback<void(base::TimeDelta)>> {
  STATIC_ONLY(CrossThreadCopier);
};
}  // namespace WTF

namespace blink {
namespace {

using PassKey = base::PassKey<PeerConnectionDependencyFactory>;

enum WebRTCIPHandlingPolicy {
  kDefault,
  kDefaultPublicAndPrivateInterfaces,
  kDefaultPublicInterfaceOnly,
  kDisableNonProxiedUdp,
};

WebRTCIPHandlingPolicy GetWebRTCIPHandlingPolicy(const String& preference) {
  if (preference == kWebRTCIPHandlingDefaultPublicAndPrivateInterfaces)
    return kDefaultPublicAndPrivateInterfaces;
  if (preference == kWebRTCIPHandlingDefaultPublicInterfaceOnly)
    return kDefaultPublicInterfaceOnly;
  if (preference == kWebRTCIPHandlingDisableNonProxiedUdp)
    return kDisableNonProxiedUdp;
  return kDefault;
}

bool IsValidPortRange(uint16_t min_port, uint16_t max_port) {
  DCHECK(min_port <= max_port);
  return min_port != 0 && max_port != 0;
}

scoped_refptr<media::MojoVideoEncoderMetricsProviderFactory>
CreateMojoVideoEncoderMetricsProviderFactory(LocalFrame* local_frame) {
  CHECK(local_frame);
  mojo::PendingRemote<media::mojom::VideoEncoderMetricsProvider>
      video_encoder_metrics_provider;
  local_frame->GetBrowserInterfaceBroker().GetInterface(
      video_encoder_metrics_provider.InitWithNewPipeAndPassReceiver());
  return base::MakeRefCounted<media::MojoVideoEncoderMetricsProviderFactory>(
      media::mojom::VideoEncoderUseCase::kWebRTC,
      std::move(video_encoder_metrics_provider));
}

// PeerConnectionDependencies wants to own the factory, so we provide a simple
// object that delegates calls to the IpcPacketSocketFactory.
// TODO(zstein): Move the creation logic from IpcPacketSocketFactory in to this
// class.
class ProxyAsyncDnsResolverFactory final
    : public webrtc::AsyncDnsResolverFactoryInterface {
 public:
  explicit ProxyAsyncDnsResolverFactory(IpcPacketSocketFactory* ipc_psf)
      : ipc_psf_(ipc_psf) {
    DCHECK(ipc_psf);
  }

  std::unique_ptr<webrtc::AsyncDnsResolverInterface> Create() override {
    return ipc_psf_->CreateAsyncDnsResolver();
  }
  std::unique_ptr<webrtc::AsyncDnsResolverInterface> CreateAndResolve(
      const rtc::SocketAddress& addr,
      absl::AnyInvocable<void()> callback) override {
    auto temp = Create();
    temp->Start(addr, std::move(callback));
    return temp;
  }
  std::unique_ptr<webrtc::AsyncDnsResolverInterface> CreateAndResolve(
      const rtc::SocketAddress& addr,
      int family,
      absl::AnyInvocable<void()> callback) override {
    auto temp = Create();
    temp->Start(addr, family, std::move(callback));
    return temp;
  }

 private:
  raw_ptr<IpcPacketSocketFactory, DanglingUntriaged> ipc_psf_;
};

std::string WorkerThreadName() {
  if (base::FeatureList::IsEnabled(
          features::kWebRtcCombinedNetworkAndWorkerThread)) {
    return "WebRTC_W_and_N";
  }
  return "WebRTC_Worker";
}

// Encapsulates process-wide static dependencies used by
// `PeerConnectionDependencyFactory`, namely the threads used by WebRTC. This
// avoids allocating multiple threads per factory instance, as they are
// "heavy-weight" and we don't want to create them per frame.
class PeerConnectionStaticDeps {
 public:
  PeerConnectionStaticDeps()
      : chrome_signaling_thread_("WebRTC_Signaling"),
        chrome_worker_thread_(WorkerThreadName()) {
    if (!base::FeatureList::IsEnabled(
            features::kWebRtcCombinedNetworkAndWorkerThread)) {
      chrome_network_thread_.emplace("WebRTC_Network");
    }
  }

  ~PeerConnectionStaticDeps() {
    if (chrome_worker_thread_.IsRunning()) {
      chrome_worker_thread_.task_runner()->DeleteSoon(
          FROM_HERE, std::move(decode_metronome_source_));

      if (encode_metronome_source_) {
        chrome_worker_thread_.task_runner()->DeleteSoon(
            FROM_HERE, std::move(encode_metronome_source_));
      }
    }
  }

  std::unique_ptr<webrtc::Metronome> CreateDecodeMetronome() {
    CHECK(decode_metronome_source_);
    return decode_metronome_source_->CreateWebRtcMetronome();
  }

  std::unique_ptr<webrtc::Metronome> MaybeCreateEncodeMetronome() {
    if (encode_metronome_source_) {
      return encode_metronome_source_->CreateWebRtcMetronome();
    } else {
      return nullptr;
    }
  }

  void EnsureVsyncProvider(ExecutionContext& context) {
    if (!vsync_tick_provider_) {
      vsync_provider_.emplace(
          Platform::Current()->VideoFrameCompositorTaskRunner(),
          To<LocalDOMWindow>(context)
              .GetFrame()
              ->GetPage()
              ->GetChromeClient()
              .GetFrameSinkId(To<LocalDOMWindow>(context).GetFrame())
              .client_id());
      vsync_tick_provider_ = VSyncTickProvider::Create(
          *vsync_provider_, chrome_worker_thread_.task_runner(),
          base::MakeRefCounted<TimerBasedTickProvider>(
              features::kVSyncDecodingHiddenOccludedTickDuration.Get()));
    }
  }

  void EnsureChromeThreadsStarted(ExecutionContext& context) {
    base::ThreadType thread_type = base::ThreadType::kDefault;
    if (base::FeatureList::IsEnabled(
            features::kWebRtcThreadsUseResourceEfficientType)) {
      thread_type = base::ThreadType::kResourceEfficient;
    }
    if (!chrome_signaling_thread_.IsRunning()) {
      chrome_signaling_thread_.StartWithOptions(
          base::Thread::Options(thread_type));
    }
    if (chrome_network_thread_ && !chrome_network_thread_->IsRunning()) {
      chrome_network_thread_->StartWithOptions(
          base::Thread::Options(thread_type));
    }

    if (!chrome_worker_thread_.IsRunning()) {
      chrome_worker_thread_.StartWithOptions(
          base::Thread::Options(thread_type));
    }
    // To allow sending to the signaling/worker threads.
    webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();
    webrtc::ThreadWrapper::current()->set_send_allowed(true);
    if (!decode_metronome_source_) {
      if (base::FeatureList::IsEnabled(features::kVSyncDecoding)) {
        EnsureVsyncProvider(context);
        decode_metronome_source_ =
            std::make_unique<MetronomeSource>(vsync_tick_provider_);
      } else {
        auto tick_provider = base::MakeRefCounted<TimerBasedTickProvider>(
            TimerBasedTickProvider::kDefaultPeriod);
        decode_metronome_source_ =
            std::make_unique<MetronomeSource>(std::move(tick_provider));
      }
    }
    if (base::FeatureList::IsEnabled(features::kVSyncEncoding) &&
        !encode_metronome_source_) {
      EnsureVsyncProvider(context);
      encode_metronome_source_ =
          std::make_unique<MetronomeSource>(vsync_tick_provider_);
    }
  }

  base::WaitableEvent& InitializeWorkerThread() {
    if (!worker_thread_) {
      PostCrossThreadTask(
          *chrome_worker_thread_.task_runner(), FROM_HERE,
          CrossThreadBindOnce(
              &PeerConnectionStaticDeps::InitializeOnThread,
              CrossThreadUnretained(&worker_thread_),
              CrossThreadUnretained(&init_worker_event),
              ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                  PeerConnectionStaticDeps::LogTaskLatencyWorker)),
              ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                  PeerConnectionStaticDeps::LogTaskDurationWorker))));
    }
    return init_worker_event;
  }

  base::WaitableEvent& InitializeNetworkThread() {
    if (!network_thread_) {
      if (chrome_network_thread_) {
        PostCrossThreadTask(
            *chrome_network_thread_->task_runner(), FROM_HERE,
            CrossThreadBindOnce(
                &PeerConnectionStaticDeps::InitializeOnThread,
                CrossThreadUnretained(&network_thread_),
                CrossThreadUnretained(&init_network_event),
                ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                    PeerConnectionStaticDeps::LogTaskLatencyNetwork)),
                ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                    PeerConnectionStaticDeps::LogTaskDurationNetwork))));
      } else {
        init_network_event.Signal();
      }
    }
    return init_network_event;
  }

  base::WaitableEvent& InitializeSignalingThread() {
    if (!signaling_thread_) {
      PostCrossThreadTask(
          *chrome_signaling_thread_.task_runner(), FROM_HERE,
          CrossThreadBindOnce(
              &PeerConnectionStaticDeps::InitializeOnThread,
              CrossThreadUnretained(&signaling_thread_),
              CrossThreadUnretained(&init_signaling_event),
              ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                  PeerConnectionStaticDeps::LogTaskLatencySignaling)),
              ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                  PeerConnectionStaticDeps::LogTaskDurationSignaling))));
    }
    return init_signaling_event;
  }

  rtc::Thread* GetSignalingThread() { return signaling_thread_; }
  rtc::Thread* GetWorkerThread() { return worker_thread_; }
  rtc::Thread* GetNetworkThread() {
    return chrome_network_thread_ ? network_thread_ : worker_thread_;
  }
  base::Thread& GetChromeSignalingThread() { return chrome_signaling_thread_; }
  base::Thread& GetChromeWorkerThread() { return chrome_worker_thread_; }
  base::Thread& GetChromeNetworkThread() {
    return chrome_network_thread_ ? *chrome_network_thread_
                                  : chrome_worker_thread_;
  }

 private:
  static void LogTaskLatencyWorker(base::TimeDelta sample) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "WebRTC.PeerConnection.Latency.Worker", sample, base::Microseconds(1),
        base::Seconds(10), 50);
  }
  static void LogTaskDurationWorker(base::TimeDelta sample) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "WebRTC.PeerConnection.Duration.Worker", sample, base::Microseconds(1),
        base::Seconds(10), 50);
  }
  static void LogTaskLatencyNetwork(base::TimeDelta sample) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "WebRTC.PeerConnection.Latency.Network", sample, base::Microseconds(1),
        base::Seconds(10), 50);
  }
  static void LogTaskDurationNetwork(base::TimeDelta sample) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "WebRTC.PeerConnection.Duration.Network", sample, base::Microseconds(1),
        base::Seconds(10), 50);
  }
  static void LogTaskLatencySignaling(base::TimeDelta sample) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "WebRTC.PeerConnection.Latency.Signaling", sample,
        base::Microseconds(1), base::Seconds(10), 50);
  }
  static void LogTaskDurationSignaling(base::TimeDelta sample) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "WebRTC.PeerConnection.Duration.Signaling", sample,
        base::Microseconds(1), base::Seconds(10), 50);
  }

  static void InitializeOnThread(
      raw_ptr<rtc::Thread>* thread,
      base::WaitableEvent* event,
      base::RepeatingCallback<void(base::TimeDelta)> latency_callback,
      base::RepeatingCallback<void(base::TimeDelta)> duration_callback) {
    webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();
    webrtc::ThreadWrapper::current()->set_send_allowed(true);
    webrtc::ThreadWrapper::current()->SetLatencyAndTaskDurationCallbacks(
        std::move(latency_callback), std::move(duration_callback));
    if (!*thread) {
      *thread = webrtc::ThreadWrapper::current();
      event->Signal();
    }
  }

  // PeerConnection threads. signaling_thread_ is created from the "current"
  // (main) chrome thread.
  raw_ptr<rtc::Thread> signaling_thread_ = nullptr;
  raw_ptr<rtc::Thread> worker_thread_ = nullptr;
  raw_ptr<rtc::Thread> network_thread_ = nullptr;
  base::Thread chrome_signaling_thread_;
  base::Thread chrome_worker_thread_;
  std::optional<base::Thread> chrome_network_thread_;

  // Metronome source used for driving decoding and encoding, created from
  // renderer main thread, always used and destroyed on `chrome_worker_thread_`.
  std::unique_ptr<MetronomeSource> decode_metronome_source_;
  std::unique_ptr<MetronomeSource> encode_metronome_source_;

  // WaitableEvents for observing thread initialization.
  base::WaitableEvent init_signaling_event{
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};
  base::WaitableEvent init_worker_event{
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};
  base::WaitableEvent init_network_event{
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};

  // Generates VSync ticks, these two are always allocated together.
  std::optional<VSyncProviderImpl> vsync_provider_;
  scoped_refptr<MetronomeSource::TickProvider> vsync_tick_provider_;

  THREAD_CHECKER(thread_checker_);
};

PeerConnectionStaticDeps& StaticDeps() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(PeerConnectionStaticDeps, instance, ());
  return instance;
}

rtc::Thread* GetSignalingThread() {
  return StaticDeps().GetSignalingThread();
}
rtc::Thread* GetWorkerThread() {
  return StaticDeps().GetWorkerThread();
}
rtc::Thread* GetNetworkThread() {
  return StaticDeps().GetNetworkThread();
}
base::Thread& GetChromeSignalingThread() {
  return StaticDeps().GetChromeSignalingThread();
}
base::Thread& GetChromeWorkerThread() {
  return StaticDeps().GetChromeWorkerThread();
}
base::Thread& GetChromeNetworkThread() {
  return StaticDeps().GetChromeNetworkThread();
}

class InterceptingNetworkControllerFactory
    : public webrtc::NetworkControllerFactoryInterface {
 public:
  InterceptingNetworkControllerFactory(
      scoped_refptr<base::SequencedTaskRunner> context_task_runner,
      RTCRtpTransport* rtp_transport)
      : context_task_runner_(context_task_runner),
        rtp_transport_(rtp_transport) {
    CHECK(rtp_transport);
  }

  // Note: Called on a webrtc thread.
  std::unique_ptr<webrtc::NetworkControllerInterface> Create(
      webrtc::NetworkControllerConfig config) override {
    return std::make_unique<InterceptingNetworkController>(
        goog_cc_factory_->Create(config), rtp_transport_, context_task_runner_);
  }

  // Note: Called on a webrtc thread.
  webrtc::TimeDelta GetProcessInterval() const override {
    return goog_cc_factory_->GetProcessInterval();
  }

 private:
  const std::unique_ptr<webrtc::GoogCcNetworkControllerFactory>
      goog_cc_factory_ =
          std::make_unique<webrtc::GoogCcNetworkControllerFactory>();
  const scoped_refptr<base::SequencedTaskRunner> context_task_runner_;
  // Store just a CrossThreadWeakHandle pointing at an RTCRtpTransport, to be
  // used on a webrtc thread when creating InterceptingNetworkController
  // instances.
  const CrossThreadWeakHandle<RTCRtpTransport> rtp_transport_;
};

}  // namespace

// static
const char PeerConnectionDependencyFactory::kSupplementName[] =
    "PeerConnectionDependencyFactory";

PeerConnectionDependencyFactory& PeerConnectionDependencyFactory::From(
    ExecutionContext& context) {
  CHECK(!context.IsContextDestroyed());
  auto* supplement =
      Supplement<ExecutionContext>::From<PeerConnectionDependencyFactory>(
          context);
  if (!supplement) {
    supplement = MakeGarbageCollected<PeerConnectionDependencyFactory>(
        context, PassKey());
    ProvideTo(context, supplement);
  }
  return *supplement;
}

PeerConnectionDependencyFactory::PeerConnectionDependencyFactory(
    ExecutionContext& context,
    PassKey)
    : Supplement(context),
      ExecutionContextLifecycleObserver(&context),
      context_task_runner_(
          context.GetTaskRunner(TaskType::kInternalMediaRealTime)),
      network_manager_(nullptr),
      p2p_socket_dispatcher_(P2PSocketDispatcher::From(context)) {
  // Initialize mojo pipe for encode/decode performance stats data collection.
  mojo::PendingRemote<media::mojom::blink::WebrtcVideoPerfRecorder>
      perf_recorder;
  context.GetBrowserInterfaceBroker().GetInterface(
      perf_recorder.InitWithNewPipeAndPassReceiver());

  webrtc_video_perf_reporter_ = MakeGarbageCollected<WebrtcVideoPerfReporter>(
      context.GetTaskRunner(TaskType::kInternalMedia), &context,
      std::move(perf_recorder));
}

PeerConnectionDependencyFactory::PeerConnectionDependencyFactory()
    : Supplement(nullptr), ExecutionContextLifecycleObserver(nullptr) {}

PeerConnectionDependencyFactory::~PeerConnectionDependencyFactory() = default;

std::unique_ptr<RTCPeerConnectionHandler>
PeerConnectionDependencyFactory::CreateRTCPeerConnectionHandler(
    RTCPeerConnectionHandlerClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool encoded_insertable_streams) {
  // Save histogram data so we can see how much PeerConnection is used.
  // The histogram counts the number of calls to the JS API
  // RTCPeerConnection.
  UpdateWebRTCMethodCount(RTCAPIName::kRTCPeerConnection);

  return std::make_unique<RTCPeerConnectionHandler>(client, this, task_runner,
                                                    encoded_insertable_streams);
}

const rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>&
PeerConnectionDependencyFactory::GetPcFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!pc_factory_)
    CreatePeerConnectionFactory();
  CHECK(pc_factory_);
  return pc_factory_;
}

void PeerConnectionDependencyFactory::CreatePeerConnectionFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!pc_factory_.get());
  DCHECK(!network_manager_);
  DCHECK(!socket_factory_);

  DVLOG(1) << "PeerConnectionDependencyFactory::CreatePeerConnectionFactory()";

  StaticDeps().EnsureChromeThreadsStarted(
      *ExecutionContextLifecycleObserver::GetExecutionContext());
  base::WaitableEvent& worker_thread_started_event =
      StaticDeps().InitializeWorkerThread();
  StaticDeps().InitializeNetworkThread();
  StaticDeps().InitializeSignalingThread();

// TODO(crbug.com/355256378): OpenH264 for encoding and FFmpeg for H264 decoding
// should be detangled such that software decoding can be enabled without
// software encoding.
#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS) && \
    BUILDFLAG(ENABLE_OPENH264)
  // Building /w |rtc_use_h264|, is the corresponding run-time feature enabled?
  if (!base::FeatureList::IsEnabled(
          blink::features::kWebRtcH264WithOpenH264FFmpeg)) {
    // Feature is to be disabled.
    webrtc::DisableRtcUseH264();
  }
#else
  webrtc::DisableRtcUseH264();
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS) &&
        // BUILDFLAG(ENABLE_OPENH264)

  EnsureWebRtcAudioDeviceImpl();

  // Init SSL, which will be needed by PeerConnection.
  // TODO: https://issues.webrtc.org/issues/339300437 - remove once
  // BoringSSL no longer requires this after
  // https://bugs.chromium.org/p/boringssl/issues/detail?id=35
  if (!rtc::InitializeSSL()) {
    LOG(ERROR) << "Failed on InitializeSSL.";
    NOTREACHED_IN_MIGRATION();
    return;
  }

  base::WaitableEvent create_network_manager_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::unique_ptr<MdnsResponderAdapter> mdns_responder;
#if BUILDFLAG(ENABLE_MDNS)
  if (base::FeatureList::IsEnabled(
          blink::features::kWebRtcHideLocalIpsWithMdns)) {
    // Note that MdnsResponderAdapter is created on the main thread to have
    // access to the connector to the service manager.
    mdns_responder =
        std::make_unique<MdnsResponderAdapter>(*GetSupplementable());
  }
#endif  // BUILDFLAG(ENABLE_MDNS)
  PostCrossThreadTask(
      *GetWebRtcNetworkTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&PeerConnectionDependencyFactory::
                              CreateIpcNetworkManagerOnNetworkThread,
                          WrapCrossThreadPersistent(this),
                          CrossThreadUnretained(&create_network_manager_event),
                          std::move(mdns_responder)));

  create_network_manager_event.Wait();
  CHECK(GetNetworkThread());

  // Wait for the worker thread, since `InitializeSignalingThread` needs to
  // refer to `worker_thread_`.
  worker_thread_started_event.Wait();
  CHECK(GetWorkerThread());

  // Only the JS main thread can establish mojo connection with a browser
  // process against RendererFrameHost. RTCVideoEncoderFactory and
  // RTCVideoEncoders run in the webrtc encoder thread. Therefore, we create
  // MojoVideoEncoderMetricsProviderFactory, establish the mojo connection here
  // and pass it to RTCVideoEncoder. VideoEncoderMetricsProviders created by
  // MojoVideoEncoderMetricsProviderFactory::CreateVideoEncoderMetricsProvider()
  // use the mojo connection. The factory will be destroyed in gpu task runner.
  base::WaitableEvent start_signaling_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  PostCrossThreadTask(
      *GetChromeSignalingThread().task_runner(), FROM_HERE,
      CrossThreadBindOnce(
          &PeerConnectionDependencyFactory::InitializeSignalingThread,
          WrapCrossThreadPersistent(this),
          Platform::Current()->GetRenderingColorSpace(),
          CrossThreadUnretained(Platform::Current()->GetGpuFactories()),
          CreateMojoVideoEncoderMetricsProviderFactory(DomWindow()->GetFrame()),
          CrossThreadUnretained(&start_signaling_event)));

  start_signaling_event.Wait();

  CHECK(pc_factory_);
  CHECK(socket_factory_);
  CHECK(GetSignalingThread());
}

void PeerConnectionDependencyFactory::InitializeSignalingThread(
    const gfx::ColorSpace& render_color_space,
    media::GpuVideoAcceleratorFactories* gpu_factories,
    scoped_refptr<media::MojoVideoEncoderMetricsProviderFactory>
        video_encoder_metrics_provider_factory,
    base::WaitableEvent* event) {
  DCHECK(GetChromeSignalingThread().task_runner()->BelongsToCurrentThread());
  DCHECK(GetNetworkThread());
  // The task to initialize `signaling_thread_` was posted to the same thread,
  // so there is no need to wait on its event.
  DCHECK(GetSignalingThread());
  DCHECK(p2p_socket_dispatcher_);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("webrtc_peer_connection", R"(
        semantics {
          sender: "WebRTC"
          description:
            "WebRTC is an API that provides web applications with Real Time "
            "Communication (RTC) capabilities. It is used to establish a "
            "secure session with a remote peer, transmitting and receiving "
            "audio, video and potentially other data."
          trigger:
            "Application creates an RTCPeerConnection and connects it to a "
            "remote peer by exchanging an SDP offer and answer."
          data:
            "Media encrypted using DTLS-SRTP, and protocol-level messages for "
            "the various subprotocols employed by WebRTC (including ICE, DTLS, "
            "RTCP, etc.). Note that ICE connectivity checks may leak the "
            "user's IP address(es), subject to the restrictions/guidance in "
            "https://datatracker.ietf.org/doc/draft-ietf-rtcweb-ip-handling."
          destination: OTHER
          destination_other:
            "A destination determined by the web application that created the "
            "connection."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but it won't be used "
            "unless the application creates an RTCPeerConnection. Media can "
            "only be captured with user's consent, but data may be sent "
            "withouth that."
          policy_exception_justification:
            "Not implemented. 'WebRtcUdpPortRange' policy can limit the range "
            "of ports used by WebRTC, but there is no policy to generally "
            "block it."
        }
    )");
  // TODO(crbug.com/40265716): remove batch_udp_packets parameter.
  socket_factory_ = std::make_unique<IpcPacketSocketFactory>(
      WTF::CrossThreadBindRepeating(
          &PeerConnectionDependencyFactory::DoGetDevtoolsToken,
          WrapCrossThreadWeakPersistent(this)),
      p2p_socket_dispatcher_.Get(), traffic_annotation, /*batch_udp_packets=*/
      false);

  gpu_factories_ = gpu_factories;
  // WrapCrossThreadWeakPersistent is safe below, because
  // PeerConnectionDependencyFactory (that holds `webrtc_video_perf_reporter_`)
  // outlives the encoders and decoders that are using the callback. The
  // lifetime of PeerConnectionDependencyFactory is tied to the ExecutionContext
  // and the destruction of the encoders and decoders is triggered by a call to
  // RTCPeerConnection::ContextDestroyed() which happens just before the
  // ExecutionContext is destroyed.
  std::unique_ptr<webrtc::VideoEncoderFactory> webrtc_encoder_factory =
      blink::CreateWebrtcVideoEncoderFactory(
          gpu_factories, std::move(video_encoder_metrics_provider_factory),
          base::BindRepeating(&WebrtcVideoPerfReporter::StoreWebrtcVideoStats,
                              WrapCrossThreadWeakPersistent(
                                  webrtc_video_perf_reporter_.Get())));
  std::unique_ptr<webrtc::VideoDecoderFactory> webrtc_decoder_factory =
      blink::CreateWebrtcVideoDecoderFactory(
          gpu_factories, render_color_space,
          base::BindRepeating(&WebrtcVideoPerfReporter::StoreWebrtcVideoStats,
                              WrapCrossThreadWeakPersistent(
                                  webrtc_video_perf_reporter_.Get())));

  if (blink::Platform::Current()->UsesFakeCodecForPeerConnection()) {
    webrtc_encoder_factory =
        std::make_unique<webrtc::FakeVideoEncoderFactory>();
    webrtc_decoder_factory =
        std::make_unique<webrtc::FakeVideoDecoderFactory>();
  }

  webrtc::PeerConnectionFactoryDependencies pcf_deps;
  pcf_deps.worker_thread = GetWorkerThread();
  pcf_deps.signaling_thread = GetSignalingThread();
  pcf_deps.network_thread = GetNetworkThread();
  if (pcf_deps.worker_thread == pcf_deps.network_thread) {
    LOG(INFO) << "Running WebRTC with a combined Network and Worker thread.";
  }
  pcf_deps.task_queue_factory = CreateWebRtcTaskQueueFactory();
  pcf_deps.decode_metronome = StaticDeps().CreateDecodeMetronome();
  pcf_deps.encode_metronome = StaticDeps().MaybeCreateEncodeMetronome();
  pcf_deps.event_log_factory = std::make_unique<webrtc::RtcEventLogFactory>();
  pcf_deps.adm = audio_device_.get();
  pcf_deps.audio_encoder_factory = blink::CreateWebrtcAudioEncoderFactory();
  pcf_deps.audio_decoder_factory = blink::CreateWebrtcAudioDecoderFactory();
  pcf_deps.video_encoder_factory = std::move(webrtc_encoder_factory);
  pcf_deps.video_decoder_factory = std::move(webrtc_decoder_factory);

  // Audio Processing Module (APM) instances are owned and handled by the Blink
  // media stream module.
  DCHECK_EQ(pcf_deps.audio_processing.get(), nullptr);
  webrtc::EnableMedia(pcf_deps);
  pc_factory_ = webrtc::CreateModularPeerConnectionFactory(std::move(pcf_deps));
  CHECK(pc_factory_.get());

  webrtc::PeerConnectionFactoryInterface::Options factory_options;
  factory_options.disable_encryption =
      !blink::Platform::Current()->IsWebRtcEncryptionEnabled();
  pc_factory_->SetOptions(factory_options);

  event->Signal();
}

void PeerConnectionDependencyFactory::DoGetDevtoolsToken(
    base::OnceCallback<void(std::optional<base::UnguessableToken>)> then) {
  context_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      ConvertToBaseOnceCallback(WTF::CrossThreadBindOnce(
          [](PeerConnectionDependencyFactory* factory)
              -> std::optional<base::UnguessableToken> {
            if (!factory) {
              return std::nullopt;
            }
            return factory->GetDevtoolsToken();
          },
          WrapCrossThreadWeakPersistent(this))),
      std::move(then));
}

std::optional<base::UnguessableToken>
PeerConnectionDependencyFactory::GetDevtoolsToken() {
  if (!GetExecutionContext()) {
    return std::nullopt;
  }
  CHECK(GetExecutionContext()->IsContextThread());
  std::optional<base::UnguessableToken> devtools_token;
  probe::WillCreateP2PSocketUdp(GetExecutionContext(), &devtools_token);
  return devtools_token;
}

bool PeerConnectionDependencyFactory::PeerConnectionFactoryCreated() {
  return !!pc_factory_;
}

rtc::scoped_refptr<webrtc::PeerConnectionInterface>
PeerConnectionDependencyFactory::CreatePeerConnection(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    blink::WebLocalFrame* web_frame,
    webrtc::PeerConnectionObserver* observer,
    ExceptionState& exception_state,
    RTCRtpTransport* rtp_transport) {
  CHECK(observer);
  if (!GetPcFactory().get())
    return nullptr;

  webrtc::PeerConnectionDependencies dependencies(observer);
  // |web_frame| may be null in tests, e.g. if
  // RTCPeerConnectionHandler::InitializeForTest() is used.
  if (web_frame) {
    dependencies.allocator = CreatePortAllocator(web_frame);
  }
  dependencies.async_dns_resolver_factory = CreateAsyncDnsResolverFactory();
  if (rtp_transport) {
    dependencies.network_controller_factory =
        std::make_unique<InterceptingNetworkControllerFactory>(
            context_task_runner_, rtp_transport);
  }
  auto pc_or_error = GetPcFactory()->CreatePeerConnectionOrError(
      config, std::move(dependencies));
  if (pc_or_error.ok()) {
    return pc_or_error.value();
  } else {
    // Convert error
    ThrowExceptionFromRTCError(pc_or_error.error(), exception_state);
    return nullptr;
  }
}

std::unique_ptr<cricket::PortAllocator>
PeerConnectionDependencyFactory::CreatePortAllocator(
    blink::WebLocalFrame* web_frame) {
  DCHECK(web_frame);
  EnsureInitialized();

  // Copy the flag from Preference associated with this WebLocalFrame.
  P2PPortAllocator::Config port_config;
  uint16_t min_port = 0;
  uint16_t max_port = 0;
  bool allow_mdns_obfuscation = true;

  // |media_permission| will be called to check mic/camera permission. If at
  // least one of them is granted, P2PPortAllocator is allowed to gather local
  // host IP addresses as ICE candidates. |media_permission| could be nullptr,
  // which means the permission will be granted automatically. This could be the
  // case when either the experiment is not enabled or the preference is not
  // enforced.
  //
  // Note on |media_permission| lifetime: |media_permission| is owned by a frame
  // (RenderFrameImpl). It is also stored as an indirect member of
  // RTCPeerConnectionHandler (through PeerConnection/PeerConnectionInterface ->
  // P2PPortAllocator -> FilteringNetworkManager -> |media_permission|).
  // The RTCPeerConnectionHandler is owned as RTCPeerConnection::m_peerHandler
  // in Blink, which will be reset in RTCPeerConnection::stop(). Since
  // ActiveDOMObject::stop() is guaranteed to be called before a frame is
  // detached, it is impossible for RTCPeerConnectionHandler to outlive the
  // frame. Therefore using a raw pointer of |media_permission| is safe here.
  media::MediaPermission* media_permission = nullptr;
  if (!Platform::Current()->ShouldEnforceWebRTCRoutingPreferences()) {
    port_config.enable_multiple_routes = true;
    port_config.enable_nonproxied_udp = true;
    VLOG(3) << "WebRTC routing preferences will not be enforced";
  } else {
    if (web_frame && web_frame->View()) {
      WebString webrtc_ip_handling_policy;
      Platform::Current()->GetWebRTCRendererPreferences(
          web_frame, &webrtc_ip_handling_policy, &min_port, &max_port,
          &allow_mdns_obfuscation);

      // TODO(guoweis): |enable_multiple_routes| should be renamed to
      // |request_multiple_routes|. Whether local IP addresses could be
      // collected depends on if mic/camera permission is granted for this
      // origin.
      WebRTCIPHandlingPolicy policy =
          GetWebRTCIPHandlingPolicy(webrtc_ip_handling_policy);
      switch (policy) {
        // TODO(guoweis): specify the flag of disabling local candidate
        // collection when webrtc is updated.
        case kDefaultPublicInterfaceOnly:
        case kDefaultPublicAndPrivateInterfaces:
          port_config.enable_multiple_routes = false;
          port_config.enable_nonproxied_udp = true;
          port_config.enable_default_local_candidate =
              (policy == kDefaultPublicAndPrivateInterfaces);
          break;
        case kDisableNonProxiedUdp:
          port_config.enable_multiple_routes = false;
          port_config.enable_nonproxied_udp = false;
          break;
        case kDefault:
          port_config.enable_multiple_routes = true;
          port_config.enable_nonproxied_udp = true;
          break;
      }

      VLOG(3) << "WebRTC routing preferences: "
              << "policy: " << policy
              << ", multiple_routes: " << port_config.enable_multiple_routes
              << ", nonproxied_udp: " << port_config.enable_nonproxied_udp
              << ", min_udp_port: " << min_port
              << ", max_udp_port: " << max_port
              << ", allow_mdns_obfuscation: " << allow_mdns_obfuscation;
    }
    if (port_config.enable_multiple_routes) {
      media_permission =
          blink::Platform::Current()->GetWebRTCMediaPermission(web_frame);
    }
  }

  std::unique_ptr<rtc::NetworkManager> network_manager;
  if (port_config.enable_multiple_routes) {
    network_manager = std::make_unique<FilteringNetworkManager>(
        network_manager_.get(), media_permission, allow_mdns_obfuscation);
  } else {
    network_manager =
        std::make_unique<blink::EmptyNetworkManager>(network_manager_.get());
  }
  auto port_allocator = std::make_unique<P2PPortAllocator>(
      std::move(network_manager), socket_factory_.get(), port_config);
  if (IsValidPortRange(min_port, max_port))
    port_allocator->SetPortRange(min_port, max_port);

  return port_allocator;
}

std::unique_ptr<webrtc::AsyncDnsResolverFactoryInterface>
PeerConnectionDependencyFactory::CreateAsyncDnsResolverFactory() {
  EnsureInitialized();
  return std::make_unique<ProxyAsyncDnsResolverFactory>(socket_factory_.get());
}

scoped_refptr<webrtc::MediaStreamInterface>
PeerConnectionDependencyFactory::CreateLocalMediaStream(const String& label) {
  return GetPcFactory()->CreateLocalMediaStream(label.Utf8()).get();
}

scoped_refptr<webrtc::VideoTrackSourceInterface>
PeerConnectionDependencyFactory::CreateVideoTrackSourceProxy(
    webrtc::VideoTrackSourceInterface* source) {
  // PeerConnectionFactory needs to be instantiated to make sure that
  // signaling_thread_ and network_thread_ exist.
  if (!PeerConnectionFactoryCreated())
    CreatePeerConnectionFactory();

  return webrtc::CreateVideoTrackSourceProxy(GetSignalingThread(),
                                             GetNetworkThread(), source)
      .get();
}

scoped_refptr<webrtc::VideoTrackInterface>
PeerConnectionDependencyFactory::CreateLocalVideoTrack(
    const String& id,
    webrtc::VideoTrackSourceInterface* source) {
  return GetPcFactory()
      ->CreateVideoTrack(
          rtc::scoped_refptr<webrtc::VideoTrackSourceInterface>(source),
          id.Utf8())
      .get();
}

webrtc::IceCandidateInterface*
PeerConnectionDependencyFactory::CreateIceCandidate(const String& sdp_mid,
                                                    int sdp_mline_index,
                                                    const String& sdp) {
  return webrtc::CreateIceCandidate(sdp_mid.Utf8(), sdp_mline_index, sdp.Utf8(),
                                    nullptr);
}

blink::WebRtcAudioDeviceImpl*
PeerConnectionDependencyFactory::GetWebRtcAudioDevice() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  EnsureWebRtcAudioDeviceImpl();
  return audio_device_.get();
}

void PeerConnectionDependencyFactory::CreateIpcNetworkManagerOnNetworkThread(
    base::WaitableEvent* event,
    std::unique_ptr<MdnsResponderAdapter> mdns_responder) {
  DCHECK(GetChromeNetworkThread().task_runner()->BelongsToCurrentThread());
  // The task to initialize `network_thread_` was posted to the same thread, so
  // there is no need to wait on its event.
  DCHECK(GetNetworkThread());

  network_manager_ = std::make_unique<blink::IpcNetworkManager>(
      p2p_socket_dispatcher_.Get(), std::move(mdns_responder));

  event->Signal();
}

void PeerConnectionDependencyFactory::DeleteIpcNetworkManager(
    std::unique_ptr<IpcNetworkManager> network_manager,
    base::WaitableEvent* event) {
  DCHECK(GetChromeNetworkThread().task_runner()->BelongsToCurrentThread());
  network_manager = nullptr;
  event->Signal();
}

void PeerConnectionDependencyFactory::ContextDestroyed() {
  if (network_manager_) {
    PostCrossThreadTask(
        *GetWebRtcNetworkTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&IpcNetworkManager::ContextDestroyed,
                            CrossThreadUnretained(network_manager_.get())));
  }
}

void PeerConnectionDependencyFactory::CleanupPeerConnectionFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << "PeerConnectionDependencyFactory::CleanupPeerConnectionFactory()";
  socket_factory_ = nullptr;
  // Not obtaining `signaling_thread` using GetWebRtcSignalingTaskRunner()
  // because that method triggers EnsureInitialized() and we're trying to
  // perform cleanup.
  scoped_refptr<base::SingleThreadTaskRunner> signaling_thread =
      GetChromeSignalingThread().IsRunning()
          ? GetChromeSignalingThread().task_runner()
          : nullptr;
  if (signaling_thread) {
    // To avoid a PROXY block-invoke to ~webrtc::PeerConnectionFactory(), we
    // move our reference to the signaling thread in a PostTask.
    signaling_thread->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pcf) {
              // The binding releases `pcf` on the signaling thread as this
              // method goes out of scope.
            },
            std::move(pc_factory_)));
  } else {
    pc_factory_ = nullptr;
  }
  DCHECK(!pc_factory_);
  if (network_manager_) {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    // The network manager needs to free its resources on the thread they were
    // created, which is the network thread.
    PostCrossThreadTask(
        *GetWebRtcNetworkTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(
            &PeerConnectionDependencyFactory::DeleteIpcNetworkManager,
            std::move(network_manager_), CrossThreadUnretained(&event)));
    network_manager_ = nullptr;
    event.Wait();
  }
}

void PeerConnectionDependencyFactory::EnsureInitialized() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetPcFactory();
}

scoped_refptr<base::SingleThreadTaskRunner>
PeerConnectionDependencyFactory::GetWebRtcNetworkTaskRunner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetChromeNetworkThread().IsRunning()
             ? GetChromeNetworkThread().task_runner()
             : nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
PeerConnectionDependencyFactory::GetWebRtcWorkerTaskRunner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetChromeWorkerThread().IsRunning()
             ? GetChromeWorkerThread().task_runner()
             : nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
PeerConnectionDependencyFactory::GetWebRtcSignalingTaskRunner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  EnsureInitialized();
  return GetChromeSignalingThread().IsRunning()
             ? GetChromeSignalingThread().task_runner()
             : nullptr;
}

void PeerConnectionDependencyFactory::EnsureWebRtcAudioDeviceImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (audio_device_.get())
    return;

  audio_device_ = new rtc::RefCountedObject<blink::WebRtcAudioDeviceImpl>();
}

std::unique_ptr<webrtc::RtpCapabilities>
PeerConnectionDependencyFactory::GetSenderCapabilities(const String& kind) {
  if (kind == "audio") {
    return std::make_unique<webrtc::RtpCapabilities>(
        GetPcFactory()->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_AUDIO));
  } else if (kind == "video") {
    return std::make_unique<webrtc::RtpCapabilities>(
        GetPcFactory()->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_VIDEO));
  }
  return nullptr;
}

std::unique_ptr<webrtc::RtpCapabilities>
PeerConnectionDependencyFactory::GetReceiverCapabilities(const String& kind) {
  if (kind == "audio") {
    return std::make_unique<webrtc::RtpCapabilities>(
        GetPcFactory()->GetRtpReceiverCapabilities(cricket::MEDIA_TYPE_AUDIO));
  } else if (kind == "video") {
    return std::make_unique<webrtc::RtpCapabilities>(
        GetPcFactory()->GetRtpReceiverCapabilities(cricket::MEDIA_TYPE_VIDEO));
  }
  return nullptr;
}

media::GpuVideoAcceleratorFactories*
PeerConnectionDependencyFactory::GetGpuFactories() {
  return gpu_factories_;
}

void PeerConnectionDependencyFactory::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(p2p_socket_dispatcher_);
  visitor->Trace(webrtc_video_perf_reporter_);
}

std::unique_ptr<webrtc::Metronome>
PeerConnectionDependencyFactory::CreateDecodeMetronome() {
  return StaticDeps().CreateDecodeMetronome();
}
}  // namespace blink
