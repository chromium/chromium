// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_AUDIO_OUTPUT_IPC_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_AUDIO_OUTPUT_IPC_FACTORY_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/media/renderer_audio_output_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class AudioOutputIPC;
}

namespace service_manager {
class InterfaceProvider;
}

namespace content {

// This is a factory for AudioOutputIPC objects. It is threadsafe. This class
// is designed to be leaked at shutdown, as it posts tasks to itself using
// base::Unretained and also hands out references to itself in the
// AudioOutputIPCs it creates, but in the case where the owner is sure that
// there are no outstanding references (such as in a unit test), the class can
// be destructed.
// TODO(maxmorin): Registering the factories for each frame will become
// unnecessary when https://crbug.com/668275 is fixed. When that is done, this
// class can be greatly simplified.
class CONTENT_EXPORT AudioOutputIPCFactory {
 public:
  AudioOutputIPCFactory(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~AudioOutputIPCFactory();

  static AudioOutputIPCFactory* get() { return instance_; }

  const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner() const {
    return io_task_runner_;
  }

  // Enables |this| to create MojoAudioOutputIPCs for the specified frame.
  // Does nothing if not using mojo factories.
  void RegisterRemoteFactory(
      int frame_id,
      service_manager::InterfaceProvider* interface_provider);

  // Every call to the above method must be matched by a call to this one when
  // the frame is destroyed. Does nothing if not using mojo factories.
  void MaybeDeregisterRemoteFactory(int frame_id);

  // The returned object may only be used on |io_task_runner()|.
  std::unique_ptr<media::AudioOutputIPC> CreateAudioOutputIPC(
      int frame_id) const;

 private:
  using StreamFactoryMap =
      base::flat_map<int,
                     mojo::Remote<mojom::RendererAudioOutputStreamFactory>>;

  mojom::RendererAudioOutputStreamFactory* GetRemoteFactory(int frame_id) const;

  void RegisterRemoteFactoryOnIOThread(
      int frame_id,
      mojo::PendingRemote<mojom::RendererAudioOutputStreamFactory>
          factory_pending_remote);

  void MaybeDeregisterRemoteFactoryOnIOThread(int frame_id);

  // Indicates whether mojo factories are used.
  bool UsingMojoFactories() const;

  // Maps frame id to the corresponding factory.
  StreamFactoryMap factory_remotes_;

  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Global instance, set in constructor and unset in destructor.
  static AudioOutputIPCFactory* instance_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputIPCFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_AUDIO_OUTPUT_IPC_FACTORY_H_
