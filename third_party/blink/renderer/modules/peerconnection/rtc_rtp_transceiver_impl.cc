// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transceiver_impl.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/webrtc/api/scoped_refptr.h"

namespace blink {

RtpTransceiverState::RtpTransceiverState(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner,
    scoped_refptr<webrtc::RtpTransceiverInterface> webrtc_transceiver,
    std::optional<blink::RtpSenderState> sender_state,
    std::optional<blink::RtpReceiverState> receiver_state,
    std::optional<std::string> mid,
    webrtc::RtpTransceiverDirection direction,
    std::optional<webrtc::RtpTransceiverDirection> current_direction,
    std::optional<webrtc::RtpTransceiverDirection> fired_direction,
    WTF::Vector<webrtc::RtpHeaderExtensionCapability>
        header_extensions_negotiated)
    : main_task_runner_(std::move(main_task_runner)),
      signaling_task_runner_(std::move(signaling_task_runner)),
      webrtc_transceiver_(std::move(webrtc_transceiver)),
      is_initialized_(false),
      sender_state_(std::move(sender_state)),
      receiver_state_(std::move(receiver_state)),
      mid_(std::move(mid)),
      direction_(std::move(direction)),
      current_direction_(std::move(current_direction)),
      fired_direction_(std::move(fired_direction)),
      header_extensions_negotiated_(std::move(header_extensions_negotiated)) {
  DCHECK(main_task_runner_);
  DCHECK(signaling_task_runner_);
  DCHECK(webrtc_transceiver_);
}

RtpTransceiverState::RtpTransceiverState(RtpTransceiverState&& other)
    : main_task_runner_(other.main_task_runner_),
      signaling_task_runner_(other.signaling_task_runner_),
      webrtc_transceiver_(std::move(other.webrtc_transceiver_)),
      is_initialized_(other.is_initialized_),
      sender_state_(std::move(other.sender_state_)),
      receiver_state_(std::move(other.receiver_state_)),
      mid_(std::move(other.mid_)),
      direction_(std::move(other.direction_)),
      current_direction_(std::move(other.current_direction_)),
      fired_direction_(std::move(other.fired_direction_)),
      header_extensions_negotiated_(
          std::move(other.header_extensions_negotiated_)) {
  // Explicitly null |other|'s task runners for use in destructor.
  other.main_task_runner_ = nullptr;
  other.signaling_task_runner_ = nullptr;
}

RtpTransceiverState::~RtpTransceiverState() {
  // It's OK to not be on the main thread if this state has been moved, in which
  // case |main_task_runner_| is null.
  DCHECK(!main_task_runner_ || main_task_runner_->BelongsToCurrentThread());
}

RtpTransceiverState& RtpTransceiverState::operator=(
    RtpTransceiverState&& other) {
  DCHECK_EQ(main_task_runner_, other.main_task_runner_);
  DCHECK_EQ(signaling_task_runner_, other.signaling_task_runner_);
  // Need to be on main thread for sender/receiver state's destructor that can
  // be triggered by replacing .
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  // Explicitly null |other|'s task runners for use in destructor.
  other.main_task_runner_ = nullptr;
  other.signaling_task_runner_ = nullptr;
  webrtc_transceiver_ = std::move(other.webrtc_transceiver_);
  is_initialized_ = other.is_initialized_;
  sender_state_ = std::move(other.sender_state_);
  receiver_state_ = std::move(other.receiver_state_);
  mid_ = std::move(other.mid_);
  direction_ = std::move(other.direction_);
  current_direction_ = std::move(other.current_direction_);
  fired_direction_ = std::move(other.fired_direction_);
  header_extensions_negotiated_ =
      std::move(other.header_extensions_negotiated_);

  return *this;
}

bool RtpTransceiverState::is_initialized() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return is_initialized_;
}

void RtpTransceiverState::Initialize() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (sender_state_)
    sender_state_->Initialize();
  if (receiver_state_)
    receiver_state_->Initialize();
  is_initialized_ = true;
}

scoped_refptr<base::SingleThreadTaskRunner>
RtpTransceiverState::main_task_runner() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return main_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RtpTransceiverState::signaling_task_runner() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return signaling_task_runner_;
}

scoped_refptr<webrtc::RtpTransceiverInterface>
RtpTransceiverState::webrtc_transceiver() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return webrtc_transceiver_;
}

const std::optional<blink::RtpSenderState>& RtpTransceiverState::sender_state()
    const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return sender_state_;
}

blink::RtpSenderState RtpTransceiverState::MoveSenderState() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  std::optional<blink::RtpSenderState> temp(std::nullopt);
  sender_state_.swap(temp);
  return *std::move(temp);
}

const std::optional<blink::RtpReceiverState>&
RtpTransceiverState::receiver_state() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return receiver_state_;
}

blink::RtpReceiverState RtpTransceiverState::MoveReceiverState() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  std::optional<blink::RtpReceiverState> temp(std::nullopt);
  receiver_state_.swap(temp);
  return *std::move(temp);
}

std::optional<std::string> RtpTransceiverState::mid() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return mid_;
}

webrtc::RtpTransceiverDirection RtpTransceiverState::direction() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return direction_;
}

void RtpTransceiverState::set_direction(
    webrtc::RtpTransceiverDirection direction) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  direction_ = direction;
}

std::optional<webrtc::RtpTransceiverDirection>
RtpTransceiverState::current_direction() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return current_direction_;
}

std::optional<webrtc::RtpTransceiverDirection>
RtpTransceiverState::fired_direction() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return fired_direction_;
}

const Vector<webrtc::RtpHeaderExtensionCapability>&
RtpTransceiverState::header_extensions_negotiated() const {
  return header_extensions_negotiated_;
}

class RTCRtpTransceiverImpl::RTCRtpTransceiverInternal
    : public WTF::ThreadSafeRefCounted<
          RTCRtpTransceiverImpl::RTCRtpTransceiverInternal,
          RTCRtpTransceiverImpl::RTCRtpTransceiverInternalTraits> {
 public:
  RTCRtpTransceiverInternal(
      rtc::scoped_refptr<webrtc::PeerConnectionInterface>
          native_peer_connection,
      scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap> track_map,
      RtpTransceiverState state,
      bool require_encoded_insertable_streams,
      std::unique_ptr<webrtc::Metronome> decode_metronome)
      : main_task_runner_(state.main_task_runner()),
        signaling_task_runner_(state.signaling_task_runner()),
        webrtc_transceiver_(state.webrtc_transceiver()),
        state_(std::move(state)) {
    sender_ = std::make_unique<blink::RTCRtpSenderImpl>(
        native_peer_connection, track_map, state_.MoveSenderState(),
        require_encoded_insertable_streams);
    receiver_ = std::make_unique<blink::RTCRtpReceiverImpl>(
        native_peer_connection, state_.MoveReceiverState(),
        require_encoded_insertable_streams, std::move(decode_metronome));
  }

  const RtpTransceiverState& state() const {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    return state_;
  }

  void set_state(RtpTransceiverState state,
                 TransceiverStateUpdateMode update_mode) {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    DCHECK_EQ(state.main_task_runner(), main_task_runner_);
    DCHECK_EQ(state.signaling_task_runner(), signaling_task_runner_);
    DCHECK(state.webrtc_transceiver() == webrtc_transceiver_);
    DCHECK(state.is_initialized());
    auto previous_direction = state_.direction();
    state_ = std::move(state);
    auto sender_state = state_.MoveSenderState();
    if (update_mode == TransceiverStateUpdateMode::kSetDescription) {
      // setLocalDescription() and setRemoteDescription() cannot modify
      // "sender.track" so this part of the state information is either
      // identical to the current state or out-dated information. Surfacing
      // out-dated information has caused crashes and other problems,
      // see https://crbug.com/950280.
      sender_state.set_track_ref(sender_->state().track_ref()
                                     ? sender_->state().track_ref()->Copy()
                                     : nullptr);
      // The direction attribute is normally controlled by the JavaScript layer
      // and we want to keep `previous_state` to avoid getting out-of-sync.
      // There is one exception to this though: setting SDP can make it
      // permanently stopped and must be surfaced.
      if (state_.direction() != webrtc::RtpTransceiverDirection::kStopped) {
        state_.set_direction(previous_direction);
      }
    }
    sender_->set_state(std::move(sender_state));
    receiver_->set_state(state_.MoveReceiverState());
  }

  blink::RTCRtpSenderImpl* content_sender() {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    return sender_.get();
  }

  blink::RTCRtpReceiverImpl* content_receiver() {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    return receiver_.get();
  }

  webrtc::RTCError SetDirection(webrtc::RtpTransceiverDirection direction) {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    // This implicitly performs a blocking invoke on the webrtc signaling thread
    // due to use of PROXY references for |webrtc_transceiver_|.
    auto error = webrtc_transceiver_->SetDirectionWithError(direction);
    if (error.ok()) {
      state_.set_direction(webrtc_transceiver_->direction());
    }
    return error;
  }

  webrtc::RTCError Stop() {
    auto error = webrtc_transceiver_->StopStandard();
    if (error.ok()) {
      state_.set_direction(webrtc::RtpTransceiverDirection::kStopped);
    }
    return error;
  }

  webrtc::RTCError setCodecPreferences(
      std::vector<webrtc::RtpCodecCapability> codec_preferences) {
    return webrtc_transceiver_->SetCodecPreferences(codec_preferences);
  }

  webrtc::RTCError SetHeaderExtensionsToNegotiate(
      std::vector<webrtc::RtpHeaderExtensionCapability> header_extensions) {
    return webrtc_transceiver_->SetHeaderExtensionsToNegotiate(
        header_extensions);
  }

  Vector<webrtc::RtpHeaderExtensionCapability> GetNegotiatedHeaderExtensions()
      const {
    return state_.header_extensions_negotiated();
  }

  std::vector<webrtc::RtpHeaderExtensionCapability>
  GetHeaderExtensionsToNegotiate() const {
    return webrtc_transceiver_->GetHeaderExtensionsToNegotiate();
  }

 private:
  friend class WTF::ThreadSafeRefCounted<RTCRtpTransceiverInternal,
                                         RTCRtpTransceiverInternalTraits>;
  friend struct RTCRtpTransceiverImpl::RTCRtpTransceiverInternalTraits;

  ~RTCRtpTransceiverInternal() {
    // Ensured by destructor traits.
    DCHECK(main_task_runner_->BelongsToCurrentThread());
  }

  // Task runners and webrtc transceiver: Same information as stored in |state_|
  // but const and safe to touch on the signaling thread to avoid race with
  // set_state().
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner_;
  const scoped_refptr<webrtc::RtpTransceiverInterface> webrtc_transceiver_;
  RtpTransceiverState state_;
  std::unique_ptr<blink::RTCRtpSenderImpl> sender_;
  std::unique_ptr<blink::RTCRtpReceiverImpl> receiver_;
};

struct RTCRtpTransceiverImpl::RTCRtpTransceiverInternalTraits {
  static void Destruct(const RTCRtpTransceiverInternal* transceiver) {
    // RTCRtpTransceiverInternal owns AdapterRefs which have to be destroyed on
    // the main thread, this ensures delete always happens there.
    if (!transceiver->main_task_runner_->BelongsToCurrentThread()) {
      transceiver->main_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &RTCRtpTransceiverImpl::RTCRtpTransceiverInternalTraits::Destruct,
              base::Unretained(transceiver)));
      return;
    }
    delete transceiver;
  }
};

uintptr_t RTCRtpTransceiverImpl::GetId(
    const webrtc::RtpTransceiverInterface* webrtc_transceiver) {
  return reinterpret_cast<uintptr_t>(webrtc_transceiver);
}

RTCRtpTransceiverImpl::RTCRtpTransceiverImpl(
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection,
    scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap> track_map,
    RtpTransceiverState transceiver_state,
    bool encoded_insertable_streams,
    std::unique_ptr<webrtc::Metronome> decode_metronome)
    : internal_(base::MakeRefCounted<RTCRtpTransceiverInternal>(
          std::move(native_peer_connection),
          std::move(track_map),
          std::move(transceiver_state),
          encoded_insertable_streams,
          std::move(decode_metronome))) {}

RTCRtpTransceiverImpl::RTCRtpTransceiverImpl(const RTCRtpTransceiverImpl& other)
    : internal_(other.internal_) {}

RTCRtpTransceiverImpl::~RTCRtpTransceiverImpl() {}

RTCRtpTransceiverImpl& RTCRtpTransceiverImpl::operator=(
    const RTCRtpTransceiverImpl& other) {
  internal_ = other.internal_;
  return *this;
}

std::unique_ptr<RTCRtpTransceiverImpl> RTCRtpTransceiverImpl::ShallowCopy()
    const {
  return std::make_unique<RTCRtpTransceiverImpl>(*this);
}

const RtpTransceiverState& RTCRtpTransceiverImpl::state() const {
  return internal_->state();
}

blink::RTCRtpSenderImpl* RTCRtpTransceiverImpl::content_sender() {
  return internal_->content_sender();
}

blink::RTCRtpReceiverImpl* RTCRtpTransceiverImpl::content_receiver() {
  return internal_->content_receiver();
}

void RTCRtpTransceiverImpl::set_state(RtpTransceiverState transceiver_state,
                                      TransceiverStateUpdateMode update_mode) {
  internal_->set_state(std::move(transceiver_state), update_mode);
}

uintptr_t RTCRtpTransceiverImpl::Id() const {
  return GetId(internal_->state().webrtc_transceiver().get());
}

String RTCRtpTransceiverImpl::Mid() const {
  const auto& mid = internal_->state().mid();
  return mid ? String::FromUTF8(*mid) : String();
}

std::unique_ptr<blink::RTCRtpSenderPlatform> RTCRtpTransceiverImpl::Sender()
    const {
  return internal_->content_sender()->ShallowCopy();
}

std::unique_ptr<RTCRtpReceiverPlatform> RTCRtpTransceiverImpl::Receiver()
    const {
  return internal_->content_receiver()->ShallowCopy();
}

webrtc::RtpTransceiverDirection RTCRtpTransceiverImpl::Direction() const {
  return internal_->state().direction();
}

webrtc::RTCError RTCRtpTransceiverImpl::SetDirection(
    webrtc::RtpTransceiverDirection direction) {
  return internal_->SetDirection(direction);
}

std::optional<webrtc::RtpTransceiverDirection>
RTCRtpTransceiverImpl::CurrentDirection() const {
  return internal_->state().current_direction();
}

std::optional<webrtc::RtpTransceiverDirection>
RTCRtpTransceiverImpl::FiredDirection() const {
  return internal_->state().fired_direction();
}

webrtc::RTCError RTCRtpTransceiverImpl::Stop() {
  return internal_->Stop();
}

webrtc::RTCError RTCRtpTransceiverImpl::SetCodecPreferences(
    Vector<webrtc::RtpCodecCapability> codec_preferences) {
  std::vector<webrtc::RtpCodecCapability> std_codec_preferences(
      codec_preferences.size());
  std::move(codec_preferences.begin(), codec_preferences.end(),
            std_codec_preferences.begin());
  return internal_->setCodecPreferences(std_codec_preferences);
}

webrtc::RTCError RTCRtpTransceiverImpl::SetHeaderExtensionsToNegotiate(
    Vector<webrtc::RtpHeaderExtensionCapability> header_extensions) {
  std::vector<webrtc::RtpHeaderExtensionCapability> std_header_extensions;
  std::move(header_extensions.begin(), header_extensions.end(),
            std::back_inserter(std_header_extensions));
  return internal_->SetHeaderExtensionsToNegotiate(std_header_extensions);
}

Vector<webrtc::RtpHeaderExtensionCapability>
RTCRtpTransceiverImpl::GetNegotiatedHeaderExtensions() const {
  return internal_->GetNegotiatedHeaderExtensions();
}

Vector<webrtc::RtpHeaderExtensionCapability>
RTCRtpTransceiverImpl::GetHeaderExtensionsToNegotiate() const {
  auto std_extensions = internal_->GetHeaderExtensionsToNegotiate();
  Vector<webrtc::RtpHeaderExtensionCapability> extensions;
  std::move(std_extensions.begin(), std_extensions.end(),
            std::back_inserter(extensions));
  return extensions;
}

}  // namespace blink
