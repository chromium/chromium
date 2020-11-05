// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_AUDIO_AUDIO_RENDERER_SINK_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_AUDIO_AUDIO_RENDERER_SINK_CACHE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "media/audio/audio_sink_parameters.h"
#include "media/base/output_device_info.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace media {
class AudioRendererSink;
}

namespace blink {

class LocalDOMWindow;

// Caches AudioRendererSink instances, provides them to the clients for usage,
// tracks their used/unused state, reuses them to obtain output device
// information, garbage-collects unused sinks.
// Must live on the main render thread. Thread safe.
class MODULES_EXPORT AudioRendererSinkCache {
 public:
  class WindowObserver;

  // Callback to be used for AudioRendererSink creation
  using CreateSinkCallback =
      base::RepeatingCallback<scoped_refptr<media::AudioRendererSink>(
          const LocalFrameToken& frame_token,
          const media::AudioSinkParameters& params)>;

  // If called, the cache will drop sinks belonging to the specified window on
  // navigation.
  static void InstallWindowObserver(LocalDOMWindow&);

  // |cleanup_task_runner| will be used to delete sinks when they are unused,
  // AudioRendererSinkCache must outlive any tasks posted to it. Since
  // the sink cache is normally a process-wide singleton, this isn't a problem.
  AudioRendererSinkCache(
      scoped_refptr<base::SequencedTaskRunner> cleanup_task_runner,
      CreateSinkCallback create_sink_callback,
      base::TimeDelta delete_timeout);
  ~AudioRendererSinkCache();

  // AudioRendererSinkCache implementation:
  media::OutputDeviceInfo GetSinkInfo(const LocalFrameToken& source_frame_token,
                                      const base::UnguessableToken& session_id,
                                      const std::string& device_id);
  scoped_refptr<media::AudioRendererSink> GetSink(
      const LocalFrameToken& source_frame_token,
      const std::string& device_id);
  void ReleaseSink(const media::AudioRendererSink* sink_ptr);

 private:
  friend class AudioRendererSinkCacheTest;
  friend class CacheEntryFinder;
  friend class AudioRendererSinkCache::WindowObserver;

  struct CacheEntry;
  using CacheContainer = std::vector<CacheEntry>;

  // Schedules a sink for deletion. Deletion will be performed on the same
  // thread the cache is created on.
  void DeleteLaterIfUnused(scoped_refptr<media::AudioRendererSink> sink);

  // Deletes a sink from the cache. If |force_delete_used| is set, a sink being
  // deleted can (and should) be in use at the moment of deletion; otherwise the
  // sink is deleted only if unused.
  void DeleteSink(const media::AudioRendererSink* sink_ptr,
                  bool force_delete_used);

  CacheContainer::iterator FindCacheEntry_Locked(
      const LocalFrameToken& source_frame_token,
      const std::string& device_id,
      bool unused_only);

  void CacheOrStopUnusedSink(const LocalFrameToken& source_frame_token,
                             const std::string& device_id,
                             scoped_refptr<media::AudioRendererSink> sink);

  void DropSinksForFrame(const LocalFrameToken& source_frame_token);

  // To avoid publishing CacheEntry structure in the header.
  size_t GetCacheSizeForTesting();

  // Global instance, set in constructor and unset in destructor.
  static AudioRendererSinkCache* instance_;

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

  DISALLOW_COPY_AND_ASSIGN(AudioRendererSinkCache);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_AUDIO_AUDIO_RENDERER_SINK_CACHE_H_
