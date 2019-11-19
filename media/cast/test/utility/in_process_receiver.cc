// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/utility/in_process_receiver.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "base/values.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/udp_transport_impl.h"

using media::cast::CastTransportStatus;
using media::cast::UdpTransportImpl;

namespace media {
namespace cast {

void InProcessReceiver::TransportClient::OnStatusChanged(
    CastTransportStatus status) {
  LOG_IF(ERROR, status == media::cast::TRANSPORT_SOCKET_ERROR)
      << "Transport socket error occurred.  InProcessReceiver is likely "
         "dead.";
  VLOG(1) << "CastTransportStatus is now " << status;
}

void InProcessReceiver::TransportClient::ProcessRtpPacket(
    std::unique_ptr<Packet> packet) {
  in_process_receiver_->ReceivePacket(std::move(packet));
}

InProcessReceiver::InProcessReceiver(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const net::IPEndPoint& local_end_point,
    const net::IPEndPoint& remote_end_point,
    const FrameReceiverConfig& audio_config,
    const FrameReceiverConfig& video_config)
    : cast_environment_(cast_environment),
      local_end_point_(local_end_point),
      remote_end_point_(remote_end_point),
      audio_config_(audio_config),
      video_config_(video_config) {}

InProcessReceiver::~InProcessReceiver() {
  Stop();
}

void InProcessReceiver::Start() {
  cast_environment_->PostTask(CastEnvironment::MAIN,
                              FROM_HERE,
                              base::Bind(&InProcessReceiver::StartOnMainThread,
                                         base::Unretained(this)));
  stopped_ = false;
}

void InProcessReceiver::Stop() {
  if (stopped_) {
    return;
  }
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  if (cast_environment_->CurrentlyOn(CastEnvironment::MAIN)) {
    StopOnMainThread(&event);
  } else {
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&InProcessReceiver::StopOnMainThread,
                                           base::Unretained(this),
                                           &event));
    event.Wait();
  }
  stopped_ = true;
}

void InProcessReceiver::StopOnMainThread(base::WaitableEvent* event) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_receiver_.reset(nullptr);
  transport_.reset(nullptr);
  weak_factory_.InvalidateWeakPtrs();
  event->Signal();
}

void InProcessReceiver::UpdateCastTransportStatus(CastTransportStatus status) {
  LOG_IF(ERROR, status == media::cast::TRANSPORT_SOCKET_ERROR)
      << "Transport socket error occurred.  InProcessReceiver is likely dead.";
  VLOG(1) << "CastTransportStatus is now " << status;
}

void InProcessReceiver::StartOnMainThread() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  DCHECK(!transport_ && !cast_receiver_);

  transport_ = CastTransport::Create(
      cast_environment_->Clock(), base::TimeDelta(),
      base::WrapUnique(new InProcessReceiver::TransportClient(this)),
      std::make_unique<UdpTransportImpl>(
          cast_environment_->GetTaskRunner(CastEnvironment::MAIN),
          local_end_point_, remote_end_point_,
          base::Bind(&InProcessReceiver::UpdateCastTransportStatus,
                     base::Unretained(this))),
      cast_environment_->GetTaskRunner(CastEnvironment::MAIN));

  cast_receiver_ = CastReceiver::Create(
      cast_environment_, audio_config_, video_config_, transport_.get());

  PullNextAudioFrame();
  PullNextVideoFrame();
}

void InProcessReceiver::GotAudioFrame(std::unique_ptr<AudioBus> audio_frame,
                                      base::TimeTicks playout_time,
                                      bool is_continuous) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (audio_frame.get())
    OnAudioFrame(std::move(audio_frame), playout_time, is_continuous);
  PullNextAudioFrame();
}

void InProcessReceiver::GotVideoFrame(scoped_refptr<VideoFrame> video_frame,
                                      base::TimeTicks playout_time,
                                      bool is_continuous) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (video_frame)
    OnVideoFrame(std::move(video_frame), playout_time, is_continuous);
  PullNextVideoFrame();
}

void InProcessReceiver::PullNextAudioFrame() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_receiver_->RequestDecodedAudioFrame(
      base::Bind(&InProcessReceiver::GotAudioFrame,
                 weak_factory_.GetWeakPtr()));
}

void InProcessReceiver::PullNextVideoFrame() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_receiver_->RequestDecodedVideoFrame(base::Bind(
      &InProcessReceiver::GotVideoFrame, weak_factory_.GetWeakPtr()));
}

void InProcessReceiver::ReceivePacket(std::unique_ptr<Packet> packet) {
  // TODO(Hubbe): Make an InsertPacket method instead.
  cast_receiver_->ReceivePacket(std::move(packet));
}

}  // namespace cast
}  // namespace media
