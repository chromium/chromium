// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTING_RENDERER_FACTORY_H_
#define MEDIA_REMOTING_REMOTING_RENDERER_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/renderer_factory.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/openscreen/src/cast/streaming/public/rpc_messenger.h"

namespace media {
namespace remoting {

class Receiver;
class ReceiverController;

// RemotingRendererFactory is created, called (CreateRenderer), and destroyed on
// the renderer main sequence. The Receiver (Renderer) instances it creates are
// operated and destroyed on `media_task_runner_`.
class RemotingRendererFactory : public RendererFactory {
 public:
  RemotingRendererFactory(
      mojo::PendingRemote<mojom::Remotee> remotee,
      std::unique_ptr<RendererFactory> renderer_factory,
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner);
  ~RemotingRendererFactory() override;

  // RendererFactory implementation.
  // Note: Per PipelineImpl::AsyncCreateRenderer, CreateRenderer() is always
  // invoked on the main sequence, but the returned Renderer will run on
  // `media_task_runner`.
  std::unique_ptr<Renderer> CreateRenderer(
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      AudioRendererSink* audio_renderer_sink,
      VideoRendererSink* video_renderer_sink,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) override;

 private:
  // Callback function when RPC message is received.
  void OnReceivedRpc(std::unique_ptr<openscreen::cast::RpcMessage> message);
  void OnAcquireRenderer(std::unique_ptr<openscreen::cast::RpcMessage> message);
  void OnAcquireRendererDone(int receiver_rpc_handle);

  // Indicates whether RPC_ACQUIRE_RENDERER_DONE is sent or not.
  bool is_acquire_renderer_done_sent_ = false;

  const raw_ptr<ReceiverController> receiver_controller_;

  const raw_ptr<openscreen::cast::RpcMessenger> rpc_messenger_;

  // The RPC handle used by all Receiver instances created by `this`. Sent only
  // once to the sender side, through RPC_ACQUIRE_RENDERER_DONE, regardless of
  // how many times CreateRenderer() is called.
  const int renderer_handle_ = openscreen::cast::RpcMessenger::kInvalidHandle;

  // The RPC handle of the CourierRenderer on the sender side. Will be received
  // once, via an RPC_ACQUIRE_RENDERER message.
  int remote_renderer_handle_ = openscreen::cast::RpcMessenger::kInvalidHandle;

  // Track whether a Receiver was created before RPC_ACQUIRE_RENDERER arrived.
  // `waiting_for_remote_handle_receiver_` holds the WeakPtr of the Receiver
  // instance and must only be dereferenced on `media_task_runner_`. Checking
  // whether a receiver is waiting is tracked via `has_waiting_receiver_` to
  // avoid calling WeakPtr::operator bool() on the main sequence (which would
  // bind Receiver's WeakPtrFactory to the main sequence instead of the media
  // sequence).
  bool has_waiting_receiver_ = false;
  base::WeakPtr<Receiver> waiting_for_remote_handle_receiver_;
  std::unique_ptr<RendererFactory> real_renderer_factory_;

  // Tasks of this class must run on `main_task_runner_`.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Used to instantiate `receiver_`.
  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  // WeakPtrs vended from `weak_factory_` must be consumed strictly on
  // `main_task_runner_`.
  base::WeakPtrFactory<RemotingRendererFactory> weak_factory_{this};
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_REMOTING_RENDERER_FACTORY_H_
