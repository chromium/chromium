// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_DELEGATE_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_DELEGATE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"

namespace base {
class UnsafeSharedMemoryRegion;
class CancelableSyncSocket;
}

namespace media {

class MEDIA_EXPORT AudioOutputDelegate {
 public:
  // An AudioOutputDelegate must not call back to its EventHandler in its
  // constructor.
  class MEDIA_EXPORT EventHandler {
   public:
    virtual ~EventHandler() = 0;

    // Called when the underlying stream is ready for playout.
    virtual void OnStreamCreated(
        int stream_id,
        base::UnsafeSharedMemoryRegion shared_memory_region,
        std::unique_ptr<base::CancelableSyncSocket> socket) = 0;

    // Called if stream encounters an error and has become unusable.
    virtual void OnStreamError(int stream_id) = 0;
  };

  virtual ~AudioOutputDelegate() = 0;

  virtual int GetStreamId() = 0;

  // Stream control:
  virtual void OnPlayStream() = 0;
  virtual void OnPauseStream() = 0;
  virtual void OnFlushStream() = 0;
  virtual void OnSetVolume(double volume) = 0;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_DELEGATE_H_
