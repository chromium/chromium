// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_STREAM_PROVIDER_H_
#define MEDIA_REMOTING_STREAM_PROVIDER_H_

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/remoting/media_remoting_rpc.pb.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

class MojoDecoderBufferReader;

namespace remoting {

class ReceiverController;
class RpcBroker;

// The media stream provider for Media Remoting receiver.
class StreamProvider final : public Demuxer {
 public:
  StreamProvider(
      ReceiverController* receiver_controller,
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner);

  // Demuxer implementation.
  std::vector<DemuxerStream*> GetAllStreams() override;
  std::string GetDisplayName() const override;
  void Initialize(DemuxerHost* host, PipelineStatusCallback status_cb) override;
  void AbortPendingReads() override;
  void StartWaitingForSeek(base::TimeDelta seek_time) override;
  void CancelPendingSeek(base::TimeDelta seek_time) override;
  void Seek(base::TimeDelta time, PipelineStatusCallback status_cb) override;
  void Stop() override;
  base::TimeDelta GetStartTime() const override;
  base::Time GetTimelineOffset() const override;
  int64_t GetMemoryUsage() const override;
  base::Optional<container_names::MediaContainerName> GetContainerForMetrics()
      const override;
  void OnEnabledAudioTracksChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;
  void OnSelectedVideoTrackChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;

 protected:
  // Deletion is only allowed via Destroy().
  ~StreamProvider() override;

 private:
  // An implementation of media::DemuxerStream on Media Remoting receiver.
  // Receives data from mojo data pipe, and returns one frame or/and status when
  // Read() is called.
  class MediaStream final : public DemuxerStream,
                            public mojom::RemotingDataStreamReceiver {
   public:
    using UniquePtr =
        std::unique_ptr<MediaStream, std::function<void(MediaStream*)>>;

    // MediaStream should be created on the main thread to be able to get unique
    // handle ID from |rpc_broker_|.
    static void CreateOnMainThread(
        RpcBroker* rpc_broker,
        Type type,
        int32_t handle,
        const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
        base::OnceCallback<void(MediaStream::UniquePtr)> callback);

    // In order to destroy members in the right thread, MediaStream has to use
    // DestructionHelper() to destroy itself.
    static void DestructionHelper(MediaStream* stream);

    MediaStream(
        RpcBroker* rpc_broker,
        Type type,
        int32_t remote_handle,
        const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner);

    // DemuxerStream implementation.
    void Read(ReadCB read_cb) override;
    AudioDecoderConfig audio_decoder_config() override;
    VideoDecoderConfig video_decoder_config() override;
    DemuxerStream::Type type() const override;
    Liveness liveness() const override;
    bool SupportsConfigChanges() override;

    void Initialize(base::OnceClosure init_done_cb);

    mojo::PendingRemote<mojom::RemotingDataStreamReceiver>
    BindNewPipeAndPassRemote() {
      return receiver_.BindNewPipeAndPassRemote();
    }

   private:
    friend class base::DeleteHelper<MediaStream>;  // For using DeleteSoon().
    // For testing.
    friend class StreamProviderTest;

    // Prevent from unique_ptr using ~MediaStream() to destroy MediaStream
    // instances. Use DestructionHelper() as the custom deleter with unique_ptr
    // to destroy MediaStream instances.
    ~MediaStream() override;

    void Destroy();

    // Send RPC message on |main_task_runner_|.
    void SendRpcMessageOnMainThread(std::unique_ptr<pb::RpcMessage> message);

    // mojom::RemotingDataStreamReceiver implementation.
    void InitializeDataPipe(
        mojo::ScopedDataPipeConsumerHandle data_pipe) override;
    void ReceiveFrame(uint32_t count, mojom::DecoderBufferPtr buffer) override;
    void FlushUntil(uint32_t count) override;

    // RPC messages handlers.
    void OnReceivedRpc(std::unique_ptr<pb::RpcMessage> message);
    void OnInitializeCallback(std::unique_ptr<pb::RpcMessage> message);
    void OnReadUntilCallback(std::unique_ptr<pb::RpcMessage> message);

    // Issues the ReadUntil RPC message when read is pending and buffer is
    // empty.
    void SendReadUntil();

    // Run |init_done_callback_| when MojoDecoderBufferReader is created and
    // received RPC_DS_INITIALIZE_CALLBACK
    void CompleteInitialize();

    // Append a frame into |buffers_|.
    void AppendBuffer(uint32_t count, scoped_refptr<DecoderBuffer> buffer);

    // Run and reset the read callback.
    void CompleteRead(DemuxerStream::Status status);

    // Update the audio/video decoder config. When config changes in the mid
    // stream, the new config will be stored in |next_audio_decoder_config_|.
    // Old config will be dropped when all associated frames are consumed.
    void UpdateAudioConfig(const pb::AudioDecoderConfig& audio_message);
    void UpdateVideoConfig(const pb::VideoDecoderConfig& video_message);

    // Called when any error occurs.
    void OnError(const std::string& error);

    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

    RpcBroker* const rpc_broker_;  // Outlives this class.
    const Type type_;
    const int remote_handle_;
    const int rpc_handle_;

    // Set when Initialize() is called.
    base::OnceClosure init_done_callback_;

    // The frame count of the frame to be returned on the next Read call. It
    // will be increased whenever a frame is read. It will be updated when
    // FlushUntil() is called.
    uint32_t current_frame_count_ = 0;

    // One plus the last frame count received over RTP. Used for continuity
    // check.
    uint32_t buffered_frame_count_ = 0;

    // The total number of frames received from the sender side. It will be used
    // as the base value for sending ReadUntil() to request more frames and be
    // updated in OnReadUntilCallback() which would get the message that
    // contains how many frames are sent.
    uint32_t total_received_frame_count_ = 0;

    // Indicates whether a ReadUntil RPC message was sent without receiving the
    // ReadUntilCallback message yet.
    bool read_until_sent_ = false;

    // Indicates whether RPC_DS_INITIALIZE_CALLBACK received.
    bool rpc_initialized_ = false;

    // Set when Read() is called. Run only once when read completes.
    ReadCB read_complete_callback_;

    // The frame data would be sent via Mojo IPC as MojoDecoderBuffer. When a
    // frame is sent to |this| from host by calling ReceiveFrame(),
    // |decoder_buffer_reader_| is used to read the frame date from data pipe.
    std::unique_ptr<MojoDecoderBufferReader> decoder_buffer_reader_;

    base::circular_deque<scoped_refptr<DecoderBuffer>> buffers_;

    // Current audio/video config.
    AudioDecoderConfig audio_decoder_config_;
    VideoDecoderConfig video_decoder_config_;

    // Stores the new audio/video config when config changes.
    AudioDecoderConfig next_audio_decoder_config_;
    VideoDecoderConfig next_video_decoder_config_;

    mojo::Receiver<mojom::RemotingDataStreamReceiver> receiver_{this};

    base::WeakPtr<MediaStream> media_weak_this_;
    base::WeakPtrFactory<MediaStream> media_weak_factory_{this};
  };

  friend std::default_delete<StreamProvider>;
  friend class base::DeleteHelper<StreamProvider>;  // For using DeleteSoon().

  // For testing.
  friend class StreamProviderTest;

  void Destroy();

  // RPC messages handlers.
  void OnReceivedRpc(std::unique_ptr<pb::RpcMessage> message);
  void OnAcquireDemuxer(std::unique_ptr<pb::RpcMessage> message);

  // Called when audio/video stream is created and initialized.
  void InitializeDataPipe();
  void OnAudioStreamCreated(MediaStream::UniquePtr stream);
  void OnVideoStreamCreated(MediaStream::UniquePtr stream);
  void OnAudioStreamInitialized();
  void OnVideoStreamInitialized();
  void CompleteInitialize();

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  ReceiverController* const receiver_controller_;  // Outlives this class
  RpcBroker* const rpc_broker_;                    // Outlives this class
  MediaStream::UniquePtr audio_stream_;
  MediaStream::UniquePtr video_stream_;
  bool has_audio_{false};
  bool has_video_{false};
  bool audio_stream_initialized_{false};
  bool video_stream_initialized_{false};

  // Set when Initialize() is called, and will run when both video and audio
  // streams are initialized or error occurs.
  PipelineStatusCallback init_done_callback_;

  base::WeakPtr<StreamProvider> media_weak_this_;
  base::WeakPtrFactory<StreamProvider> media_weak_factory_{this};
};

}  // namespace remoting
}  // namespace media

namespace std {

// Specialize std::default_delete to call Destroy().
template <>
struct default_delete<media::remoting::StreamProvider> {
  constexpr default_delete() = default;

  template <typename U,
            typename = typename std::enable_if<std::is_convertible<
                U*,
                media::remoting::StreamProvider*>::value>::type>
  explicit default_delete(const default_delete<U>& d) {}

  void operator()(media::remoting::StreamProvider* ptr) const;
};

}  // namespace std

#endif  // MEDIA_REMOTING_STREAM_PROVIDER_H_
