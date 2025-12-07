// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_COURIER_RENDERER_H_
#define MEDIA_REMOTING_COURIER_RENDERER_H_

#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/remoting/metrics.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/openscreen/src/cast/streaming/public/rpc_messenger.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"
#include "third_party/openscreen/src/util/weak_ptr.h"

namespace media {

class RendererClient;
class VideoRendererSink;

namespace remoting {

class DemuxerStreamAdapter;
class RendererController;

// A media::Renderer implementation that proxies all operations to a remote
// renderer via RPCs. The CourierRenderer is instantiated by
// AdaptiveRendererFactory when media remoting is meant to take place.
class CourierRenderer final : public Renderer {
 public:
  // The whole class except for constructor and GetMediaTime() runs on
  // |media_task_runner|. The constructor and GetMediaTime() run on render main
  // thread.
  CourierRenderer(scoped_refptr<base::SequencedTaskRunner> media_task_runner,
                  const base::WeakPtr<RendererController>& controller,
                  VideoRendererSink* video_renderer_sink);

  CourierRenderer(const CourierRenderer&) = delete;
  CourierRenderer& operator=(const CourierRenderer&) = delete;

  ~CourierRenderer() final;

 private:
  // Callback when attempting to establish data pipe. The function is set to
  // static in order to post task to media thread in order to avoid threading
  // race condition.
  static void OnDataPipeCreatedOnMainThread(
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      base::WeakPtr<CourierRenderer> self,
      openscreen::WeakPtr<openscreen::cast::RpcMessenger> rpc_messenger,
      mojo::PendingRemote<mojom::RemotingDataStreamSender> audio,
      mojo::PendingRemote<mojom::RemotingDataStreamSender> video,
      mojo::ScopedDataPipeProducerHandle audio_handle,
      mojo::ScopedDataPipeProducerHandle video_handle);

  // Callback function when RPC message is received. The function is set to
  // static in order to post task to media thread in order to avoid threading
  // race condition.
  static void OnMessageReceivedOnMainThread(
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      base::WeakPtr<CourierRenderer> self,
      std::unique_ptr<openscreen::cast::RpcMessage> message);

 public:
  // media::Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) final;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) final;
  void Flush(base::OnceClosure flush_cb) final;
  void StartPlayingFrom(base::TimeDelta time) final;
  void SetPlaybackRate(double playback_rate) final;
  void SetVolume(float volume) final;
  base::TimeDelta GetMediaTime() final;
  RendererType GetRendererType() final;

 private:
  friend class CourierRendererTest;

  enum State {
    STATE_UNINITIALIZED,
    STATE_CREATE_PIPE,
    STATE_ACQUIRING,
    STATE_INITIALIZING,
    STATE_FLUSHING,
    STATE_PLAYING,
    STATE_ERROR
  };

  // Callback when attempting to establish data pipe. Runs on media thread only.
  void OnDataPipeCreated(
      mojo::PendingRemote<mojom::RemotingDataStreamSender> audio,
      mojo::PendingRemote<mojom::RemotingDataStreamSender> video,
      mojo::ScopedDataPipeProducerHandle audio_handle,
      mojo::ScopedDataPipeProducerHandle video_handle,
      int audio_rpc_handle,
      int video_rpc_handle);

  // Callback function when RPC message is received. Runs on media thread only.
  void OnReceivedRpc(std::unique_ptr<openscreen::cast::RpcMessage> message);

  // Function to post task to main thread in order to send RPC message.
  void SendRpcToRemote(std::unique_ptr<openscreen::cast::RpcMessage> message);

  // Functions when RPC message is received.
  void AcquireRendererDone(
      std::unique_ptr<openscreen::cast::RpcMessage> message);
  void InitializeCallback(
      std::unique_ptr<openscreen::cast::RpcMessage> message);
  void FlushUntilCallback();
  void OnTimeUpdate(std::unique_ptr<openscreen::cast::RpcMessage> message);
  void OnBufferingStateChange(
      std::unique_ptr<openscreen::cast::RpcMessage> message);
  void OnAudioConfigChange(
      std::unique_ptr<openscreen::cast::RpcMessage> message);
  void OnVideoConfigChange(
      std::unique_ptr<openscreen::cast::RpcMessage> message);
  void OnVideoNaturalSizeChange(
      std::unique_ptr<openscreen::cast::RpcMessage> message);
  void OnVideoOpacityChange(
      std::unique_ptr<openscreen::cast::RpcMessage> message);
  void OnStatisticsUpdate(
      std::unique_ptr<openscreen::cast::RpcMessage> message);

  // Called when |current_media_time_| is updated.
  void OnMediaTimeUpdated();

  // Called to update the |video_stats_queue_|.
  void UpdateVideoStatsQueue(int video_frames_decoded,
                             int video_frames_dropped);

  // Called to clear all recent measurements history and schedule resuming after
  // a stabilization period elapses.
  void ResetMeasurements();

  // Called when a fatal runtime error occurs. |stop_trigger| is the error code
  // handed to the RendererController.
  void OnFatalError(StopTrigger stop_trigger);

  // Called periodically to measure the data flows from the
  // DemuxerStreamAdapters and record this information in the metrics.
  void MeasureAndRecordDataRates();

  // Helper to check whether is waiting for data from the Demuxers while
  // receiver is waiting for buffering. If yes, remoting will be continued even
  // though the playback might be delayed or paused.
  bool IsWaitingForDataFromDemuxers() const;

  // Helpers to register/deregister the renderer with the RPC messenger. These
  // must be called on the media thread to dereference the weak pointer to
  // this, which if contains a valid RPC messenger pointer will result in a
  // jump to the main thread.
  void RegisterForRpcMessaging();
  void DeregisterFromRpcMessaging();

  State state_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // Current renderer playback time information.
  base::TimeDelta current_media_time_;
  base::TimeDelta current_max_time_;
  // Both |current_media_time_| and |current_max_time_| should be protected by
  // lock because it can be accessed from both media and render main thread.
  base::Lock time_lock_;

  raw_ptr<MediaResource> media_resource_;
  raw_ptr<RendererClient> client_;
  std::unique_ptr<DemuxerStreamAdapter> audio_demuxer_stream_adapter_;
  std::unique_ptr<DemuxerStreamAdapter> video_demuxer_stream_adapter_;

  // Component to establish mojo remoting service on browser process.
  const base::WeakPtr<RendererController> controller_;

  // Broker class to process incoming and outgoing RPC messages.
  // Only accessed on |main_task_runner_|. NOTE: the messenger is wrapped
  // in an |openscreen::WeakPtr| instead of |base|'s implementation due to
  // it being defined in the third_party/openscreen repository.
  const openscreen::WeakPtr<openscreen::cast::RpcMessenger> rpc_messenger_;

  // RPC handle value for CourierRenderer component.
  const int rpc_handle_;

  // RPC handle value for render on receiver endpoint.
  int remote_renderer_handle_;

  // Callbacks.
  PipelineStatusCallback init_workflow_done_callback_;
  base::OnceClosure flush_cb_;

  const raw_ptr<VideoRendererSink>
      video_renderer_sink_;  // Outlives this class.

  // Current playback rate.
  double playback_rate_ = 0;

  // Current volume.
  float volume_ = 1.0f;

  // Ignores updates until this time.
  base::TimeTicks ignore_updates_until_time_;

  // Indicates whether stats has been updated.
  bool stats_updated_ = false;

  // Stores all |current_media_time_| and the local time when updated in the
  // moving time window. This is used to check whether the playback duration
  // matches the update duration in the window.
  base::circular_deque<std::pair<base::TimeTicks, base::TimeDelta>>
      media_time_queue_;

  // Stores all updates on the number of video frames decoded/dropped, and the
  // local time when updated in the moving time window. This is used to check
  // whether too many video frames were dropped.
  base::circular_deque<std::tuple<base::TimeTicks, int, int>>
      video_stats_queue_;

  // The total number of frames decoded/dropped in the time window.
  int sum_video_frames_decoded_ = 0;
  int sum_video_frames_dropped_ = 0;

  // Records the number of consecutive times that remoting playback was delayed.
  int times_playback_delayed_ = 0;

  // Records events and measurements of interest.
  RendererMetricsRecorder metrics_recorder_;

  raw_ptr<const base::TickClock> clock_;

  // A timer that polls the DemuxerStreamAdapters periodically to measure
  // the data flow rates for metrics.
  base::RepeatingTimer data_flow_poll_timer_;

  // Indicates whether is waiting for data from the Demuxers while receiver
  // reported buffer underflow.
  bool receiver_is_blocked_on_local_demuxers_ = true;

  base::WeakPtrFactory<CourierRenderer> weak_factory_{this};
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_COURIER_RENDERER_H_
