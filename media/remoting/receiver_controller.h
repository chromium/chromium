// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_RECEIVER_CONTROLLER_H_
#define MEDIA_REMOTING_RECEIVER_CONTROLLER_H_

#include <memory>

#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/openscreen/src/cast/streaming/public/rpc_messenger.h"

namespace media {
namespace remoting {

// ReceiverController is the bridge that owns |rpc_messenger_| to allow
// Receivers and StreamProvider::MediaStreams to communicate with the sender via
// RPC calls.
//
// It also forwards calls to a |media_remotee_| instance, which will be
// implemented the browser process. Currently, the only use case will be on
// Chromecast, the Remotee implementation will be implemented in the browser
// code on Chromecast.
//
// NOTE: ReceiverController is a singleton per process.
class ReceiverController : mojom::RemotingSink {
 public:
  static ReceiverController* GetInstance();
  void Initialize(mojo::PendingRemote<mojom::Remotee> remotee);

  // Proxy functions to |media_remotee_|.
  void OnRendererFlush(uint32_t audio_count, uint32_t video_count);
  void OnVideoNaturalSizeChange(const gfx::Size& size);
  void StartDataStreams(
      mojo::PendingRemote<mojom::RemotingDataStreamReceiver> audio_stream,
      mojo::PendingRemote<mojom::RemotingDataStreamReceiver> video_stream);

  openscreen::cast::RpcMessenger* rpc_messenger() { return &rpc_messenger_; }

 private:
  friend base::NoDestructor<ReceiverController>;
  friend class MockReceiverController;
  friend void ResetForTesting(ReceiverController* controller);

  ReceiverController();
  ~ReceiverController() override;

  // media::mojom::RemotingSink implementation.
  void OnMessageFromSource(const std::vector<uint8_t>& message) override;

  // Callback for |rpc_messenger_| to send messages.
  void OnSendRpc(std::vector<uint8_t> message);

  openscreen::cast::RpcMessenger rpc_messenger_;

  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  mojo::Remote<media::mojom::Remotee> media_remotee_;
  mojo::Receiver<media::mojom::RemotingSink> receiver_{this};
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_RECEIVER_CONTROLLER_H_
