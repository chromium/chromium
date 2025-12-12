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
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
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
#include "media/webrtc/webrtc_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/net_buildflags.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"
#include "third_party/blink/public/mojom/peerconnection/webrtc_ip_handling_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink-forward.h"
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
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
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
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/bind_post_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_gfx.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/webrtc/api/create_modular_peer_connection_factory.h"
#include "third_party/webrtc/api/enable_media.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/rtc_event_log/rtc_event_log_factory.h"
#include "third_party/webrtc/api/transport/goog_cc_factory.h"
#include "third_party/webrtc/api/video_track_source_proxy_factory.h"
#include "third_party/webrtc/media/engine/fake_video_codec_factory.h"
#include "third_party/webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"
#include "third_party/webrtc/rtc_base/ssl_adapter.h"
#include "third_party/webrtc_overrides/environment.h"
#include "third_party/webrtc_overrides/metronome_source.h"
#include "third_party/webrtc_overrides/timer_based_tick_provider.h"

namespace blink {

namespace {

using PassKey = base::PassKey<PeerConnectionDependencyFactory>;

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

network::mojom::IPAddressSpace FromSocketAddress(
    const webrtc::SocketAddress socket_address) {
  switch (socket_address.GetIPAddressType()) {
    case webrtc::IPAddressType::kAny:
      return network::mojom::IPAddressSpace::kPublic;
    case webrtc::IPAddressType::kLoopback:
      return network::mojom::IPAddressSpace::kLoopback;
    case webrtc::IPAddressType::kPrivate:
      return network::mojom::IPAddressSpace::kLocal;
    case webrtc::IPAddressType::kPublic:
      return network::mojom::IPAddressSpace::kPublic;
    case webrtc::IPAddressType::kUnknown:
      return network::mojom::IPAddressSpace::kUnknown;
  }
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
      const webrtc::SocketAddress& addr,
      absl::AnyInvocable<void()> callback) override {
    auto temp = Create();
    temp->Start(addr, std::move(callback));
    return temp;
  }
  std::unique_ptr<webrtc::AsyncDnsResolverInterface> CreateAndResolve(
      const webrtc::SocketAddress& addr,
      int family,
      absl::AnyInvocable<void()> callback) override {
    auto temp = Create();
    temp->Start(addr, family, std::move(callback));
    return temp;
  }

 private:
  raw_ptr<IpcPacketSocketFactory, DanglingUntriaged> ipc_psf_;
};

LocalNetworkAccessRequestType GetLocalNetworkAccessRequestType(
    network::mojom::IPAddressSpace originator,
    network::mojom::IPAddressSpace target) {
  if (originator == network::mojom::IPAddressSpace::kUnknown ||
      target == network::mojom::IPAddressSpace::kUnknown) {
    return LocalNetworkAccessRequestType::kUnknown;
  }

  switch (originator) {
    case network::mojom::IPAddressSpace::kLoopback:
      switch (target) {
        case network::mojom::IPAddressSpace::kLoopback:
          return LocalNetworkAccessRequestType::kLoopbackToLoopback;
        case network::mojom::IPAddressSpace::kLocal:
          return LocalNetworkAccessRequestType::kLoopbackToLocal;
        case network::mojom::IPAddressSpace::kPublic:
          return LocalNetworkAccessRequestType::kLoopbackToPublic;
        case network::mojom::IPAddressSpace::kUnknown:
          NOTREACHED();
      }
      break;
    case network::mojom::IPAddressSpace::kLocal:
      switch (target) {
        case network::mojom::IPAddressSpace::kLoopback:
          return LocalNetworkAccessRequestType::kLocalToLoopback;
        case network::mojom::IPAddressSpace::kLocal:
          return LocalNetworkAccessRequestType::kLocalToLocal;
        case network::mojom::IPAddressSpace::kPublic:
          return LocalNetworkAccessRequestType::kLocalToPublic;
        case network::mojom::IPAddressSpace::kUnknown:
          NOTREACHED();
      }
      break;
    case network::mojom::IPAddressSpace::kPublic:
      switch (target) {
        case network::mojom::IPAddressSpace::kLoopback:
          return LocalNetworkAccessRequestType::kPublicToLoopback;
        case network::mojom::IPAddressSpace::kLocal:
          return LocalNetworkAccessRequestType::kPublicToLocal;
        case network::mojom::IPAddressSpace::kPublic:
          return LocalNetworkAccessRequestType::kPublicToPublic;
        case network::mojom::IPAddressSpace::kUnknown:
          NOTREACHED();
      }
      break;
    case network::mojom::IPAddressSpace::kUnknown:
      NOTREACHED();
  }
  NOTREACHED();
}

class LocalNetworkAccessPermission final
    : public webrtc::LocalNetworkAccessPermissionInterface {
 public:
  explicit LocalNetworkAccessPermission(
      network::mojom::IPAddressSpace originator_address_space,
      mojo::Remote<mojom::blink::PermissionService> permission_service,
      blink::CrossThreadRepeatingFunction<void(LocalNetworkAccessRequestType)>
          count_callback)
      : originator_address_space_(originator_address_space),
        permission_service_(std::move(permission_service)),
        count_callback_(std::move(count_callback)) {}

  ~LocalNetworkAccessPermission() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

  bool ShouldRequestPermission(
      const webrtc::SocketAddress& candidate_address) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    const auto target_address_space = FromSocketAddress(candidate_address);
    auto request_type = GetLocalNetworkAccessRequestType(
        originator_address_space_, target_address_space);
    base::UmaHistogramEnumeration(
        "WebRTC.PeerConnection.LocalNetworkAccess.RequestType", request_type);
    count_callback_.Run(request_type);

    if (!RuntimeEnabledFeatures::LocalNetworkAccessWebRTCEnabled()) {
      return false;
    }

    const bool is_less_public = network::IsLessPublicAddressSpace(
        target_address_space, originator_address_space_);

    if (network::features::kLocalNetworkAccessChecksWebRTCLoopbackOnly.Get()) {
      return candidate_address.IsLoopbackIP() && is_less_public;
    }

    return is_less_public;
  }

  void RequestPermission(
      const webrtc::SocketAddress& candidate_address,
      absl::AnyInvocable<void(webrtc::LocalNetworkAccessPermissionStatus)>
          callback) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    CHECK(RuntimeEnabledFeatures::LocalNetworkAccessWebRTCEnabled());

    callback_ = std::move(callback);
    permission_service_->RequestPermission(
        CreatePermissionDescriptor(
            mojom::blink::PermissionName::LOCAL_NETWORK_ACCESS),
        /*user_gesture=*/false,
        BindRepeating(
            &LocalNetworkAccessPermission::OnPermissionRequested,
            // This is safe because this class owns `permission_service_` which
            // won't call its callback after its destroyed.
            base::Unretained(this)));
  }

  void OnPermissionRequested(mojom::blink::PermissionStatus status) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    switch (status) {
      case mojom::blink::PermissionStatus::GRANTED:
        callback_(webrtc::LocalNetworkAccessPermissionStatus::kGranted);
        break;
      case mojom::blink::PermissionStatus::ASK:
      // Treat ASK i.e. the user closing the prompt, as denied.
      case mojom::blink::PermissionStatus::DENIED:
        callback_(webrtc::LocalNetworkAccessPermissionStatus::kDenied);
        break;
    }
  }

  absl::AnyInvocable<void(webrtc::LocalNetworkAccessPermissionStatus)>
      callback_;
  const network::mojom::IPAddressSpace originator_address_space_;
  mojo::Remote<mojom::blink::PermissionService> permission_service_;
  blink::CrossThreadRepeatingFunction<void(LocalNetworkAccessRequestType)>
      count_callback_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<LocalNetworkAccessPermission> weak_ptr_factory_{this};
};

class LocalNetworkAccessPermissionFactory final
    : public webrtc::LocalNetworkAccessPermissionFactoryInterface {
 public:
  explicit LocalNetworkAccessPermissionFactory(
      PeerConnectionDependencyFactory* factory)
      : factory_(factory),
        originator_address_space_(factory->DomWindow()
                                      ->GetPolicyContainer()
                                      ->GetPolicies()
                                      .ip_address_space),
        main_thread_task_runner_(
            factory->DomWindow()->GetTaskRunner(TaskType::kNetworking)) {}

  std::unique_ptr<webrtc::LocalNetworkAccessPermissionInterface> Create()
      override {
    mojo::Remote<mojom::blink::PermissionService> permission_service;
    PostCrossThreadTask(
        *main_thread_task_runner_.get(), FROM_HERE,
        CrossThreadBindOnce(
            &PeerConnectionDependencyFactory::BindPermissionService,
            MakeUnwrappingCrossThreadWeakHandle(factory_),
            permission_service.BindNewPipeAndPassReceiver()));

    return std::make_unique<LocalNetworkAccessPermission>(
        originator_address_space_, std::move(permission_service),
        BindPostTask(
            main_thread_task_runner_,
            CrossThreadBindRepeating(
                &PeerConnectionDependencyFactory::CountLocalNetworkAccess,
                MakeUnwrappingCrossThreadWeakHandle(factory_))));
  }

 private:
  // Use a WeakHandle to allow the factory to be garbage collected.
  //
  // Binding BindPermissionService with a CrossThreadHandle, would cause
  // PeerConnectionDependencyFactory to not be garbage-collected and leak, so
  // we use a CrossThreadWeakHandle instead.
  CrossThreadWeakHandle<PeerConnectionDependencyFactory> factory_;
  network::mojom::IPAddressSpace originator_address_space_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
};

// Encapsulates process-wide static dependencies used by
// `PeerConnectionDependencyFactory`, namely the threads used by WebRTC. This
// avoids allocating multiple threads per factory instance, as they are
// "heavy-weight" and we don't want to create them per frame.
class PeerConnectionStaticDeps {
 public:
  PeerConnectionStaticDeps()
      : chrome_signaling_thread_("WebRTC_Signaling"),
        chrome_worker_thread_("WebRTC_W_and_N") {}

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
    if (!chrome_signaling_thread_.IsRunning()) {
      chrome_signaling_thread_.StartWithOptions(
          base::Thread::Options(base::ThreadType::kDefault));
    }

    if (!chrome_worker_thread_.IsRunning()) {
      chrome_worker_thread_.StartWithOptions(
          base::Thread::Options(base::ThreadType::kDefault));
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

  webrtc::Thread* GetSignalingThread() { return signaling_thread_; }
  webrtc::Thread* GetWorkerThread() { return worker_thread_; }
  webrtc::Thread* GetNetworkThread() { return worker_thread_; }
  base::Thread& GetChromeSignalingThread() { return chrome_signaling_thread_; }
  base::Thread& GetChromeWorkerThread() { return chrome_worker_thread_; }
  base::Thread& GetChromeNetworkThread() { return chrome_worker_thread_; }

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
      raw_ptr<webrtc::Thread>* thread,
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
  raw_ptr<webrtc::Thread> signaling_thread_ = nullptr;
  raw_ptr<webrtc::Thread> worker_thread_ = nullptr;
  base::Thread chrome_signaling_thread_;
  base::Thread chrome_worker_thread_;

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

webrtc::Thread* GetSignalingThread() {
  return StaticDeps().GetSignalingThread();
}
webrtc::Thread* GetWorkerThread() {
  return StaticDeps().GetWorkerThread();
}
webrtc::Thread* GetNetworkThread() {
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

// The enum is used for logging. Entries should not be renumbered or reused.
// Keep in sync with the corresponding enum in
// tools/metrics/histograms/metadata/web_rtc/enums.xml.
enum class EncodeScalabilityMode {
  NotSupported = 0,
  kL1T1 = 1,
  kL1T2 = 2,
  kL1T3 = 3,
  kMaxValue = kL1T3,
};

struct ScalabilityMap {
  std::string scalability_string;
  EncodeScalabilityMode scalability_enum;
};

void ReportUmaEncodeDecodeCapabilities(
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  const gfx::ColorSpace& render_color_space =
      Platform::Current()->GetRenderingColorSpace();

  // Create encoder/decoder factories.
  std::unique_ptr<webrtc::VideoEncoderFactory> webrtc_encoder_factory =
      blink::CreateWebrtcVideoEncoderFactoryForUmaLogging(gpu_factories);
  std::unique_ptr<webrtc::VideoDecoderFactory> webrtc_decoder_factory =
      blink::CreateWebrtcVideoDecoderFactoryForUmaLogging(gpu_factories,
                                                          render_color_space);
  if (webrtc_encoder_factory && webrtc_decoder_factory) {
    const std::array<webrtc::SdpVideoFormat, 3> kSdpFormats = {
        webrtc::SdpVideoFormat{"VP9"}, webrtc::SdpVideoFormat{"AV1"},
        webrtc::SdpVideoFormat{"H265"}};
    const std::array<ScalabilityMap, 3> kScalabilityModes = {
        ScalabilityMap{"L1T1", EncodeScalabilityMode::kL1T1},
        ScalabilityMap{"L1T2", EncodeScalabilityMode::kL1T2},
        ScalabilityMap{"L1T3", EncodeScalabilityMode::kL1T3}};

    for (const auto& sdp_format : kSdpFormats) {
      bool decode_support =
          webrtc_decoder_factory
              ->QueryCodecSupport(sdp_format, /*reference_scaling=*/false)
              .is_power_efficient;

      EncodeScalabilityMode encode_support =
          EncodeScalabilityMode::NotSupported;
      for (const auto& mode : kScalabilityModes) {
        if (webrtc_encoder_factory
                ->QueryCodecSupport(sdp_format, mode.scalability_string)
                .is_power_efficient) {
          encode_support = mode.scalability_enum;
        } else {
          break;
        }
      }

      base::UmaHistogramBoolean(
          "WebRTC.Video.HwCapabilities.Decode." + sdp_format.name,
          decode_support);
      base::UmaHistogramEnumeration(
          "WebRTC.Video.HwCapabilities.Encode." + sdp_format.name,
          encode_support);
    }
  }
}

void WaitForEncoderSupportReady(
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  gpu_factories->NotifyEncoderSupportKnown(
      BindOnce(&ReportUmaEncodeDecodeCapabilities, Unretained(gpu_factories)));
}

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

const webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>&
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
  StaticDeps().InitializeSignalingThread();

  if (!::features::IsOpenH264SoftwareEncoderEnabledForWebRTC()) {
    // Feature is to be disabled.
    webrtc::DisableRtcUseH264();
  }

  EnsureWebRtcAudioDeviceImpl();

  // Init SSL, which will be needed by PeerConnection.
  // TODO: https://issues.webrtc.org/issues/339300437 - remove once
  // BoringSSL no longer requires this after
  // https://bugs.chromium.org/p/boringssl/issues/detail?id=35
  if (!webrtc::InitializeSSL()) {
    NOTREACHED() << "Failed on InitializeSSL.";
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
      CrossThreadBindRepeating(
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
          ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
              &WebrtcVideoPerfReporter::StoreWebrtcVideoStats,
              WrapCrossThreadWeakPersistent(
                  webrtc_video_perf_reporter_.Get()))));
  std::unique_ptr<webrtc::VideoDecoderFactory> webrtc_decoder_factory =
      blink::CreateWebrtcVideoDecoderFactory(
          gpu_factories, render_color_space,
          ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
              &WebrtcVideoPerfReporter::StoreWebrtcVideoStats,
              WrapCrossThreadWeakPersistent(
                  webrtc_video_perf_reporter_.Get()))));

  if (!encode_decode_capabilities_reported_) {
    encode_decode_capabilities_reported_ = true;
    if (gpu_factories) {
      // Wait until decoder and encoder support are known.
      gpu_factories->NotifyDecoderSupportKnown(
          BindOnce(&WaitForEncoderSupportReady, Unretained(gpu_factories)));
    } else {
      ReportUmaEncodeDecodeCapabilities(gpu_factories);
    }
  }

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
  pcf_deps.env = WebRtcEnvironment();
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
  DCHECK_EQ(pcf_deps.audio_processing_builder, nullptr);
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
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
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

webrtc::scoped_refptr<webrtc::PeerConnectionInterface>
PeerConnectionDependencyFactory::CreatePeerConnection(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    blink::WebLocalFrame* web_frame,
    webrtc::PeerConnectionObserver* observer,
    ExceptionState& exception_state) {
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
  dependencies.lna_permission_factory =
      std::make_unique<LocalNetworkAccessPermissionFactory>(this);
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

std::unique_ptr<webrtc::PortAllocator>
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
      mojom::blink::WebRtcIpHandlingPolicy webrtc_ip_handling_policy;
      Platform::Current()->GetWebRTCRendererPreferences(
          web_frame, &webrtc_ip_handling_policy, &min_port, &max_port,
          &allow_mdns_obfuscation);
      DVLOG(1) << "Active WebRtcIPHandlingPolicy: "
               << ToString(webrtc_ip_handling_policy);
      // TODO(guoweis): |enable_multiple_routes| should be renamed to
      // |request_multiple_routes|. Whether local IP addresses could be
      // collected depends on if mic/camera permission is granted for this
      // origin.
      switch (webrtc_ip_handling_policy) {
        // TODO(guoweis): specify the flag of disabling local candidate
        // collection when webrtc is updated.
        case mojom::blink::WebRtcIpHandlingPolicy::kDefaultPublicInterfaceOnly:
        case mojom::blink::WebRtcIpHandlingPolicy::
            kDefaultPublicAndPrivateInterfaces:
          port_config.enable_multiple_routes = false;
          port_config.enable_nonproxied_udp = true;
          port_config.enable_default_local_candidate =
              (webrtc_ip_handling_policy ==
               mojom::blink::WebRtcIpHandlingPolicy::
                   kDefaultPublicAndPrivateInterfaces);
          break;
        case mojom::blink::WebRtcIpHandlingPolicy::kDisableNonProxiedUdp:
          port_config.enable_multiple_routes = false;
          port_config.enable_nonproxied_udp = false;
          break;
        case mojom::blink::WebRtcIpHandlingPolicy::kDefault:
          port_config.enable_multiple_routes = true;
          port_config.enable_nonproxied_udp = true;
          break;
      }

      VLOG(3) << "WebRTC routing preferences: " << "policy: "
              << ToString(webrtc_ip_handling_policy)
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

  std::unique_ptr<webrtc::NetworkManager> network_manager;
  if (port_config.enable_multiple_routes) {
    network_manager = std::make_unique<FilteringNetworkManager>(
        network_manager_.get(), media_permission, allow_mdns_obfuscation);
  } else {
    network_manager =
        std::make_unique<blink::EmptyNetworkManager>(network_manager_.get());
  }

  auto port_allocator = std::make_unique<P2PPortAllocator>(
      std::move(network_manager), socket_factory_.get(), port_config,
      std::make_unique<LocalNetworkAccessPermissionFactory>(this));
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
  // signaling_thread_ exist.
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
          webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface>(source),
          id.Utf8())
      .get();
}

webrtc::IceCandidate* PeerConnectionDependencyFactory::CreateIceCandidate(
    const String& sdp_mid,
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
    PostCrossThreadTask(
        *signaling_thread, FROM_HERE,
        CrossThreadBindOnce(
            [](webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
                   pcf) {
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

  audio_device_ = new webrtc::RefCountedObject<blink::WebRtcAudioDeviceImpl>();
}

std::unique_ptr<webrtc::RtpCapabilities>
PeerConnectionDependencyFactory::GetSenderCapabilities(const String& kind) {
  if (kind == "audio") {
    return std::make_unique<webrtc::RtpCapabilities>(
        GetPcFactory()->GetRtpSenderCapabilities(webrtc::MediaType::AUDIO));
  } else if (kind == "video") {
    return std::make_unique<webrtc::RtpCapabilities>(
        GetPcFactory()->GetRtpSenderCapabilities(webrtc::MediaType::VIDEO));
  }
  return nullptr;
}

std::unique_ptr<webrtc::RtpCapabilities>
PeerConnectionDependencyFactory::GetReceiverCapabilities(const String& kind) {
  if (kind == "audio") {
    return std::make_unique<webrtc::RtpCapabilities>(
        GetPcFactory()->GetRtpReceiverCapabilities(webrtc::MediaType::AUDIO));
  } else if (kind == "video") {
    return std::make_unique<webrtc::RtpCapabilities>(
        GetPcFactory()->GetRtpReceiverCapabilities(webrtc::MediaType::VIDEO));
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

void PeerConnectionDependencyFactory::BindPermissionService(
    mojo::PendingReceiver<mojom::blink::PermissionService> permission_service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* execution_context = GetExecutionContext();
  if (!execution_context) {
    return;
  }

  execution_context->GetBrowserInterfaceBroker().GetInterface(
      std::move(permission_service));
}

std::unique_ptr<webrtc::LocalNetworkAccessPermissionFactoryInterface>
PeerConnectionDependencyFactory::
    CreateLocalNetworkAccessPermissionFactoryForTesting() {
  return std::make_unique<LocalNetworkAccessPermissionFactory>(this);
}

void PeerConnectionDependencyFactory::CountLocalNetworkAccess(
    LocalNetworkAccessRequestType request_type) {
  UseCounter::Count(DomWindow(),
                    mojom::blink::WebFeature::kWebRTCLocalNetworkAccessCheck);

  switch (request_type) {
    // To same or more public cases:
    case LocalNetworkAccessRequestType::kUnknown:
    case LocalNetworkAccessRequestType::kPublicToPublic:
    case LocalNetworkAccessRequestType::kLocalToPublic:
    case LocalNetworkAccessRequestType::kLocalToLocal:
    case LocalNetworkAccessRequestType::kLoopbackToPublic:
    case LocalNetworkAccessRequestType::kLoopbackToLocal:
    case LocalNetworkAccessRequestType::kLoopbackToLoopback:
      return;
    // To less public cases:
    case LocalNetworkAccessRequestType::kPublicToLocal:
      UseCounter::Count(
          DomWindow(),
          mojom::blink::WebFeature::kWebRTCLocalNetworkAccessPublicToLocal);
      return;
    case LocalNetworkAccessRequestType::kPublicToLoopback:
      UseCounter::Count(
          DomWindow(),
          mojom::blink::WebFeature::kWebRTCLocalNetworkAccessPublicToLoopback);
      return;
    case LocalNetworkAccessRequestType::kLocalToLoopback:
      UseCounter::Count(
          DomWindow(),
          mojom::blink::WebFeature::kWebRTCLocalNetworkAccessLocalToLoopback);
      return;
  }
}

}  // namespace blink
