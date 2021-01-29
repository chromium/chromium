// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_UTILITY_IN_PROCESS_RECEIVER_H_
#define MEDIA_CAST_TEST_UTILITY_IN_PROCESS_RECEIVER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/audio_bus.h"
#include "media/cast/cast_config.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/net/cast_transport_config.h"
#include "net/base/ip_endpoint.h"

namespace base {
class TimeTicks;
class WaitableEvent;
}  // namespace base

namespace net {
class IPEndPoint;
}  // namespace net

namespace media {

class VideoFrame;

namespace cast {

class CastEnvironment;
class CastReceiver;
class InProcessReceiver;

// Common base functionality for an in-process Cast receiver.  This is meant to
// be subclassed with the OnAudioFrame() and OnVideoFrame() methods implemented,
// so that the implementor can focus on what is to be done with the frames,
// rather than on the boilerplate "glue" code.
class InProcessReceiver {
 public:
  class TransportClient : public CastTransport::Client {
   public:
    explicit TransportClient(InProcessReceiver* in_process_receiver)
        : in_process_receiver_(in_process_receiver) {}

    void OnStatusChanged(CastTransportStatus status) final;
    void OnLoggingEventsReceived(
        std::unique_ptr<std::vector<FrameEvent>> frame_events,
        std::unique_ptr<std::vector<PacketEvent>> packet_events) final {}
    void ProcessRtpPacket(std::unique_ptr<Packet> packet) final;

   private:
    InProcessReceiver* in_process_receiver_;

    DISALLOW_COPY_AND_ASSIGN(TransportClient);
  };

  // Construct a receiver with the given configuration.  |remote_end_point| can
  // be left empty, if the transport should automatically mate with the first
  // remote sender it encounters.
  InProcessReceiver(const scoped_refptr<CastEnvironment>& cast_environment,
                    const net::IPEndPoint& local_end_point,
                    const net::IPEndPoint& remote_end_point,
                    const FrameReceiverConfig& audio_config,
                    const FrameReceiverConfig& video_config);

  virtual ~InProcessReceiver();

  // Convenience accessors.
  scoped_refptr<CastEnvironment> cast_env() const { return cast_environment_; }
  const FrameReceiverConfig& audio_config() const { return audio_config_; }
  const FrameReceiverConfig& video_config() const { return video_config_; }

  // Begin delivering any received audio/video frames to the OnXXXFrame()
  // methods.
  //
  // Start() and Stop() must only be called from one thread.
  virtual void Start();

  // Destroy the sub-compontents of this class.
  // After this call, it is safe to destroy this object on any thread.
  //
  // Start() and Stop() must only be called from one thread.
  virtual void Stop();

 protected:
  // To be implemented by subclasses.  These are called on the Cast MAIN thread
  // as each frame is received.
  virtual void OnAudioFrame(std::unique_ptr<AudioBus> audio_frame,
                            base::TimeTicks playout_time,
                            bool is_continuous) = 0;
  virtual void OnVideoFrame(scoped_refptr<VideoFrame> video_frame,
                            base::TimeTicks playout_time,
                            bool is_continuous) = 0;

  // Helper method that creates |transport_| and |cast_receiver_|, starts
  // |transport_| receiving, and requests the first audio/video frame.
  // Subclasses may final to provide additional start-up functionality.
  virtual void StartOnMainThread();

  // Helper method that destroys |transport_| and |cast_receiver_|.
  // Subclasses may final to provide additional start-up functionality.
  virtual void StopOnMainThread(base::WaitableEvent* event);

  // Callback for the transport to notify of status changes.  A default
  // implementation is provided here that simply logs socket errors.
  virtual void UpdateCastTransportStatus(CastTransportStatus status);

 private:
  friend class base::RefCountedThreadSafe<InProcessReceiver>;

  // CastReceiver callbacks that receive a frame and then request another.  See
  // comments for the callbacks defined in src/media/cast/cast_receiver.h for
  // argument description and semantics.
  void GotAudioFrame(std::unique_ptr<AudioBus> audio_frame,
                     base::TimeTicks playout_time,
                     bool is_continuous);
  void GotVideoFrame(scoped_refptr<VideoFrame> video_frame,
                     base::TimeTicks playout_time,
                     bool is_continuous);
  void PullNextAudioFrame();
  void PullNextVideoFrame();

  void ReceivePacket(std::unique_ptr<Packet> packet);

  const scoped_refptr<CastEnvironment> cast_environment_;
  const net::IPEndPoint local_end_point_;
  const net::IPEndPoint remote_end_point_;
  const FrameReceiverConfig audio_config_;
  const FrameReceiverConfig video_config_;

  std::unique_ptr<CastTransport> transport_;
  std::unique_ptr<CastReceiver> cast_receiver_;

  // Boolean gate to avoid stopping if stopped.
  bool stopped_ = true;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<InProcessReceiver> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InProcessReceiver);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_UTILITY_IN_PROCESS_RECEIVER_H_
