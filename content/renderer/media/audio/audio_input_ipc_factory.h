// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_AUDIO_INPUT_IPC_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_AUDIO_INPUT_IPC_FACTORY_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "media/audio/audio_source_parameters.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace media {
class AudioInputIPC;
}

namespace content {

// This is a thread-safe factory for AudioInputIPC objects.
class CONTENT_EXPORT AudioInputIPCFactory {
 public:
  AudioInputIPCFactory(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  ~AudioInputIPCFactory();

  static AudioInputIPCFactory* get() {
    DCHECK(instance_);
    return instance_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner() const {
    return io_task_runner_;
  }

  // The returned object may only be used on io_task_runner().
  std::unique_ptr<media::AudioInputIPC> CreateAudioInputIPC(
      const blink::LocalFrameToken& frame_token,
      const media::AudioSourceParameters& source_params) const;

 private:
  const scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Global instance, set in constructor and unset in destructor.
  static AudioInputIPCFactory* instance_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputIPCFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_AUDIO_INPUT_IPC_FACTORY_H_
