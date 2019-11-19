// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio/audio_output_ipc_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "content/renderer/media/audio/mojo_audio_output_ipc.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

AudioOutputIPCFactory* AudioOutputIPCFactory::instance_ = nullptr;

AudioOutputIPCFactory::AudioOutputIPCFactory(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(std::move(io_task_runner)) {
  DCHECK(!instance_);
  instance_ = this;
}

AudioOutputIPCFactory::~AudioOutputIPCFactory() {
  // Allow destruction in tests.
  DCHECK(factory_remotes_.empty());
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

std::unique_ptr<media::AudioOutputIPC>
AudioOutputIPCFactory::CreateAudioOutputIPC(int frame_id) const {
    // Unretained is safe due to the contract at the top of the header file.
    return std::make_unique<MojoAudioOutputIPC>(
        base::BindRepeating(&AudioOutputIPCFactory::GetRemoteFactory,
                            base::Unretained(this), frame_id),
        io_task_runner_);
}

void AudioOutputIPCFactory::RegisterRemoteFactory(
    int frame_id,
    service_manager::InterfaceProvider* interface_provider) {
  mojo::PendingRemote<mojom::RendererAudioOutputStreamFactory> factory_remote;
  interface_provider->GetInterface(
      factory_remote.InitWithNewPipeAndPassReceiver());
  // Unretained is safe due to the contract at the top of the header file.
  // It's safe to pass the |factory_remote| PendingRemote between threads.
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioOutputIPCFactory::RegisterRemoteFactoryOnIOThread,
                     base::Unretained(this), frame_id,
                     std::move(factory_remote)));
}

void AudioOutputIPCFactory::MaybeDeregisterRemoteFactory(int frame_id) {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioOutputIPCFactory::MaybeDeregisterRemoteFactoryOnIOThread,
          base::Unretained(this), frame_id));
}

mojom::RendererAudioOutputStreamFactory*
AudioOutputIPCFactory::GetRemoteFactory(int frame_id) const {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  auto it = factory_remotes_.find(frame_id);
  return it == factory_remotes_.end() ? nullptr : it->second.get();
}

void AudioOutputIPCFactory::RegisterRemoteFactoryOnIOThread(
    int frame_id,
    mojo::PendingRemote<mojom::RendererAudioOutputStreamFactory>
        factory_pending_remote) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  std::pair<StreamFactoryMap::iterator, bool> emplace_result =
      factory_remotes_.emplace(frame_id, std::move(factory_pending_remote));

  DCHECK(emplace_result.second) << "Attempt to register a factory for a "
                                   "frame which already has a factory "
                                   "registered.";

  auto& emplaced_factory = emplace_result.first->second;
  DCHECK(emplaced_factory.is_bound())
      << "Factory is not bound to a remote implementation.";

  // Unretained is safe because |this| owns the remote, so a connection error
  // cannot trigger after destruction.
  emplaced_factory.set_disconnect_handler(base::BindOnce(
      &AudioOutputIPCFactory::MaybeDeregisterRemoteFactoryOnIOThread,
      base::Unretained(this), frame_id));
}

void AudioOutputIPCFactory::MaybeDeregisterRemoteFactoryOnIOThread(
    int frame_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // This function can be called both by the frame and the connection error
  // handler of the factory remote. Calling erase multiple times even though
  // there is nothing to erase is safe, so we don't have to handle this in any
  // particular way.
  factory_remotes_.erase(frame_id);
}

}  // namespace content
