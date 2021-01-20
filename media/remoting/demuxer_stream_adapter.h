// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_DEMUXER_STREAM_ADAPTER_H_
#define MEDIA_REMOTING_DEMUXER_STREAM_ADAPTER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/common/mojo_data_pipe_read_write.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/remoting/rpc_broker.h"
#include "media/remoting/triggers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class DemuxerStream;

namespace remoting {

// Class to fetch audio/video buffer from demuxer and send it to browser process
// via mojo::Remoting interface. Note the class is created and run on media
// thread using |media_task_runner|, Mojo data pipe should run on media thread,
// while RPC message should be sent on main thread using |main_task_runner|.
class DemuxerStreamAdapter {
 public:
  using ErrorCallback = base::OnceCallback<void(StopTrigger)>;

  // |main_task_runner|: Task runner to post RPC message on main thread
  // |media_task_runner|: Task runner to run whole class on media thread.
  // |name|: Demuxer stream name. For troubleshooting purposes.
  // |demuxer_stream|: Demuxer component.
  // |rpc_broker|: Broker class to handle incoming and outgoing RPC message. It
  //               is used only on the main thread.
  // |rpc_handle|: Unique value that references this DemuxerStreamAdapter.
  // |stream_sender_remote|: Transfer of pipe binding on the media thread. It is
  //                         to access mojo remote for sending data stream.
  // |producer_handle|: handle to send data using mojo data pipe.
  // |error_callback|: Run if a fatal runtime error occurs and remoting should
  //                   be shut down.
  DemuxerStreamAdapter(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      const std::string& name,
      DemuxerStream* demuxer_stream,
      const base::WeakPtr<RpcBroker>& rpc_broker,
      int rpc_handle,
      mojo::PendingRemote<mojom::RemotingDataStreamSender> stream_sender_remote,
      mojo::ScopedDataPipeProducerHandle producer_handle,
      ErrorCallback error_callback);
  ~DemuxerStreamAdapter();

  // Rpc handle for this class. This is used for sending/receiving RPC message
  // with specific hanle using Rpcbroker.
  int rpc_handle() const { return rpc_handle_; }

  // Returns the number of bytes that have been written to the data pipe since
  // the last call to this method. This is polled periodically by
  // CourierRenderer for metrics purposes.
  int64_t GetBytesWrittenAndReset();

  // Signals if system is in flushing state. The caller uses |flushing| to
  // signal when flush starts and when is done. During flush operation, all
  // fetching data actions will be discarded. The return value indicates frame
  // count in order to signal receiver what frames are in flight before flush,
  // or base::nullopt if the flushing state was unchanged.
  base::Optional<uint32_t> SignalFlush(bool flushing);

  bool is_processing_read_request() const {
    // |read_until_callback_handle_| is set when RPC_DS_READUNTIL message is
    // received, and will be reset to invalid value after
    // RPC_DS_READUNTIL_CALLBACK is sent back to receiver. Therefore it can be
    // used to determine if the class is in the reading state or not.
    return read_until_callback_handle_ != RpcBroker::kInvalidHandle;
  }

  // Indicates whether there is data waiting to be written to the mojo data
  // pipe.
  bool is_data_pending() const { return !pending_frame_.empty(); }

  // Creates a Mojo data pipe configured appropriately for use with a
  // DemuxerStreamAdapter.
  static mojo::DataPipe* CreateDataPipe();

 private:
  friend class MockDemuxerStreamAdapter;

  // Receives RPC message from RpcBroker.
  void OnReceivedRpc(std::unique_ptr<pb::RpcMessage> message);

  // RPC message tasks.
  void Initialize(int remote_callback_handle);
  void ReadUntil(std::unique_ptr<pb::RpcMessage> message);
  void EnableBitstreamConverter();
  void RequestBuffer();
  void SendReadAck();

  // Callback function when retrieving data from demuxer.
  void OnNewBuffer(DemuxerStream::Status status,
                   scoped_refptr<DecoderBuffer> input);
  // Write the current frame into the mojo data pipe. OnFrameWritten() will be
  // called when the writing has finished.
  void WriteFrame();
  void OnFrameWritten(bool success);
  void ResetPendingFrame();

  // Callback function when a fatal runtime error occurs.
  void OnFatalError(StopTrigger stop_trigger);

  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // Name of demuxer stream. Debug only.
  const std::string name_;

  // Weak pointer of RpcBroker. It should use |main_task_runner_| to access the
  // interfaces.
  const base::WeakPtr<RpcBroker> rpc_broker_;

  // RPC handle for this demuxer stream service.
  const int rpc_handle_;

  // Demuxer stream and stream type.
  DemuxerStream* const demuxer_stream_;
  const DemuxerStream::Type type_;

  // Run by OnFatalError to propagate StopTriggers back to the
  // CourierRenderer that owns this instance. This is not-null at
  // construction time, and set to null the first time OnFatalError() is called.
  ErrorCallback error_callback_;

  // Remote RPC handle for demuxer initialization. The value is provided by
  // receiver from RPC_DS_INITIALIZE message and will be used as handle in
  // RPC_DS_INITIALIZE_CALLBACK message.
  int remote_callback_handle_;

  // Remote RPC handle for reading data from demuxer. The value is provided by
  // receiver from RPC_DS_READUNTIL message and will be used as handle in
  // RPC_DS_READUNTIL_CALLBACK message. The handle can be used to indicate
  // whether it is in reading state because not only each RPC_DS_READUNTIL
  //  message provides different callback handle, but also it is only set to
  // valid handle while in reading state.
  int read_until_callback_handle_;

  // Current frame count issued by RPC_DS_READUNTIL RPC message. It should send
  // all frame data with count id smaller than |read_until_count_| before
  // sending RPC_DS_READUNTIL_CALLBACK back to receiver.
  uint32_t read_until_count_;

  // Count id of last frame sent.
  uint32_t last_count_;

  // Flag to indicate if it's on flushing operation. All actions should be
  // aborted and data should be discarded when the value is true.
  bool pending_flush_;

  // Frame buffer and its information that is currently in process of writing to
  // Mojo data pipe.
  std::vector<uint8_t> pending_frame_;
  bool pending_frame_is_eos_;

  // Keeps latest demuxer stream status and audio/video decoder config.
  DemuxerStream::Status media_status_;
  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;

  mojo::Remote<mojom::RemotingDataStreamSender> stream_sender_;
  MojoDataPipeWriter data_pipe_writer_;

  // Tracks the number of bytes written to the pipe.
  int64_t bytes_written_to_pipe_;

  // WeakPtrFactory only for reading buffer from demuxer stream. This is used
  // for canceling all read callbacks provided to the |demuxer_stream_| before a
  // flush.
  base::WeakPtrFactory<DemuxerStreamAdapter> request_buffer_weak_factory_{this};
  // WeakPtrFactory for normal usage.
  base::WeakPtrFactory<DemuxerStreamAdapter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DemuxerStreamAdapter);
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_DEMUXER_STREAM_ADAPTER_H_
