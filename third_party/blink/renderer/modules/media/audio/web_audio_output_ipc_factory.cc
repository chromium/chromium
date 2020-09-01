// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/media/audio/web_audio_output_ipc_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/media/renderer_audio_output_stream_factory.mojom-blink.h"
#include "third_party/blink/renderer/modules/media/audio/mojo_audio_output_ipc.h"

namespace blink {

WebAudioOutputIPCFactory* WebAudioOutputIPCFactory::instance_ = nullptr;

class WebAudioOutputIPCFactory::Impl {
 public:
  using StreamFactoryMap = base::flat_map<
      blink::LocalFrameToken,
      mojo::Remote<mojom::blink::RendererAudioOutputStreamFactory>>;

  explicit Impl(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
      : io_task_runner_(std::move(io_task_runner)) {}
  ~Impl() { DCHECK(factory_remotes_.empty()); }

  mojom::blink::RendererAudioOutputStreamFactory* GetRemoteFactory(
      const blink::LocalFrameToken& frame_token) const;

  void RegisterRemoteFactoryOnIOThread(
      const blink::LocalFrameToken& frame_token,
      mojo::PendingRemote<mojom::blink::RendererAudioOutputStreamFactory>
          factory_pending_remote);

  void MaybeDeregisterRemoteFactoryOnIOThread(
      const blink::LocalFrameToken& frame_token);

  // Maps frame id to the corresponding factory.
  StreamFactoryMap factory_remotes_;
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Impl);
};

WebAudioOutputIPCFactory::WebAudioOutputIPCFactory(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : impl_(std::make_unique<Impl>(std::move(io_task_runner))) {
  DCHECK(!instance_);
  instance_ = this;
}

WebAudioOutputIPCFactory::~WebAudioOutputIPCFactory() {
  // Allow destruction in tests.
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

std::unique_ptr<media::AudioOutputIPC>
WebAudioOutputIPCFactory::CreateAudioOutputIPC(
    const blink::LocalFrameToken& frame_token) const {
  // Unretained is safe due to the contract at the top of the header file.
  return std::make_unique<MojoAudioOutputIPC>(
      base::BindRepeating(&WebAudioOutputIPCFactory::Impl::GetRemoteFactory,
                          base::Unretained(impl_.get()), frame_token),
      io_task_runner());
}

void WebAudioOutputIPCFactory::RegisterRemoteFactory(
    const blink::LocalFrameToken& frame_token,
    blink::BrowserInterfaceBrokerProxy* interface_broker) {
  mojo::PendingRemote<mojom::blink::RendererAudioOutputStreamFactory>
      factory_remote;
  interface_broker->GetInterface(
      factory_remote.InitWithNewPipeAndPassReceiver());
  // Unretained is safe due to the contract at the top of the header file.
  // It's safe to pass the |factory_remote| PendingRemote between threads.
  io_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebAudioOutputIPCFactory::Impl::RegisterRemoteFactoryOnIOThread,
          base::Unretained(impl_.get()), frame_token,
          std::move(factory_remote)));
}

void WebAudioOutputIPCFactory::MaybeDeregisterRemoteFactory(
    const blink::LocalFrameToken& frame_token) {
  io_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&WebAudioOutputIPCFactory::Impl::
                                    MaybeDeregisterRemoteFactoryOnIOThread,
                                base::Unretained(impl_.get()), frame_token));
}

const scoped_refptr<base::SingleThreadTaskRunner>&
WebAudioOutputIPCFactory::io_task_runner() const {
  return impl_->io_task_runner_;
}

mojom::blink::RendererAudioOutputStreamFactory*
WebAudioOutputIPCFactory::Impl::GetRemoteFactory(
    const blink::LocalFrameToken& frame_token) const {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  auto it = factory_remotes_.find(frame_token);
  return it == factory_remotes_.end() ? nullptr : it->second.get();
}

void WebAudioOutputIPCFactory::Impl::RegisterRemoteFactoryOnIOThread(
    const blink::LocalFrameToken& frame_token,
    mojo::PendingRemote<mojom::blink::RendererAudioOutputStreamFactory>
        factory_pending_remote) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  std::pair<StreamFactoryMap::iterator, bool> emplace_result =
      factory_remotes_.emplace(frame_token, std::move(factory_pending_remote));

  DCHECK(emplace_result.second) << "Attempt to register a factory for a "
                                   "frame which already has a factory "
                                   "registered.";

  auto& emplaced_factory = emplace_result.first->second;
  DCHECK(emplaced_factory.is_bound())
      << "Factory is not bound to a remote implementation.";

  // Unretained is safe because |this| owns the remote, so a connection error
  // cannot trigger after destruction.
  emplaced_factory.set_disconnect_handler(base::BindOnce(
      &WebAudioOutputIPCFactory::Impl::MaybeDeregisterRemoteFactoryOnIOThread,
      base::Unretained(this), frame_token));
}

void WebAudioOutputIPCFactory::Impl::MaybeDeregisterRemoteFactoryOnIOThread(
    const blink::LocalFrameToken& frame_token) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // This function can be called both by the frame and the connection error
  // handler of the factory remote. Calling erase multiple times even though
  // there is nothing to erase is safe, so we don't have to handle this in any
  // particular way.
  factory_remotes_.erase(frame_token);
}

}  // namespace blink
