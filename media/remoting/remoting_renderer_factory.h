// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTING_RENDERER_FACTORY_H_
#define MEDIA_REMOTING_REMOTING_RENDERER_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/renderer_factory.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/openscreen/src/cast/streaming/public/rpc_messenger.h"

namespace media {
namespace remoting {

class Receiver;
class ReceiverController;

class RemotingRendererFactory : public RendererFactory {
 public:
  RemotingRendererFactory(
      mojo::PendingRemote<mojom::Remotee> remotee,
      std::unique_ptr<RendererFactory> renderer_factory,
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner);
  ~RemotingRendererFactory() override;

  // RendererFactory implementation
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

  // The RPC handle used by all Receiver instances created by |this|. Sent only
  // once to the sender side, through RPC_ACQUIRE_RENDERER_DONE, regardless of
  // how many times CreateRenderer() is called."
  const int renderer_handle_ = openscreen::cast::RpcMessenger::kInvalidHandle;

  // The RPC handle of the CourierRenderer on the sender side. Will be received
  // once, via an RPC_ACQUIRE_RENDERER message"
  int remote_renderer_handle_ = openscreen::cast::RpcMessenger::kInvalidHandle;

  // Used to set remote handle if receiving RPC_ACQUIRE_RENDERER after
  // CreateRenderer() is called.
  base::WeakPtr<Receiver> waiting_for_remote_handle_receiver_;
  std::unique_ptr<RendererFactory> real_renderer_factory_;

  // Used to instantiate |receiver_|.
  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  base::WeakPtrFactory<RemotingRendererFactory> weak_factory_{this};
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_REMOTING_RENDERER_FACTORY_H_
