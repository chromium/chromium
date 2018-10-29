// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/common/buildflags.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/feature_h264_with_openh264_ffmpeg.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/webrtc_ip_handling_policy.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/media/stream/media_stream_video_source.h"
#include "content/renderer/media/stream/media_stream_video_track.h"
#include "content/renderer/media/webrtc/audio_codec_factory.h"
#include "content/renderer/media/webrtc/rtc_peer_connection_handler.h"
#include "content/renderer/media/webrtc/rtc_video_decoder_factory.h"
#include "content/renderer/media/webrtc/rtc_video_encoder_factory.h"
#include "content/renderer/media/webrtc/stun_field_trial.h"
#include "content/renderer/media/webrtc/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc/webrtc_uma_histograms.h"
#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"
#include "content/renderer/media/webrtc_logging.h"
#include "content/renderer/p2p/empty_network_manager.h"
#include "content/renderer/p2p/filtering_network_manager.h"
#include "content/renderer/p2p/ipc_network_manager.h"
#include "content/renderer/p2p/ipc_socket_factory.h"
#include "content/renderer/p2p/port_allocator.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "crypto/openssl_util.h"
#include "jingle/glue/thread_wrapper.h"
#include "media/base/media_permission.h"
#include "media/media_buildflags.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/webrtc/api/mediaconstraintsinterface.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_factory.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"
#include "third_party/webrtc/api/videosourceproxy.h"
#include "third_party/webrtc/media/engine/convert_legacy_video_factory.h"
#include "third_party/webrtc/media/engine/multiplexcodecfactory.h"
#include "third_party/webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "third_party/webrtc/rtc_base/refcountedobject.h"
#include "third_party/webrtc/rtc_base/ssladapter.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

namespace content {

namespace {

enum WebRTCIPHandlingPolicy {
  DEFAULT,
  DEFAULT_PUBLIC_AND_PRIVATE_INTERFACES,
  DEFAULT_PUBLIC_INTERFACE_ONLY,
  DISABLE_NON_PROXIED_UDP,
};

WebRTCIPHandlingPolicy GetWebRTCIPHandlingPolicy(
    const std::string& preference) {
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
    P2PSocketDispatcher* p2p_socket_dispatcher)
    : network_manager_(nullptr),
      p2p_socket_dispatcher_(p2p_socket_dispatcher),
      signaling_thread_(nullptr),
      worker_thread_(nullptr),
      chrome_signaling_thread_("Chrome_libJingle_Signaling"),
      chrome_worker_thread_("Chrome_libJingle_WorkerThread") {
  TryScheduleStunProbeTrial();
}

PeerConnectionDependencyFactory::~PeerConnectionDependencyFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "~PeerConnectionDependencyFactory()";
  DCHECK(!pc_factory_);
}

std::unique_ptr<blink::WebRTCPeerConnectionHandler>
PeerConnectionDependencyFactory::CreateRTCPeerConnectionHandler(
    blink::WebRTCPeerConnectionHandlerClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  // Save histogram data so we can see how much PeerConnetion is used.
  // The histogram counts the number of calls to the JS API
  // webKitRTCPeerConnection.
  UpdateWebRTCMethodCount(blink::WebRTCAPIName::kRTCPeerConnection);

  return std::make_unique<RTCPeerConnectionHandler>(client, this, task_runner);
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
  DCHECK(!worker_thread_);
  DCHECK(!network_manager_);
  DCHECK(!socket_factory_);
  DCHECK(!chrome_signaling_thread_.IsRunning());
  DCHECK(!chrome_worker_thread_.IsRunning());

  DVLOG(1) << "PeerConnectionDependencyFactory::CreatePeerConnectionFactory()";

#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  // Building /w |rtc_use_h264|, is the corresponding run-time feature enabled?
  if (!base::FeatureList::IsEnabled(kWebRtcH264WithOpenH264FFmpeg)) {
    // Feature is to be disabled.
    webrtc::DisableRtcUseH264();
  }
#else
  webrtc::DisableRtcUseH264();
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)

  base::MessageLoopCurrent::Get()->AddDestructionObserver(this);
  // To allow sending to the signaling/worker threads.
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);

  EnsureWebRtcAudioDeviceImpl();

  CHECK(chrome_signaling_thread_.Start());
  CHECK(chrome_worker_thread_.Start());

  base::WaitableEvent start_worker_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  chrome_worker_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PeerConnectionDependencyFactory::InitializeWorkerThread,
                     base::Unretained(this), &worker_thread_,
                     &start_worker_event));

  base::WaitableEvent create_network_manager_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  chrome_worker_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PeerConnectionDependencyFactory::
                         CreateIpcNetworkManagerOnWorkerThread,
                     base::Unretained(this), &create_network_manager_event));

  start_worker_event.Wait();
  create_network_manager_event.Wait();

  CHECK(worker_thread_);

  // Init SSL, which will be needed by PeerConnection.
  if (!rtc::InitializeSSL()) {
    LOG(ERROR) << "Failed on InitializeSSL.";
    NOTREACHED();
    return;
  }

  base::WaitableEvent start_signaling_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  chrome_signaling_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PeerConnectionDependencyFactory::InitializeSignalingThread,
          base::Unretained(this),
          RenderThreadImpl::current()->GetGpuFactories(),
          &start_signaling_event));

  start_signaling_event.Wait();
  CHECK(signaling_thread_);
}

void PeerConnectionDependencyFactory::InitializeSignalingThread(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    base::WaitableEvent* event) {
  DCHECK(chrome_signaling_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(worker_thread_);
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

  std::unique_ptr<cricket::WebRtcVideoDecoderFactory> decoder_factory;
  std::unique_ptr<cricket::WebRtcVideoEncoderFactory> encoder_factory;

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (gpu_factories && gpu_factories->IsGpuVideoAcceleratorEnabled()) {
    if (!cmd_line->HasSwitch(switches::kDisableWebRtcHWDecoding))
      decoder_factory.reset(new RTCVideoDecoderFactory(gpu_factories));

    if (!cmd_line->HasSwitch(switches::kDisableWebRtcHWEncoding))
      encoder_factory.reset(new RTCVideoEncoderFactory(gpu_factories));
  }

#if defined(OS_ANDROID)
  if (!media::MediaCodecUtil::SupportsSetParameters())
    encoder_factory.reset();
#endif

  // TODO(magjed): Update RTCVideoEncoderFactory/RTCVideoDecoderFactory to new
  // interface and let Chromium be responsible in what order video codecs are
  // listed, instead of using
  // cricket::ConvertVideoEncoderFactory/cricket::ConvertVideoDecoderFactory.
  std::unique_ptr<webrtc::VideoEncoderFactory> webrtc_encoder_factory =
      ConvertVideoEncoderFactory(std::move(encoder_factory));
  std::unique_ptr<webrtc::VideoDecoderFactory> webrtc_decoder_factory =
      ConvertVideoDecoderFactory(std::move(decoder_factory));

  // Enable Multiplex codec in SDP optionally.
  if (base::FeatureList::IsEnabled(features::kWebRtcMultiplexCodec)) {
    webrtc_encoder_factory = std::make_unique<webrtc::MultiplexEncoderFactory>(
        std::move(webrtc_encoder_factory));
    webrtc_decoder_factory = std::make_unique<webrtc::MultiplexDecoderFactory>(
        std::move(webrtc_decoder_factory));
  }

  pc_factory_ = webrtc::CreatePeerConnectionFactory(
      worker_thread_ /* network thread */, worker_thread_, signaling_thread_,
      audio_device_.get(), CreateWebrtcAudioEncoderFactory(),
      CreateWebrtcAudioDecoderFactory(), std::move(webrtc_encoder_factory),
      std::move(webrtc_decoder_factory), nullptr /* audio_mixer */,
      nullptr /* audio_processing */);
  CHECK(pc_factory_.get());

  webrtc::PeerConnectionFactoryInterface::Options factory_options;
  factory_options.disable_sctp_data_channels = false;
  factory_options.disable_encryption =
      cmd_line->HasSwitch(switches::kDisableWebRtcEncryption);
  factory_options.crypto_options.srtp.enable_gcm_crypto_suites =
      cmd_line->HasSwitch(switches::kEnableWebRtcSrtpAesGcm);
  factory_options.crypto_options.srtp.enable_encrypted_rtp_header_extensions =
      cmd_line->HasSwitch(switches::kEnableWebRtcSrtpEncryptedHeaders);
  pc_factory_->SetOptions(factory_options);

  event->Signal();
}

bool PeerConnectionDependencyFactory::PeerConnectionFactoryCreated() {
  return pc_factory_.get() != nullptr;
}

scoped_refptr<webrtc::PeerConnectionInterface>
PeerConnectionDependencyFactory::CreatePeerConnection(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    blink::WebLocalFrame* web_frame,
    webrtc::PeerConnectionObserver* observer) {
  CHECK(web_frame);
  CHECK(observer);
  if (!GetPcFactory().get())
    return nullptr;

  webrtc::PeerConnectionDependencies dependencies(observer);
  dependencies.allocator = CreatePortAllocator(web_frame);
  dependencies.async_resolver_factory =
      std::make_unique<ProxyAsyncResolverFactory>(socket_factory_.get());
  return GetPcFactory()
      ->CreatePeerConnection(config, std::move(dependencies))
      .get();
}

std::unique_ptr<P2PPortAllocator>
PeerConnectionDependencyFactory::CreatePortAllocator(
    blink::WebLocalFrame* web_frame) {
  DCHECK(web_frame);

  // Copy the flag from Preference associated with this WebLocalFrame.
  P2PPortAllocator::Config port_config;
  uint16_t min_port = 0;
  uint16_t max_port = 0;

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
  if (!GetContentClient()
           ->renderer()
           ->ShouldEnforceWebRTCRoutingPreferences()) {
    port_config.enable_multiple_routes = true;
    port_config.enable_nonproxied_udp = true;
    VLOG(3) << "WebRTC routing preferences will not be enforced";
  } else {
    if (web_frame && web_frame->View()) {
      RenderViewImpl* renderer_view_impl =
          RenderViewImpl::FromWebView(web_frame->View());
      if (renderer_view_impl) {
        // TODO(guoweis): |enable_multiple_routes| should be renamed to
        // |request_multiple_routes|. Whether local IP addresses could be
        // collected depends on if mic/camera permission is granted for this
        // origin.
        WebRTCIPHandlingPolicy policy =
            GetWebRTCIPHandlingPolicy(renderer_view_impl->renderer_preferences()
                                          .webrtc_ip_handling_policy);
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

        min_port =
            renderer_view_impl->renderer_preferences().webrtc_udp_min_port;
        max_port =
            renderer_view_impl->renderer_preferences().webrtc_udp_max_port;

        VLOG(3) << "WebRTC routing preferences: "
                << "policy: " << policy
                << ", multiple_routes: " << port_config.enable_multiple_routes
                << ", nonproxied_udp: " << port_config.enable_nonproxied_udp
                << ", min_udp_port: " << min_port
                << ", max_udp_port: " << max_port;
      }
    }
    if (port_config.enable_multiple_routes) {
      bool create_media_permission =
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnforceWebRtcIPPermissionCheck);
      create_media_permission =
          create_media_permission ||
          !StartsWith(base::FieldTrialList::FindFullName(
                          "WebRTC-LocalIPPermissionCheck"),
                      "Disabled", base::CompareCase::SENSITIVE);
      if (create_media_permission) {
        content::RenderFrameImpl* render_frame =
            content::RenderFrameImpl::FromWebFrame(web_frame);
        if (render_frame)
          media_permission = render_frame->GetMediaPermission();
        DCHECK(media_permission);
      }
    }
  }

  const GURL& requesting_origin =
      GURL(web_frame->GetDocument().Url()).GetOrigin();

  std::unique_ptr<rtc::NetworkManager> network_manager;
  if (port_config.enable_multiple_routes) {
    FilteringNetworkManager* filtering_network_manager =
        new FilteringNetworkManager(network_manager_, requesting_origin,
                                    media_permission);
    network_manager.reset(filtering_network_manager);
  } else {
    network_manager.reset(new EmptyNetworkManager(network_manager_));
  }
  auto port_allocator = std::make_unique<P2PPortAllocator>(
      p2p_socket_dispatcher_, std::move(network_manager), socket_factory_.get(),
      port_config, requesting_origin);
  if (IsValidPortRange(min_port, max_port))
    port_allocator->SetPortRange(min_port, max_port);

  return port_allocator;
}

scoped_refptr<webrtc::MediaStreamInterface>
PeerConnectionDependencyFactory::CreateLocalMediaStream(
    const std::string& label) {
  return GetPcFactory()->CreateLocalMediaStream(label).get();
}

scoped_refptr<webrtc::VideoTrackSourceInterface>
PeerConnectionDependencyFactory::CreateVideoTrackSourceProxy(
    webrtc::VideoTrackSourceInterface* source) {
  // PeerConnectionFactory needs to be instantiated to make sure that
  // signaling_thread_ and worker_thread_ exist.
  if (!PeerConnectionFactoryCreated())
    CreatePeerConnectionFactory();

  return webrtc::VideoTrackSourceProxy::Create(signaling_thread_,
                                               worker_thread_, source)
      .get();
}

scoped_refptr<webrtc::VideoTrackInterface>
PeerConnectionDependencyFactory::CreateLocalVideoTrack(
    const std::string& id,
    webrtc::VideoTrackSourceInterface* source) {
  return GetPcFactory()->CreateVideoTrack(id, source).get();
}

webrtc::SessionDescriptionInterface*
PeerConnectionDependencyFactory::CreateSessionDescription(
    const std::string& type,
    const std::string& sdp,
    webrtc::SdpParseError* error) {
  return webrtc::CreateSessionDescription(type, sdp, error);
}

webrtc::IceCandidateInterface*
PeerConnectionDependencyFactory::CreateIceCandidate(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const std::string& sdp) {
  return webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, sdp, nullptr);
}

WebRtcAudioDeviceImpl*
PeerConnectionDependencyFactory::GetWebRtcAudioDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureWebRtcAudioDeviceImpl();
  return audio_device_.get();
}

void PeerConnectionDependencyFactory::InitializeWorkerThread(
    rtc::Thread** thread,
    base::WaitableEvent* event) {
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
  *thread = jingle_glue::JingleThreadWrapper::current();
  event->Signal();
}

void PeerConnectionDependencyFactory::TryScheduleStunProbeTrial() {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  if (!cmd_line->HasSwitch(switches::kWebRtcStunProbeTrialParameter))
    return;

  GetPcFactory();

  const std::string params =
      cmd_line->GetSwitchValueASCII(switches::kWebRtcStunProbeTrialParameter);

  chrome_worker_thread_.task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &PeerConnectionDependencyFactory::StartStunProbeTrialOnWorkerThread,
          base::Unretained(this), params),
      base::TimeDelta::FromMilliseconds(kExperimentStartDelayMs));
}

void PeerConnectionDependencyFactory::StartStunProbeTrialOnWorkerThread(
    const std::string& params) {
  DCHECK(network_manager_);
  DCHECK(chrome_worker_thread_.task_runner()->BelongsToCurrentThread());
  stun_trial_.reset(
      new StunProberTrial(network_manager_, params, socket_factory_.get()));
}

void PeerConnectionDependencyFactory::CreateIpcNetworkManagerOnWorkerThread(
    base::WaitableEvent* event) {
  DCHECK(chrome_worker_thread_.task_runner()->BelongsToCurrentThread());
  network_manager_ = new IpcNetworkManager(p2p_socket_dispatcher_.get());
  event->Signal();
}

void PeerConnectionDependencyFactory::DeleteIpcNetworkManager() {
  DCHECK(chrome_worker_thread_.task_runner()->BelongsToCurrentThread());
  delete network_manager_;
  network_manager_ = nullptr;
}

void PeerConnectionDependencyFactory::CleanupPeerConnectionFactory() {
  DVLOG(1) << "PeerConnectionDependencyFactory::CleanupPeerConnectionFactory()";
  pc_factory_ = nullptr;
  if (network_manager_) {
    // The network manager needs to free its resources on the thread they were
    // created, which is the worked thread.
    if (chrome_worker_thread_.IsRunning()) {
      chrome_worker_thread_.task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &PeerConnectionDependencyFactory::DeleteIpcNetworkManager,
              base::Unretained(this)));
      // Stopping the thread will wait until all tasks have been
      // processed before returning. We wait for the above task to finish before
      // letting the the function continue to avoid any potential race issues.
      chrome_worker_thread_.Stop();
    } else {
      NOTREACHED() << "Worker thread not running.";
    }
  }
}

void PeerConnectionDependencyFactory::EnsureInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetPcFactory();
}

scoped_refptr<base::SingleThreadTaskRunner>
PeerConnectionDependencyFactory::GetWebRtcWorkerThread() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return chrome_worker_thread_.IsRunning() ? chrome_worker_thread_.task_runner()
                                           : nullptr;
}

rtc::Thread* PeerConnectionDependencyFactory::GetWebRtcWorkerThreadRtcThread()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return chrome_worker_thread_.IsRunning() ? worker_thread_ : nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
PeerConnectionDependencyFactory::GetWebRtcSignalingThread() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return chrome_signaling_thread_.IsRunning()
             ? chrome_signaling_thread_.task_runner()
             : nullptr;
}

void PeerConnectionDependencyFactory::EnsureWebRtcAudioDeviceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (audio_device_.get())
    return;

  audio_device_ = new rtc::RefCountedObject<WebRtcAudioDeviceImpl>();
}

std::unique_ptr<webrtc::RtpCapabilities>
PeerConnectionDependencyFactory::GetSenderCapabilities(
    const std::string& kind) {
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
PeerConnectionDependencyFactory::GetReceiverCapabilities(
    const std::string& kind) {
  if (kind == "audio") {
    return std::make_unique<webrtc::RtpCapabilities>(
        GetPcFactory()->GetRtpReceiverCapabilities(cricket::MEDIA_TYPE_AUDIO));
  } else if (kind == "video") {
    return std::make_unique<webrtc::RtpCapabilities>(
        GetPcFactory()->GetRtpReceiverCapabilities(cricket::MEDIA_TYPE_VIDEO));
  }
  return nullptr;
}

}  // namespace content
