// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_RENDERER_CONTROLLER_H_
#define MEDIA_REMOTING_RENDERER_CONTROLLER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "media/base/media_observer.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/mojo/mojom/remoting_common.mojom.h"
#include "media/remoting/metrics.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
#include "media/remoting/rpc_broker.h"  // nogncheck
#endif

namespace base {
class TickClock;
}

namespace media {

namespace remoting {

// This class monitors player events as a MediaObserver and may trigger the
// switch of the media renderer between local playback and remoting.
class RendererController final : public mojom::RemotingSource,
                                 public MediaObserver {
 public:
  RendererController(
      mojo::PendingReceiver<mojom::RemotingSource> source_receiver,
      mojo::PendingRemote<mojom::Remoter> remoter);
  ~RendererController() override;

  // mojom::RemotingSource implementations.
  void OnSinkAvailable(mojom::RemotingSinkMetadataPtr metadata) override;
  void OnSinkGone() override;
  void OnStarted() override;
  void OnStartFailed(mojom::RemotingStartFailReason reason) override;
  void OnMessageFromSink(const std::vector<uint8_t>& message) override;
  void OnStopped(mojom::RemotingStopReason reason) override;

  // MediaObserver implementation.
  void OnBecameDominantVisibleContent(bool is_dominant) override;
  void OnMetadataChanged(const PipelineMetadata& metadata) override;
  void OnRemotePlaybackDisabled(bool disabled) override;
  void OnPlaying() override;
  void OnPaused() override;
  void OnDataSourceInitialized(const GURL& url_after_redirects) override;
  void OnHlsManifestDetected() override;
  void SetClient(MediaObserverClient* client) override;

  base::WeakPtr<RendererController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Used by AdaptiveRendererFactory to query whether to create a Media
  // Remoting Renderer.
  bool remote_rendering_started() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return remote_rendering_started_;
  }

  using DataPipeStartCallback = base::OnceCallback<void(
      mojo::PendingRemote<mojom::RemotingDataStreamSender> audio,
      mojo::PendingRemote<mojom::RemotingDataStreamSender> video,
      mojo::ScopedDataPipeProducerHandle audio_handle,
      mojo::ScopedDataPipeProducerHandle video_handle)>;
  void StartDataPipe(std::unique_ptr<mojo::DataPipe> audio_data_pipe,
                     std::unique_ptr<mojo::DataPipe> video_data_pipe,
                     DataPipeStartCallback done_callback);

#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
  base::WeakPtr<RpcBroker> GetRpcBroker();
#endif

  // Called by CourierRenderer when it encountered a fatal error. This will
  // cause remoting to shut down. Media remoting might be re-tried after the
  // media element stops and re-starts being the dominant visible content in the
  // tab.
  void OnRendererFatalError(StopTrigger stop_trigger);

 private:
  friend class RendererControllerTest;

  bool has_audio() const {
    return pipeline_metadata_.has_audio &&
           pipeline_metadata_.audio_decoder_config.IsValidConfig();
  }

  bool has_video() const {
    return pipeline_metadata_.has_video &&
           pipeline_metadata_.video_decoder_config.IsValidConfig();
  }

  // Called when the session availability state may have changed. Each call to
  // this method could cause a remoting session to be started or stopped; and if
  // that happens, the |start_trigger| or |stop_trigger| must be the reason.
  void UpdateFromSessionState(StartTrigger start_trigger,
                              StopTrigger stop_trigger);

  bool IsVideoCodecSupported() const;
  bool IsAudioCodecSupported() const;
  bool IsAudioOrVideoSupported() const;

  // Returns true if all of the technical requirements for the media pipeline
  // and remote rendering are being met. This does not include environmental
  // conditions, such as the content being dominant in the viewport, available
  // network bandwidth, etc.
  bool CanBeRemoting() const;

  // Determines whether to enter or leave Remoting mode and switches if
  // necessary. Each call to this method could cause a remoting session to be
  // started or stopped; and if that happens, the |start_trigger| or
  // |stop_trigger| must be the reason.
  void UpdateAndMaybeSwitch(StartTrigger start_trigger,
                            StopTrigger stop_trigger);

  // Activates or deactivates the remote playback monitoring based on whether
  // the element is compatible with Remote Playback API.
  void UpdateRemotePlaybackAvailabilityMonitoringState();

  // Start |delayed_start_stability_timer_| to ensure all preconditions are met
  // and held stable for a short time before starting remoting.
  void WaitForStabilityBeforeStart(StartTrigger start_trigger);
  // Cancel the start of remoting.
  void CancelDelayedStart();
  // Called when the delayed start ends. |decoded_frame_count_before_delay| is
  // the total number of frames decoded before the delayed start began.
  // |delayed_start_time| is the time that the delayed start began.
  void OnDelayedStartTimerFired(StartTrigger start_trigger,
                                unsigned decoded_frame_count_before_delay,
                                base::TimeTicks delayed_start_time);

  // Queries on remoting sink capabilities.
  bool HasVideoCapability(mojom::RemotingSinkVideoCapability capability) const;
  bool HasAudioCapability(mojom::RemotingSinkAudioCapability capability) const;
  bool HasFeatureCapability(mojom::RemotingSinkFeature capability) const;

  // Callback from RpcBroker when sending message to remote sink.
  void SendMessageToSink(std::unique_ptr<std::vector<uint8_t>> message);

#if defined(OS_ANDROID)
  bool IsAudioRemotePlaybackSupported() const;
  bool IsVideoRemotePlaybackSupported() const;
  bool IsRemotePlaybackSupported() const;
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
  // Handles dispatching of incoming and outgoing RPC messages.
  RpcBroker rpc_broker_;
#endif

  const mojo::Receiver<mojom::RemotingSource> receiver_;
  const mojo::Remote<mojom::Remoter> remoter_;

  // When the sink is available for remoting, this describes its metadata. When
  // not available, this is empty. Updated by OnSinkAvailable/Gone().
  mojom::RemotingSinkMetadata sink_metadata_;

  // Indicates whether remoting is started.
  bool remote_rendering_started_ = false;

  // Indicates whether remote playback is currently disabled. This starts out as
  // true, and should be updated at least once via a call to
  // OnRemotePlaybackDisabled() at some point in the future. A web page
  // typically sets/removes the disableRemotePlayback attribute on a
  // HTMLMediaElement to disable/enable remoting of its content. Please see the
  // Remote Playback API spec for more details:
  // https://w3c.github.io/remote-playback
  bool is_remote_playback_disabled_ = true;

  // Indicates whether video is the dominant visible content in the tab.
  bool is_dominant_content_ = false;

  // Indicates whether video is paused.
  bool is_paused_ = true;

  // Indicates whether OnRendererFatalError() has been called. This indicates
  // one of several possible problems: 1) An environmental problem such as
  // out-of-memory, insufficient network bandwidth, etc. 2) The receiver may
  // have been unable to play-out the content correctly (e.g., not capable of a
  // high frame rate at a high resolution). 3) An implementation bug. In any
  // case, once a renderer encounters a fatal error, remoting will be shut down.
  // The value gets reset after the media element stops being the dominant
  // visible content in the tab.
  bool encountered_renderer_fatal_error_ = false;

  // When this is true, remoting will never start again for the lifetime of this
  // controller.
  bool permanently_disable_remoting_ = false;

  // This is used to check all the methods are called on the current thread in
  // debug builds.
  base::ThreadChecker thread_checker_;

  // Current pipeline metadata.
  PipelineMetadata pipeline_metadata_;

  // Current data source information.
  GURL url_after_redirects_;

  bool is_hls_ = false;

  // Records session events of interest.
  SessionMetricsRecorder metrics_recorder_;

  // Not owned by this class. Can only be set once by calling SetClient().
  MediaObserverClient* client_ = nullptr;

  // When this is running, it indicates that remoting will be started later
  // when the timer gets fired. The start will be canceled if there is any
  // precondition change that does not allow for remoting duting this period.
  // TODO(xjz): Estimate whether the transmission bandwidth is sufficient to
  // remote the content while this timer is running.
  base::OneShotTimer delayed_start_stability_timer_;

  const base::TickClock* clock_;

  base::WeakPtrFactory<RendererController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RendererController);
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_RENDERER_CONTROLLER_H_
