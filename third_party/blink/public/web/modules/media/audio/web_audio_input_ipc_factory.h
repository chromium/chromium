// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_AUDIO_WEB_AUDIO_INPUT_IPC_FACTORY_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_AUDIO_WEB_AUDIO_INPUT_IPC_FACTORY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_common.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace media {
class AudioInputIPC;
struct AudioSourceParameters;
}  // namespace media

namespace blink {

// This is a thread-safe factory for AudioInputIPC objects.
class BLINK_MODULES_EXPORT WebAudioInputIPCFactory {
 public:
  WebAudioInputIPCFactory(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~WebAudioInputIPCFactory();

  static WebAudioInputIPCFactory& GetInstance();

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

  DISALLOW_COPY_AND_ASSIGN(WebAudioInputIPCFactory);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_AUDIO_WEB_AUDIO_INPUT_IPC_FACTORY_H_
