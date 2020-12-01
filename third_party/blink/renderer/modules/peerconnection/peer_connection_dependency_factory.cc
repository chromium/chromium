// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "crypto/openssl_util.h"
#include "jingle/glue/thread_wrapper.h"
#include "media/base/media_permission.h"
#include "media/media_buildflags.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "net/net_buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_constraints.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/p2p/empty_network_manager.h"
#include "third_party/blink/renderer/platform/p2p/filtering_network_manager.h"
#include "third_party/blink/renderer/platform/p2p/ipc_network_manager.h"
#include "third_party/blink/renderer/platform/p2p/ipc_socket_factory.h"
#include "third_party/blink/renderer/platform/p2p/mdns_responder_adapter.h"
#include "third_party/blink/renderer/platform/p2p/port_allocator.h"
#include "third_party/blink/renderer/platform/p2p/socket_dispatcher.h"
#include "third_party/blink/renderer/platform/peerconnection/audio_codec_factory.h"
#include "third_party/blink/renderer/platform/peerconnection/stun_field_trial.h"
#include "third_party/blink/renderer/platform/peerconnection/video_codec_factory.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/call/call_factory_interface.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/rtc_event_log/rtc_event_log_factory.h"
#include "third_party/webrtc/api/video_track_source_proxy.h"
#include "third_party/webrtc/media/engine/fake_video_codec_factory.h"
#include "third_party/webrtc/media/engine/multiplex_codec_factory.h"
#include "third_party/webrtc/media/engine/webrtc_media_engine.h"
#include "third_party/webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "third_party/webrtc/rtc_base/openssl_stream_adapter.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"
#include "third_party/webrtc/rtc_base/ssl_adapter.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

namespace blink {

namespace {

enum WebRTCIPHandlingPolicy {
  DEFAULT,
  DEFAULT_PUBLIC_AND_PRIVATE_INTERFACES,
  DEFAULT_PUBLIC_INTERFACE_ONLY,
  DISABLE_NON_PROXIED_UDP,
};

WebRTCIPHandlingPolicy GetWebRTCIPHandlingPolicy(const String& preference) {
  if (preference == kWebRTCIPHandlingDefaultPublicAndPrivateInterfaces)
    return DEFAULT_PUBLIC_AND_PRIVATE_INTERFACES;
  if (preference == kWebRTCIPHandlingDefaultPublicInterfaceOnly)
    return DEFAULT_PUBLIC_INTERFACE_ONLY;
  if (preference == kWebRTCIPHandlingDisableNonProxiedUdp)
    return DISABLE_NON_PROXIED_UDP;
  return DEFAULT;
}

bool IsValidPortRange(uint16_t min_port, uint16_t max_port) {
  DCHECK(min_port <= max_port);
  return min_port != 0 && max_port != 0;
}

// PeerConnectionDependencies wants to own the factory, so we provide a simple
// object that delegates calls to the IpcPacketSocketFactory.
// TODO(zstein): Move the creation logic from IpcPacketSocketFactory in to this
// class.
class ProxyAsyncResolverFactory final : public webrtc::AsyncResolverFactory {
 public:
  ProxyAsyncResolverFactory(IpcPacketSocketFactory* ipc_psf)
      : ipc_psf_(ipc_psf) {
    DCHECK(ipc_psf);
  }

  rtc::AsyncResolverInterface* Create() override {
    return ipc_psf_->CreateAsyncResolver();
  }

 private:
  IpcPacketSocketFactory* ipc_psf_;
};

}  // namespace

PeerConnectionDependencyFactory::PeerConnectionDependencyFactory(
    bool create_p2p_socket_dispatcher)
    : network_manager_(nullptr),
      p2p_socket_dispatcher_(
          create_p2p_socket_dispatcher ? new P2PSocketDispatcher() : nullptr),
      chrome_signaling_thread_("WebRTC_Signaling"),
      chrome_network_thread_("WebRTC_Network") {
  TryScheduleStunProbeTrial();
}

PeerConnectionDependencyFactory::~PeerConnectionDependencyFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << "~PeerConnectionDependencyFactory()";
  DCHECK(!pc_factory_);
}

PeerConnectionDependencyFactory*
PeerConnectionDependencyFactory::GetInstance() {
  DEFINE_STATIC_LOCAL(PeerConnectionDependencyFactory, instance,
                      (/*create_p2p_socket_dispatcher= */ true));
  return &instance;
}

std::unique_ptr<RTCPeerConnectionHandler>
PeerConnectionDependencyFactory::CreateRTCPeerConnectionHandler(
    RTCPeerConnectionHandlerClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool force_encoded_audio_insertable_streams,
    bool force_encoded_video_insertable_streams) {
  // Save histogram data so we can see how much PeerConnection is used.
  // The histogram counts the number of calls to the JS API
  // RTCPeerConnection.
  UpdateWebRTCMethodCount(RTCAPIName::kRTCPeerConnection);

  return std::make_unique<RTCPeerConnectionHandler>(
      client, this, task_runner, force_encoded_audio_insertable_streams,
      force_encoded_video_insertable_streams);
}

const scoped_refptr<webrtc::PeerConnectionFactoryInterface>&
PeerConnectionDependencyFactory::GetPcFactory() {
  if (!pc_factory_.get())
    CreatePeerConnectionFactory();
  CHECK(pc_factory_.get());
  return pc_factory_;
}

void PeerConnectionDependencyFactory::WillDestroyCurrentMessageLoop() {
  CleanupPeerConnectionFactory();
}

void PeerConnectionDependencyFactory::CreatePeerConnectionFactory() {
  DCHECK(!pc_factory_.get());
  DCHECK(!signaling_thread_);
  DCHECK(!network_thread_);
  DCHECK(!network_manager_);
  DCHECK(!socket_factory_);
  DCHECK(!chrome_signaling_thread_.IsRunning());
  DCHECK(!chrome_network_thread_.IsRunning());

  DVLOG(1) << "PeerConnectionDependencyFactory::CreatePeerConnectionFactory()";

#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  // Building /w |rtc_use_h264|, is the corresponding run-time feature enabled?
  if (!base::FeatureList::IsEnabled(
          blink::features::kWebRtcH264WithOpenH264FFmpeg)) {
    // Feature is to be disabled.
    webrtc::DisableRtcUseH264();
  }
#else
  webrtc::DisableRtcUseH264();
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)

  base::CurrentThread::Get()->AddDestructionObserver(this);
  // To allow sending to the signaling/worker threads.
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);

  EnsureWebRtcAudioDeviceImpl();

  // Init SSL, which will be needed by PeerConnection.
  if (!rtc::InitializeSSL()) {
    LOG(ERROR) << "Failed on InitializeSSL.";
    NOTREACHED();
    return;
  }

  CHECK(chrome_signaling_thread_.Start());
  CHECK(chrome_network_thread_.Start());

  base::WaitableEvent create_network_manager_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::unique_ptr<MdnsResponderAdapter> mdns_responder;
#if BUILDFLAG(ENABLE_MDNS)
  if (base::FeatureList::IsEnabled(
          blink::features::kWebRtcHideLocalIpsWithMdns)) {
    // Note that MdnsResponderAdapter is created on the main thread to have
    // access to the connector to the service manager.
    mdns_responder = std::make_unique<MdnsResponderAdapter>();
  }
#endif  // BUILDFLAG(ENABLE_MDNS)
  PostCrossThreadTask(
      *chrome_network_thread_.task_runner().get(), FROM_HERE,
      CrossThreadBindOnce(&PeerConnectionDependencyFactory::
                              CreateIpcNetworkManagerOnNetworkThread,
                          CrossThreadUnretained(this),
                          CrossThreadUnretained(&create_network_manager_event),
                          std::move(mdns_responder),
                          CrossThreadUnretained(&network_thread_)));

  create_network_manager_event.Wait();
  CHECK(network_thread_);

  base::WaitableEvent start_signaling_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  PostCrossThreadTask(
      *chrome_signaling_thread_.task_runner().get(), FROM_HERE,
      CrossThreadBindOnce(
          &PeerConnectionDependencyFactory::InitializeSignalingThread,
          CrossThreadUnretained(this),
          CrossThreadUnretained(Platform::Current()->GetGpuFactories()),
          CrossThreadUnretained(&start_signaling_event)));

  start_signaling_event.Wait();
  CHECK(signaling_thread_);
}

void PeerConnectionDependencyFactory::InitializeSignalingThread(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    base::WaitableEvent* event) {
  DCHECK(chrome_signaling_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(network_thread_);
  DCHECK(p2p_socket_dispatcher_.get());

  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
  signaling_thread_ = jingle_glue::JingleThreadWrapper::current();

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
  socket_factory_.reset(new IpcPacketSocketFactory(p2p_socket_dispatcher_.get(),
                                                   traffic_annotation));

  std::unique_ptr<webrtc::VideoEncoderFactory> webrtc_encoder_factory =
      blink::CreateWebrtcVideoEncoderFactory(gpu_factories);
  std::unique_ptr<webrtc::VideoDecoderFactory> webrtc_decoder_factory =
      blink::CreateWebrtcVideoDecoderFactory(gpu_factories);

  // Enable Multiplex codec in SDP optionally.
  if (base::FeatureList::IsEnabled(blink::features::kWebRtcMultiplexCodec)) {
    webrtc_encoder_factory = std::make_unique<webrtc::MultiplexEncoderFactory>(
        std::move(webrtc_encoder_factory));
    webrtc_decoder_factory = std::make_unique<webrtc::MultiplexDecoderFactory>(
        std::move(webrtc_decoder_factory));
  }

  if (blink::Platform::Current()->UsesFakeCodecForPeerConnection()) {
    webrtc_encoder_factory =
        std::make_unique<webrtc::FakeVideoEncoderFactory>();
    webrtc_decoder_factory =
        std::make_unique<webrtc::FakeVideoDecoderFactory>();
  }

  webrtc::PeerConnectionFactoryDependencies pcf_deps;
  pcf_deps.worker_thread = signaling_thread_;
  pcf_deps.signaling_thread = signaling_thread_;
  pcf_deps.network_thread = network_thread_;
  pcf_deps.task_queue_factory = CreateWebRtcTaskQueueFactory();
  pcf_deps.call_factory = webrtc::CreateCallFactory();
  pcf_deps.event_log_factory = std::make_unique<webrtc::RtcEventLogFactory>(
      pcf_deps.task_queue_factory.get());
  cricket::MediaEngineDependencies media_deps;
  media_deps.task_queue_factory = pcf_deps.task_queue_factory.get();
  media_deps.adm = audio_device_.get();
  media_deps.audio_encoder_factory = blink::CreateWebrtcAudioEncoderFactory();
  media_deps.audio_decoder_factory = blink::CreateWebrtcAudioDecoderFactory();
  media_deps.video_encoder_factory = std::move(webrtc_encoder_factory);
  media_deps.video_decoder_factory = std::move(webrtc_decoder_factory);
  // Audio Processing Module (APM) instances are owned and handled by the Blink
  // media stream module.
  DCHECK_EQ(media_deps.audio_processing, nullptr);
  pcf_deps.media_engine = cricket::CreateMediaEngine(std::move(media_deps));
  pc_factory_ = webrtc::CreateModularPeerConnectionFactory(std::move(pcf_deps));
  CHECK(pc_factory_.get());

  webrtc::PeerConnectionFactoryInterface::Options factory_options;
  factory_options.disable_sctp_data_channels = false;
  factory_options.disable_encryption =
      !blink::Platform::Current()->IsWebRtcEncryptionEnabled();
  pc_factory_->SetOptions(factory_options);

  event->Signal();
}

bool PeerConnectionDependencyFactory::PeerConnectionFactoryCreated() {
  return !!pc_factory_;
}

scoped_refptr<webrtc::PeerConnectionInterface>
PeerConnectionDependencyFactory::CreatePeerConnection(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    blink::WebLocalFrame* web_frame,
    webrtc::PeerConnectionObserver* observer,
    ExceptionState& exception_state) {
  CHECK(web_frame);
  CHECK(observer);
  if (!GetPcFactory().get())
    return nullptr;

  rtc::SetAllowLegacyTLSProtocols(
      web_frame->Client()->AllowRTCLegacyTLSProtocols());
  webrtc::PeerConnectionDependencies dependencies(observer);
  dependencies.allocator = CreatePortAllocator(web_frame);
  dependencies.async_resolver_factory = CreateAsyncResolverFactory();
  auto pc_or_error = GetPcFactory()->CreatePeerConnectionOrError(
      config, std::move(dependencies));
  if (pc_or_error.ok()) {
    // Convert from rtc::scoped_refptr to scoped_refptr
    return pc_or_error.value().get();
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
        case DEFAULT_PUBLIC_INTERFACE_ONLY:
        case DEFAULT_PUBLIC_AND_PRIVATE_INTERFACES:
          port_config.enable_multiple_routes = false;
          port_config.enable_nonproxied_udp = true;
          port_config.enable_default_local_candidate =
              (policy == DEFAULT_PUBLIC_AND_PRIVATE_INTERFACES);
          break;
        case DISABLE_NON_PROXIED_UDP:
          port_config.enable_multiple_routes = false;
          port_config.enable_nonproxied_udp = false;
          break;
        case DEFAULT:
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

  // Now that this file is within Blink, it can not rely on WebURL's
  // GURL() operator directly. Hence, as per the comment on gurl.h, the
  // following GURL ctor is used instead.
  WebURL document_url = web_frame->GetDocument().Url();
  const GURL& requesting_origin =
      GURL(document_url.GetString().Utf8(), document_url.GetParsed(),
           document_url.IsValid())
          .GetOrigin();

  std::unique_ptr<rtc::NetworkManager> network_manager;
  if (port_config.enable_multiple_routes) {
    network_manager = std::make_unique<FilteringNetworkManager>(
        network_manager_.get(), media_permission, allow_mdns_obfuscation);
  } else {
    network_manager =
        std::make_unique<blink::EmptyNetworkManager>(network_manager_.get());
  }
  auto port_allocator = std::make_unique<P2PPortAllocator>(
      p2p_socket_dispatcher_, std::move(network_manager), socket_factory_.get(),
      port_config, requesting_origin);
  if (IsValidPortRange(min_port, max_port))
    port_allocator->SetPortRange(min_port, max_port);

  return port_allocator;
}

std::unique_ptr<webrtc::AsyncResolverFactory>
PeerConnectionDependencyFactory::CreateAsyncResolverFactory() {
  EnsureInitialized();
  return std::make_unique<ProxyAsyncResolverFactory>(socket_factory_.get());
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

  return webrtc::VideoTrackSourceProxy::Create(signaling_thread_,
                                               network_thread_, source)
      .get();
}

scoped_refptr<webrtc::VideoTrackInterface>
PeerConnectionDependencyFactory::CreateLocalVideoTrack(
    const String& id,
    webrtc::VideoTrackSourceInterface* source) {
  return GetPcFactory()->CreateVideoTrack(id.Utf8(), source).get();
}

webrtc::SessionDescriptionInterface*
PeerConnectionDependencyFactory::CreateSessionDescription(
    const String& type,
    const String& sdp,
    webrtc::SdpParseError* error) {
  return webrtc::CreateSessionDescription(type.Utf8(), sdp.Utf8(), error);
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

void PeerConnectionDependencyFactory::TryScheduleStunProbeTrial() {
  base::Optional<WebString> params =
      Platform::Current()->WebRtcStunProbeTrialParameter();
  if (!params)
    return;

  GetPcFactory();

  PostDelayedCrossThreadTask(
      *chrome_network_thread_.task_runner().get(), FROM_HERE,
      CrossThreadBindOnce(
          &PeerConnectionDependencyFactory::StartStunProbeTrialOnNetworkThread,
          CrossThreadUnretained(this), String(*params)),
      base::TimeDelta::FromMilliseconds(blink::kExperimentStartDelayMs));
}

void PeerConnectionDependencyFactory::StartStunProbeTrialOnNetworkThread(
    const String& params) {
  DCHECK(network_manager_);
  DCHECK(chrome_network_thread_.task_runner()->BelongsToCurrentThread());
  // TODO(crbug.com/787254): Remove the UTF8 conversion when StunProberTrial
  // operates over WTF::String.
  stun_trial_.reset(new StunProberTrial(network_manager_.get(), params.Utf8(),
                                        socket_factory_.get()));
}

void PeerConnectionDependencyFactory::CreateIpcNetworkManagerOnNetworkThread(
    base::WaitableEvent* event,
    std::unique_ptr<MdnsResponderAdapter> mdns_responder,
    rtc::Thread** thread) {
  DCHECK(chrome_network_thread_.task_runner()->BelongsToCurrentThread());

  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
  *thread = jingle_glue::JingleThreadWrapper::current();

  network_manager_ = std::make_unique<blink::IpcNetworkManager>(
      p2p_socket_dispatcher_.get(), std::move(mdns_responder));

  event->Signal();
}

void PeerConnectionDependencyFactory::DeleteIpcNetworkManager() {
  DCHECK(chrome_network_thread_.task_runner()->BelongsToCurrentThread());
  network_manager_.reset();
}

void PeerConnectionDependencyFactory::CleanupPeerConnectionFactory() {
  DVLOG(1) << "PeerConnectionDependencyFactory::CleanupPeerConnectionFactory()";
  pc_factory_ = nullptr;
  if (network_manager_) {
    // The network manager needs to free its resources on the thread they were
    // created, which is the worked thread.
    if (chrome_network_thread_.IsRunning()) {
      PostCrossThreadTask(
          *chrome_network_thread_.task_runner().get(), FROM_HERE,
          CrossThreadBindOnce(
              &PeerConnectionDependencyFactory::DeleteIpcNetworkManager,
              CrossThreadUnretained(this)));
      // Stopping the thread will wait until all tasks have been
      // processed before returning. We wait for the above task to finish before
      // letting the the function continue to avoid any potential race issues.
      chrome_network_thread_.Stop();
      DCHECK(!network_manager_);
    } else {
      NOTREACHED() << "Worker thread not running.";
    }
  }
}

void PeerConnectionDependencyFactory::EnsureInitialized() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetPcFactory();
}

scoped_refptr<base::SingleThreadTaskRunner>
PeerConnectionDependencyFactory::GetWebRtcNetworkTaskRunner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return chrome_network_thread_.IsRunning()
             ? chrome_network_thread_.task_runner()
             : nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
PeerConnectionDependencyFactory::GetWebRtcSignalingTaskRunner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  EnsureInitialized();
  return chrome_signaling_thread_.IsRunning()
             ? chrome_signaling_thread_.task_runner()
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

}  // namespace blink
