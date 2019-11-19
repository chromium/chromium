// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_AUDIO_RENDERER_SINK_CACHE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_AUDIO_RENDERER_SINK_CACHE_IMPL_H_

#include "content/renderer/media/audio/audio_renderer_sink_cache.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "media/audio/audio_sink_parameters.h"

namespace content {

// AudioRendererSinkCache implementation.
class CONTENT_EXPORT AudioRendererSinkCacheImpl
    : public AudioRendererSinkCache {
 public:
  class FrameObserver;

  // Callback to be used for AudioRendererSink creation
  using CreateSinkCallback =
      base::RepeatingCallback<scoped_refptr<media::AudioRendererSink>(
          int render_frame_id,
          const media::AudioSinkParameters& params)>;

  // |cleanup_task_runner| will be used to delete sinks when they are unused,
  // AudioRendererSinkCacheImpl must outlive any tasks posted to it. Since
  // the sink cache is normally a process-wide singleton, this isn't a problem.
  AudioRendererSinkCacheImpl(
      scoped_refptr<base::SequencedTaskRunner> cleanup_task_runner,
      CreateSinkCallback create_sink_callback,
      base::TimeDelta delete_timeout);

  ~AudioRendererSinkCacheImpl() final;

  media::OutputDeviceInfo GetSinkInfo(int source_render_frame_id,
                                      const base::UnguessableToken& session_id,
                                      const std::string& device_id) final;

  scoped_refptr<media::AudioRendererSink> GetSink(
      int source_render_frame_id,
      const std::string& device_id) final;

  void ReleaseSink(const media::AudioRendererSink* sink_ptr) final;

 private:
  friend class AudioRendererSinkCacheTest;
  friend class CacheEntryFinder;
  friend class AudioRendererSinkCacheImpl::FrameObserver;

  struct CacheEntry;
  using CacheContainer = std::vector<CacheEntry>;

  // Schedules a sink for deletion. Deletion will be performed on the same
  // thread the cache is created on.
  void DeleteLaterIfUnused(const media::AudioRendererSink* sink_ptr);

  // Deletes a sink from the cache. If |force_delete_used| is set, a sink being
  // deleted can (and should) be in use at the moment of deletion; otherwise the
  // sink is deleted only if unused.
  void DeleteSink(const media::AudioRendererSink* sink_ptr,
                  bool force_delete_used);

  CacheContainer::iterator FindCacheEntry_Locked(
      int source_render_frame_id,
      const std::string& device_id,
      bool unused_only);

  void CacheOrStopUnusedSink(int source_render_frame_id,
                             const std::string& device_id,
                             scoped_refptr<media::AudioRendererSink> sink);

  void DropSinksForFrame(int source_render_frame_id);

  // To avoid publishing CacheEntry structure in the header.
  int GetCacheSizeForTesting();

  // Global instance, set in constructor and unset in destructor.
  static AudioRendererSinkCacheImpl* instance_;

  // Renderer main task runner.
  const scoped_refptr<base::SequencedTaskRunner> cleanup_task_runner_;

  // Callback used for sink creation.
  const CreateSinkCallback create_sink_cb_;

  // Cached sink deletion timeout.
  // For example: (1) sink was created and cached in GetSinkInfo(), and then (2)
  // the same sink is requested in GetSink(), if time interval between (1) and
  // (2) is less than |kDeleteTimeoutMs|, then sink cached in (1) is reused in
  // (2). On the other hand, if after (1) nobody is interested in the sink
  // within |kDeleteTimeoutMs|, it is garbage-collected.
  const base::TimeDelta delete_timeout_;

  // Cached sinks, protected by lock.
  base::Lock cache_lock_;
  CacheContainer cache_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererSinkCacheImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_AUDIO_RENDERER_SINK_CACHE_IMPL_H_
