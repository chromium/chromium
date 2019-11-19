// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_AUDIO_RENDERER_SINK_CACHE_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_AUDIO_RENDERER_SINK_CACHE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "media/base/output_device_info.h"

namespace media {
class AudioRendererSink;
}

namespace content {
class RenderFrame;

// Caches AudioRendererSink instances, provides them to the clients for usage,
// tracks their used/unused state, reuses them to obtain output device
// information, garbage-collects unused sinks.
// Must live on the main render thread. Thread safe.
class CONTENT_EXPORT AudioRendererSinkCache {
 public:
  virtual ~AudioRendererSinkCache() {}

  // If called, the cache will drop sinks belonging to the specified frame on
  // navigation.
  static void ObserveFrame(RenderFrame* frame);

  // Returns output device information for a specified sink.
  virtual media::OutputDeviceInfo GetSinkInfo(
      int source_render_frame_id,
      const base::UnguessableToken& session_id,
      const std::string& device_id) = 0;

  // Provides a sink for usage. The sink must be returned to the cache by
  // calling ReleaseSink(). The sink must be stopped by the user before
  // deletion, but after releasing it from the cache.
  virtual scoped_refptr<media::AudioRendererSink> GetSink(
      int source_render_frame_id,
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

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_AUDIO_RENDERER_SINK_CACHE_H_
