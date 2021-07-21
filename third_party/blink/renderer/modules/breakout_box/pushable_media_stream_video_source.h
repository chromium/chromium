// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_PUSHABLE_MEDIA_STREAM_VIDEO_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_PUSHABLE_MEDIA_STREAM_VIDEO_SOURCE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

// Simplifies the creation of video tracks.  Just do this:
// auto source = std::make_unique<PushableMediaStreamVideoSource>();
// auto* track = CreateVideoTrackFromSource(script_state, source);
// for each frame:
//   source->PushFrame(video_frame, capture_time);
// source->Stop();
class MODULES_EXPORT PushableMediaStreamVideoSource
    : public MediaStreamVideoSource {
 public:
  // Helper class that facilitates interacting with a
  // PushableMediaStreamVideoSource from multiple threads. This also includes
  // safely posting tasks to/from outside the main thread.
  // The public methods of this class can be called on any thread.
  class MODULES_EXPORT Broker : public WTF::ThreadSafeRefCounted<Broker> {
   public:
    Broker(const Broker&) = delete;
    Broker& operator=(const Broker&) = delete;

    // Increases the count of connected clients.
    void OnClientStarted();
    // Decreases the count of connected clients. If the count reaches zero,
    // StopSource() is called.
    void OnClientStopped();
    bool IsRunning();
    void PushFrame(scoped_refptr<media::VideoFrame> video_frame,
                   base::TimeTicks estimated_capture_time);
    void StopSource();

   private:
    friend class PushableMediaStreamVideoSource;

    explicit Broker(PushableMediaStreamVideoSource* source);

    // These functions must be called on |main_task_runner_|.
    void OnSourceStarted(VideoCaptureDeliverFrameCB);
    void OnSourceDestroyedOrStopped();
    void StopSourceOnMain();

    WTF::Mutex mutex_;
    // |source_| can only change its value on |main_task_runner_|. We use
    // |mutex_| to guard it for value changes and for reads outside
    // |main_task_runner_|. It is not necessary to guard it with |mutex_| to
    // read its value on |main_task_runner_|. This helps avoid deadlocks in
    // Stop()/OnSourceDestroyedOrStopped() interactions.
    PushableMediaStreamVideoSource* source_;
    // The same apples to |frame_callback_|, but since it does not have
    // complex interactions with owners, like |source_| does, we always guard
    // it for simplicity.
    VideoCaptureDeliverFrameCB frame_callback_ GUARDED_BY(mutex_);
    int num_clients_ GUARDED_BY(mutex_) = 0;
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  };

  explicit PushableMediaStreamVideoSource(
      scoped_refptr<base::SingleThreadTaskRunner>);
  ~PushableMediaStreamVideoSource() override;

  // See the definition of VideoCaptureDeliverFrameCB in
  // media/capture/video_capturer_source.h for the documentation
  // of |estimated_capture_time| and the difference with
  // media::VideoFrame::timestamp().
  // This function can be called on any thread.
  void PushFrame(scoped_refptr<media::VideoFrame> video_frame,
                 base::TimeTicks estimated_capture_time);

  // These functions can be called on any thread.
  bool IsRunning() const { return broker_->IsRunning(); }
  scoped_refptr<Broker> GetBroker() const { return broker_; }

  // MediaStreamVideoSource
  void StartSourceImpl(VideoCaptureDeliverFrameCB frame_callback,
                       EncodedVideoFrameCB encoded_frame_callback) override;
  void StopSourceImpl() override;
  base::WeakPtr<MediaStreamVideoSource> GetWeakPtr() const override;

 private:
  scoped_refptr<Broker> broker_;
  base::WeakPtrFactory<MediaStreamVideoSource> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_PUSHABLE_MEDIA_STREAM_VIDEO_SOURCE_H_
