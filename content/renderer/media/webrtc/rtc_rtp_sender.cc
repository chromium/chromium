// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_rtp_sender.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "content/renderer/media/webrtc/rtc_dtmf_sender_handler.h"
#include "content/renderer/media/webrtc/rtc_stats.h"

namespace content {

namespace {

// TODO(hbos): Replace WebRTCVoidRequest with something resolving promises based
// on RTCError, as to surface both exception type and error message.
// https://crbug.com/790007
void OnReplaceTrackCompleted(blink::WebRTCVoidRequest request, bool result) {
  if (result)
    request.RequestSucceeded();
  else
    request.RequestFailed(
        webrtc::RTCError(webrtc::RTCErrorType::INVALID_MODIFICATION));
}

void OnSetParametersCompleted(blink::WebRTCVoidRequest request,
                              webrtc::RTCError result) {
  if (result.ok())
    request.RequestSucceeded();
  else
    request.RequestFailed(result);
}

}  // namespace

RtpSenderState::RtpSenderState(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner,
    scoped_refptr<webrtc::RtpSenderInterface> webrtc_sender,
    std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref,
    std::vector<std::string> stream_ids)
    : main_task_runner_(std::move(main_task_runner)),
      signaling_task_runner_(std::move(signaling_task_runner)),
      webrtc_sender_(std::move(webrtc_sender)),
      is_initialized_(false),
      track_ref_(std::move(track_ref)),
      stream_ids_(std::move(stream_ids)) {
  DCHECK(main_task_runner_);
  DCHECK(signaling_task_runner_);
  DCHECK(webrtc_sender_);
}

RtpSenderState::RtpSenderState(RtpSenderState&& other)
    : main_task_runner_(other.main_task_runner_),
      signaling_task_runner_(other.signaling_task_runner_),
      webrtc_sender_(std::move(other.webrtc_sender_)),
      is_initialized_(other.is_initialized_),
      track_ref_(std::move(other.track_ref_)),
      stream_ids_(std::move(other.stream_ids_)) {
  other.main_task_runner_ = nullptr;
  other.signaling_task_runner_ = nullptr;
}

RtpSenderState::~RtpSenderState() {
  // It's OK to not be on the main thread if this state has been moved, in which
  // case |main_task_runner_| is null.
  DCHECK(!main_task_runner_ || main_task_runner_->BelongsToCurrentThread());
}

RtpSenderState& RtpSenderState::operator=(RtpSenderState&& other) {
  DCHECK_EQ(main_task_runner_, other.main_task_runner_);
  DCHECK_EQ(signaling_task_runner_, other.signaling_task_runner_);
  other.main_task_runner_ = nullptr;
  other.signaling_task_runner_ = nullptr;
  webrtc_sender_ = std::move(other.webrtc_sender_);
  is_initialized_ = other.is_initialized_;
  track_ref_ = std::move(other.track_ref_);
  stream_ids_ = std::move(other.stream_ids_);
  return *this;
}

bool RtpSenderState::is_initialized() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return is_initialized_;
}

void RtpSenderState::Initialize() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (track_ref_)
    track_ref_->InitializeOnMainThread();
  is_initialized_ = true;
}

scoped_refptr<base::SingleThreadTaskRunner> RtpSenderState::main_task_runner()
    const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return main_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RtpSenderState::signaling_task_runner() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return signaling_task_runner_;
}

scoped_refptr<webrtc::RtpSenderInterface> RtpSenderState::webrtc_sender()
    const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return webrtc_sender_;
}

const std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>&
RtpSenderState::track_ref() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return track_ref_;
}

void RtpSenderState::set_track_ref(
    std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!is_initialized_ || !track_ref || track_ref->is_initialized());
  track_ref_ = std::move(track_ref);
}

std::vector<std::string> RtpSenderState::stream_ids() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return stream_ids_;
}

class RTCRtpSender::RTCRtpSenderInternal
    : public base::RefCountedThreadSafe<
          RTCRtpSender::RTCRtpSenderInternal,
          RTCRtpSender::RTCRtpSenderInternalTraits> {
 public:
  RTCRtpSenderInternal(
      scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection,
      scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_map,
      RtpSenderState state)
      : native_peer_connection_(std::move(native_peer_connection)),
        track_map_(std::move(track_map)),
        main_task_runner_(state.main_task_runner()),
        signaling_task_runner_(state.signaling_task_runner()),
        webrtc_sender_(state.webrtc_sender()),
        state_(std::move(state)) {
    DCHECK(track_map_);
    DCHECK(state_.is_initialized());
  }

  const RtpSenderState& state() const {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    return state_;
  }

  void set_state(RtpSenderState state) {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    DCHECK_EQ(state.main_task_runner(), main_task_runner_);
    DCHECK_EQ(state.signaling_task_runner(), signaling_task_runner_);
    DCHECK(state.webrtc_sender() == webrtc_sender_);
    DCHECK(state.is_initialized());
    state_ = std::move(state);
  }

  void ReplaceTrack(blink::WebMediaStreamTrack with_track,
                    base::OnceCallback<void(bool)> callback) {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref;
    webrtc::MediaStreamTrackInterface* webrtc_track = nullptr;
    if (!with_track.IsNull()) {
      track_ref = track_map_->GetOrCreateLocalTrackAdapter(with_track);
      webrtc_track = track_ref->webrtc_track();
    }
    signaling_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RTCRtpSender::RTCRtpSenderInternal::ReplaceTrackOnSignalingThread,
            this, std::move(track_ref), base::Unretained(webrtc_track),
            std::move(callback)));
  }

  std::unique_ptr<blink::WebRTCDTMFSenderHandler> GetDtmfSender() const {
    // The webrtc_sender() is a proxy, so this is a blocking call to the
    // webrtc signalling thread.
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    auto dtmf_sender = webrtc_sender_->GetDtmfSender();
    return std::make_unique<RtcDtmfSenderHandler>(main_task_runner_,
                                                  dtmf_sender);
  }

  std::unique_ptr<webrtc::RtpParameters> GetParameters() {
    // The webrtc_sender() is a proxy, so this is a blocking call to the
    // webrtc signalling thread.
    parameters_ = webrtc_sender_->GetParameters();
    return std::make_unique<webrtc::RtpParameters>(parameters_);
  }

  void SetParameters(blink::WebVector<webrtc::RtpEncodingParameters> encodings,
                     webrtc::DegradationPreference degradation_preference,
                     base::OnceCallback<void(webrtc::RTCError)> callback) {
    DCHECK(main_task_runner_->BelongsToCurrentThread());

    webrtc::RtpParameters new_parameters = parameters_;

    new_parameters.degradation_preference = degradation_preference;

    for (std::size_t i = 0; i < new_parameters.encodings.size(); ++i) {
      // Encodings have other parameters in the native layer that aren't exposed
      // to the blink layer. So instead of copying the new struct over the old
      // one, we copy the members one by one over the old struct, effectively
      // patching the changes done by the user.
      const auto& encoding = encodings[i];
      new_parameters.encodings[i].codec_payload_type =
          encoding.codec_payload_type;
      new_parameters.encodings[i].dtx = encoding.dtx;
      new_parameters.encodings[i].active = encoding.active;
      new_parameters.encodings[i].bitrate_priority = encoding.bitrate_priority;
      new_parameters.encodings[i].ptime = encoding.ptime;
      new_parameters.encodings[i].max_bitrate_bps = encoding.max_bitrate_bps;
      new_parameters.encodings[i].max_framerate = encoding.max_framerate;
      new_parameters.encodings[i].rid = encoding.rid;
      new_parameters.encodings[i].scale_resolution_down_by =
          encoding.scale_resolution_down_by;
    }

    signaling_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RTCRtpSender::RTCRtpSenderInternal::SetParametersOnSignalingThread,
            this, std::move(new_parameters), std::move(callback)));
  }

  void GetStats(std::unique_ptr<blink::WebRTCStatsReportCallback> callback) {
    signaling_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RTCRtpSender::RTCRtpSenderInternal::GetStatsOnSignalingThread,
            this, std::move(callback)));
  }

  bool RemoveFromPeerConnection(webrtc::PeerConnectionInterface* pc) {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    if (!pc->RemoveTrack(webrtc_sender_.get()))
      return false;
    // TODO(hbos): Removing the track should null the sender's track, or we
    // should do |webrtc_sender_->SetTrack(null)| but that is not allowed on a
    // stopped sender. In the meantime, there is a discrepancy between layers.
    // https://crbug.com/webrtc/7945
    state_.set_track_ref(nullptr);
    return true;
  }

 private:
  friend struct RTCRtpSender::RTCRtpSenderInternalTraits;

  ~RTCRtpSenderInternal() {
    // Ensured by destructor traits.
    DCHECK(main_task_runner_->BelongsToCurrentThread());
  }

  // |webrtc_track| is passed as an argument because |track_ref->webrtc_track()|
  // cannot be accessed on the signaling thread. https://crbug.com/756436
  void ReplaceTrackOnSignalingThread(
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref,
      webrtc::MediaStreamTrackInterface* webrtc_track,
      base::OnceCallback<void(bool)> callback) {
    DCHECK(signaling_task_runner_->BelongsToCurrentThread());
    bool result = webrtc_sender_->SetTrack(webrtc_track);
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RTCRtpSender::RTCRtpSenderInternal::ReplaceTrackCallback, this,
            result, std::move(track_ref), std::move(callback)));
  }

  void ReplaceTrackCallback(
      bool result,
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref,
      base::OnceCallback<void(bool)> callback) {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    if (result)
      state_.set_track_ref(std::move(track_ref));
    std::move(callback).Run(result);
  }

  void GetStatsOnSignalingThread(
      std::unique_ptr<blink::WebRTCStatsReportCallback> callback) {
    native_peer_connection_->GetStats(
        webrtc_sender_.get(), RTCStatsCollectorCallbackImpl::Create(
                                  main_task_runner_, std::move(callback)));
  }

  void SetParametersOnSignalingThread(
      webrtc::RtpParameters parameters,
      base::OnceCallback<void(webrtc::RTCError)> callback) {
    DCHECK(signaling_task_runner_->BelongsToCurrentThread());
    webrtc::RTCError result = webrtc_sender_->SetParameters(parameters);
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RTCRtpSender::RTCRtpSenderInternal::SetParametersCallback, this,
            std::move(result), std::move(callback)));
  }

  void SetParametersCallback(
      webrtc::RTCError result,
      base::OnceCallback<void(webrtc::RTCError)> callback) {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    std::move(callback).Run(std::move(result));
  }

  const scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection_;
  const scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_map_;
  // Task runners and webrtc sender: Same information as stored in
  // |state_| but const and safe to touch on the signaling thread to
  // avoid race with set_state().
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner_;
  const scoped_refptr<webrtc::RtpSenderInterface> webrtc_sender_;
  RtpSenderState state_;
  webrtc::RtpParameters parameters_;
};

struct RTCRtpSender::RTCRtpSenderInternalTraits {
 private:
  friend class base::RefCountedThreadSafe<RTCRtpSenderInternal,
                                          RTCRtpSenderInternalTraits>;

  static void Destruct(const RTCRtpSenderInternal* sender) {
    // RTCRtpSenderInternal owns AdapterRefs which have to be destroyed on the
    // main thread, this ensures delete always happens there.
    if (!sender->main_task_runner_->BelongsToCurrentThread()) {
      sender->main_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&RTCRtpSender::RTCRtpSenderInternalTraits::Destruct,
                         base::Unretained(sender)));
      return;
    }
    delete sender;
  }
};

uintptr_t RTCRtpSender::getId(const webrtc::RtpSenderInterface* webrtc_sender) {
  return reinterpret_cast<uintptr_t>(webrtc_sender);
}

RTCRtpSender::RTCRtpSender(
    scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection,
    scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_map,
    RtpSenderState state)
    : internal_(new RTCRtpSenderInternal(std::move(native_peer_connection),
                                         std::move(track_map),
                                         std::move(state))) {}

RTCRtpSender::RTCRtpSender(const RTCRtpSender& other)
    : internal_(other.internal_) {}

RTCRtpSender::~RTCRtpSender() {}

RTCRtpSender& RTCRtpSender::operator=(const RTCRtpSender& other) {
  internal_ = other.internal_;
  return *this;
}

const RtpSenderState& RTCRtpSender::state() const {
  return internal_->state();
}

void RTCRtpSender::set_state(RtpSenderState state) {
  internal_->set_state(std::move(state));
}

std::unique_ptr<blink::WebRTCRtpSender> RTCRtpSender::ShallowCopy() const {
  return std::make_unique<RTCRtpSender>(*this);
}

uintptr_t RTCRtpSender::Id() const {
  return getId(internal_->state().webrtc_sender().get());
}

blink::WebMediaStreamTrack RTCRtpSender::Track() const {
  const auto& track_ref = internal_->state().track_ref();
  return track_ref ? track_ref->web_track() : blink::WebMediaStreamTrack();
}

blink::WebVector<blink::WebString> RTCRtpSender::StreamIds() const {
  const auto& stream_ids = internal_->state().stream_ids();
  blink::WebVector<blink::WebString> web_stream_ids(stream_ids.size());
  for (size_t i = 0; i < stream_ids.size(); ++i)
    web_stream_ids[i] = blink::WebString::FromUTF8(stream_ids[i]);
  return web_stream_ids;
}

void RTCRtpSender::ReplaceTrack(blink::WebMediaStreamTrack with_track,
                                blink::WebRTCVoidRequest request) {
  internal_->ReplaceTrack(
      std::move(with_track),
      base::BindOnce(&OnReplaceTrackCompleted, std::move(request)));
}

std::unique_ptr<blink::WebRTCDTMFSenderHandler> RTCRtpSender::GetDtmfSender()
    const {
  return internal_->GetDtmfSender();
}

std::unique_ptr<webrtc::RtpParameters> RTCRtpSender::GetParameters() const {
  return internal_->GetParameters();
}

void RTCRtpSender::SetParameters(
    blink::WebVector<webrtc::RtpEncodingParameters> encodings,
    webrtc::DegradationPreference degradation_preference,
    blink::WebRTCVoidRequest request) {
  internal_->SetParameters(
      std::move(encodings), degradation_preference,
      base::BindOnce(&OnSetParametersCompleted, std::move(request)));
}

void RTCRtpSender::GetStats(
    std::unique_ptr<blink::WebRTCStatsReportCallback> callback) {
  internal_->GetStats(std::move(callback));
}

void RTCRtpSender::ReplaceTrack(blink::WebMediaStreamTrack with_track,
                                base::OnceCallback<void(bool)> callback) {
  internal_->ReplaceTrack(std::move(with_track), std::move(callback));
}

bool RTCRtpSender::RemoveFromPeerConnection(
    webrtc::PeerConnectionInterface* pc) {
  return internal_->RemoveFromPeerConnection(pc);
}

RTCRtpSenderOnlyTransceiver::RTCRtpSenderOnlyTransceiver(
    std::unique_ptr<blink::WebRTCRtpSender> sender)
    : sender_(std::move(sender)) {
  DCHECK(sender_);
}

RTCRtpSenderOnlyTransceiver::~RTCRtpSenderOnlyTransceiver() {}

blink::WebRTCRtpTransceiverImplementationType
RTCRtpSenderOnlyTransceiver::ImplementationType() const {
  return blink::WebRTCRtpTransceiverImplementationType::kPlanBSenderOnly;
}

uintptr_t RTCRtpSenderOnlyTransceiver::Id() const {
  NOTIMPLEMENTED();
  return 0u;
}

blink::WebString RTCRtpSenderOnlyTransceiver::Mid() const {
  NOTIMPLEMENTED();
  return blink::WebString();
}

std::unique_ptr<blink::WebRTCRtpSender> RTCRtpSenderOnlyTransceiver::Sender()
    const {
  return sender_->ShallowCopy();
}

std::unique_ptr<blink::WebRTCRtpReceiver>
RTCRtpSenderOnlyTransceiver::Receiver() const {
  NOTIMPLEMENTED();
  return nullptr;
}

bool RTCRtpSenderOnlyTransceiver::Stopped() const {
  NOTIMPLEMENTED();
  return false;
}

webrtc::RtpTransceiverDirection RTCRtpSenderOnlyTransceiver::Direction() const {
  NOTIMPLEMENTED();
  return webrtc::RtpTransceiverDirection::kSendOnly;
}

void RTCRtpSenderOnlyTransceiver::SetDirection(
    webrtc::RtpTransceiverDirection direction) {
  NOTIMPLEMENTED();
}

base::Optional<webrtc::RtpTransceiverDirection>
RTCRtpSenderOnlyTransceiver::CurrentDirection() const {
  NOTIMPLEMENTED();
  return webrtc::RtpTransceiverDirection::kSendOnly;
}

base::Optional<webrtc::RtpTransceiverDirection>
RTCRtpSenderOnlyTransceiver::FiredDirection() const {
  NOTIMPLEMENTED();
  return webrtc::RtpTransceiverDirection::kSendOnly;
}

}  // namespace content
