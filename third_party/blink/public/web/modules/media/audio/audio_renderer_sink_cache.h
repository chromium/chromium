// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_AUDIO_AUDIO_RENDERER_SINK_CACHE_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_AUDIO_AUDIO_RENDERER_SINK_CACHE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "media/base/output_device_info.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_common.h"

namespace media {
class AudioRendererSink;
}

namespace blink {

// Caches AudioRendererSink instances, provides them to the clients for usage,
// tracks their used/unused state, reuses them to obtain output device
// information, garbage-collects unused sinks.
// Must live on the main render thread. Thread safe.
//
// TODO(https://crrev.com/787252): Move this header out of the Blink public API
// layer.
class BLINK_MODULES_EXPORT AudioRendererSinkCache {
 public:
  virtual ~AudioRendererSinkCache() {}

  // Returns output device information for a specified sink.
  virtual media::OutputDeviceInfo GetSinkInfo(
      const LocalFrameToken& source_frame_token,
      const base::UnguessableToken& session_id,
      const std::string& device_id) = 0;

  // Provides a sink for usage. The sink must be returned to the cache by
  // calling ReleaseSink(). The sink must be stopped by the user before
  // deletion, but after releasing it from the cache.
  virtual scoped_refptr<media::AudioRendererSink> GetSink(
      const LocalFrameToken& source_frame_token,
      const std::string& device_id) = 0;

  // Notifies the cache that the sink is not in use any more. Must be
  // called by the client, so that the cache can garbage-collect the sink
  // reference.
  virtual void ReleaseSink(const media::AudioRendererSink* sink_ptr) = 0;

 protected:
  AudioRendererSinkCache() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioRendererSinkCache);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_AUDIO_AUDIO_RENDERER_SINK_CACHE_H_
