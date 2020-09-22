// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media/audio/audio_renderer_sink_cache.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_renderer_sink.h"
#include "third_party/blink/public/web/modules/media/audio/web_audio_device_factory.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

AudioRendererSinkCache* AudioRendererSinkCache::instance_ = nullptr;

class AudioRendererSinkCache::FrameObserver final
    : public GarbageCollected<AudioRendererSinkCache::FrameObserver>,
      public Supplement<LocalFrame>,
      public ExecutionContextLifecycleObserver {
 public:
  static const char kSupplementName[];
  static FrameObserver* From(LocalFrame& frame) {
    return Supplement<LocalFrame>::From<FrameObserver>(frame);
  }

  explicit FrameObserver(LocalFrame& frame)
      : Supplement<LocalFrame>(frame),
        ExecutionContextLifecycleObserver(frame.DomWindow()) {}
  ~FrameObserver() { DCHECK(dropped_frame_cached_); }

  void Trace(Visitor* visitor) const final {
    Supplement<LocalFrame>::Trace(visitor);
    ExecutionContextLifecycleObserver::Trace(visitor);
  }

  // ExecutionContextLifecycleObserver implementation.
  void ContextDestroyed() override { DropFrameCache(); }

 private:
  void DropFrameCache() {
    dropped_frame_cached_ = true;

    if (!AudioRendererSinkCache::instance_)
      return;
    if (!GetSupplementable())
      return;

    LocalFrameToken frame_token = GetSupplementable()->GetLocalFrameToken();
    AudioRendererSinkCache::instance_->DropSinksForFrame(frame_token);
  }

  bool dropped_frame_cached_ = false;
  DISALLOW_COPY_AND_ASSIGN(FrameObserver);
};

const char AudioRendererSinkCache::FrameObserver::kSupplementName[] =
    "AudioRendererSinkCache::FrameObserver";

namespace {

enum GetOutputDeviceInfoCacheUtilization {
  // No cached sink found.
  SINK_CACHE_MISS_NO_SINK = 0,

  // If session id is used to specify a device, we always have to create and
  // cache a new sink.
  SINK_CACHE_MISS_CANNOT_LOOKUP_BY_SESSION_ID = 1,

  // Output parmeters for an already-cached sink are requested.
  SINK_CACHE_HIT = 2,

  // For UMA.
  SINK_CACHE_LAST_ENTRY
};

bool SinkIsHealthy(media::AudioRendererSink* sink) {
  return sink->GetOutputDeviceInfo().device_status() ==
         media::OUTPUT_DEVICE_STATUS_OK;
}

}  // namespace

// Cached sink data.
struct AudioRendererSinkCache::CacheEntry {
  LocalFrameToken source_frame_token;
  std::string device_id;
  scoped_refptr<media::AudioRendererSink> sink;  // Sink instance
  bool used;                                     // True if in use by a client.
};

// static
void AudioRendererSinkCache::InstallFrameObserver(LocalFrame& frame) {
  if (AudioRendererSinkCache::FrameObserver::From(frame))
    return;
  Supplement<LocalFrame>::ProvideTo(
      frame,
      MakeGarbageCollected<AudioRendererSinkCache::FrameObserver>(frame));
}

AudioRendererSinkCache::AudioRendererSinkCache(
    scoped_refptr<base::SequencedTaskRunner> cleanup_task_runner,
    CreateSinkCallback create_sink_cb,
    base::TimeDelta delete_timeout)
    : cleanup_task_runner_(std::move(cleanup_task_runner)),
      create_sink_cb_(std::move(create_sink_cb)),
      delete_timeout_(delete_timeout) {
  DCHECK(!instance_);
  instance_ = this;
}

AudioRendererSinkCache::~AudioRendererSinkCache() {
  // We just release all the cached sinks here. Stop them first.
  // We can stop all the sinks, no matter they are used or not, since
  // everything is being destroyed anyways.
  for (auto& entry : cache_)
    entry.sink->Stop();

  if (instance_ == this)
    instance_ = nullptr;
}

media::OutputDeviceInfo AudioRendererSinkCache::GetSinkInfo(
    const LocalFrameToken& source_frame_token,
    const base::UnguessableToken& session_id,
    const std::string& device_id) {
  TRACE_EVENT_BEGIN2("audio", "AudioRendererSinkCache::GetSinkInfo",
                     "frame_token", source_frame_token.ToString(), "device id",
                     device_id);

  if (media::AudioDeviceDescription::UseSessionIdToSelectDevice(session_id,
                                                                device_id)) {
    // We are provided with session id instead of device id. Session id is
    // unique, so we can't find any matching sink. Creating a new one.
    scoped_refptr<media::AudioRendererSink> sink =
        create_sink_cb_.Run(source_frame_token, {session_id, device_id});

    CacheOrStopUnusedSink(source_frame_token,
                          sink->GetOutputDeviceInfo().device_id(), sink);

    UMA_HISTOGRAM_ENUMERATION(
        "Media.Audio.Render.SinkCache.GetOutputDeviceInfoCacheUtilization",
        SINK_CACHE_MISS_CANNOT_LOOKUP_BY_SESSION_ID, SINK_CACHE_LAST_ENTRY);
    TRACE_EVENT_END1("audio", "AudioRendererSinkCache::GetSinkInfo", "result",
                     "Cache not used due to using |session_id|");

    return sink->GetOutputDeviceInfo();
  }
  // Ignore session id.
  {
    base::AutoLock auto_lock(cache_lock_);
    auto cache_iter = FindCacheEntry_Locked(source_frame_token, device_id,
                                            false /* unused_only */);
    if (cache_iter != cache_.end()) {
      // A matching cached sink is found.
      UMA_HISTOGRAM_ENUMERATION(
          "Media.Audio.Render.SinkCache.GetOutputDeviceInfoCacheUtilization",
          SINK_CACHE_HIT, SINK_CACHE_LAST_ENTRY);
      TRACE_EVENT_END1("audio", "AudioRendererSinkCache::GetSinkInfo", "result",
                       "Cache hit");
      return cache_iter->sink->GetOutputDeviceInfo();
    }
  }

  // No matching sink found, create a new one.
  scoped_refptr<media::AudioRendererSink> sink = create_sink_cb_.Run(
      source_frame_token,
      media::AudioSinkParameters(base::UnguessableToken(), device_id));

  CacheOrStopUnusedSink(source_frame_token, device_id, sink);

  UMA_HISTOGRAM_ENUMERATION(
      "Media.Audio.Render.SinkCache.GetOutputDeviceInfoCacheUtilization",
      SINK_CACHE_MISS_NO_SINK, SINK_CACHE_LAST_ENTRY);

  TRACE_EVENT_END1("audio", "AudioRendererSinkCache::GetSinkInfo", "result",
                   "Cache miss");
  // |sink| is ref-counted, so it's ok if it is removed from cache before we
  // get here.
  return sink->GetOutputDeviceInfo();
}

scoped_refptr<media::AudioRendererSink> AudioRendererSinkCache::GetSink(
    const LocalFrameToken& source_frame_token,
    const std::string& device_id) {
  UMA_HISTOGRAM_BOOLEAN("Media.Audio.Render.SinkCache.UsedForSinkCreation",
                        true);
  TRACE_EVENT_BEGIN2("audio", "AudioRendererSinkCache::GetSink", "frame_token",
                     source_frame_token.ToString(), "device id", device_id);

  base::AutoLock auto_lock(cache_lock_);

  auto cache_iter = FindCacheEntry_Locked(source_frame_token, device_id,
                                          true /* unused sink only */);

  if (cache_iter != cache_.end()) {
    // Found unused sink; mark it as used and return.
    cache_iter->used = true;
    UMA_HISTOGRAM_BOOLEAN(
        "Media.Audio.Render.SinkCache.InfoSinkReusedForOutput", true);
    TRACE_EVENT_END1("audio", "AudioRendererSinkCache::GetSink", "result",
                     "Cache hit");
    return cache_iter->sink;
  }

  // No unused sink is found, create one, mark it used, cache it and return.
  CacheEntry cache_entry = {
      source_frame_token, device_id,
      create_sink_cb_.Run(
          source_frame_token,
          media::AudioSinkParameters(base::UnguessableToken(), device_id)),
      true /* used */};

  if (SinkIsHealthy(cache_entry.sink.get())) {
    TRACE_EVENT_INSTANT0("audio",
                         "AudioRendererSinkCache::GetSink: caching new sink",
                         TRACE_EVENT_SCOPE_THREAD);
    cache_.push_back(cache_entry);
  }

  TRACE_EVENT_END1("audio", "AudioRendererSinkCache::GetSink", "result",
                   "Cache miss");
  return cache_entry.sink;
}

void AudioRendererSinkCache::ReleaseSink(
    const media::AudioRendererSink* sink_ptr) {
  // We don't know the sink state, so won't reuse it. Delete it immediately.
  DeleteSink(sink_ptr, true);
}

void AudioRendererSinkCache::DeleteLaterIfUnused(
    const media::AudioRendererSink* sink_ptr) {
  cleanup_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AudioRendererSinkCache::DeleteSink,
                     // Unretained is safe here since this is a process-wide
                     // singleton and tests will ensure lifetime.
                     base::Unretained(this), base::RetainedRef(sink_ptr),
                     false /*do not delete if used*/),
      delete_timeout_);
}

void AudioRendererSinkCache::DeleteSink(
    const media::AudioRendererSink* sink_ptr,
    bool force_delete_used) {
  DCHECK(sink_ptr);

  scoped_refptr<media::AudioRendererSink> sink_to_stop;

  {
    base::AutoLock auto_lock(cache_lock_);

    // Looking up the sink by its pointer.
    auto cache_iter = std::find_if(cache_.begin(), cache_.end(),
                                   [sink_ptr](const CacheEntry& val) {
                                     return val.sink.get() == sink_ptr;
                                   });

    if (cache_iter == cache_.end())
      return;

    // When |force_delete_used| is set, it's expected that we are deleting a
    // used sink.
    DCHECK((!force_delete_used) || (force_delete_used && cache_iter->used))
        << "Attempt to delete a non-acquired sink.";

    if (!force_delete_used && cache_iter->used)
      return;

    // To stop the sink before deletion if it's not used, we need to hold
    // a ref to it.
    if (!cache_iter->used) {
      sink_to_stop = cache_iter->sink;
      UMA_HISTOGRAM_BOOLEAN(
          "Media.Audio.Render.SinkCache.InfoSinkReusedForOutput", false);
    }

    cache_.erase(cache_iter);
  }  // Lock scope;

  // Stop the sink out of the lock scope.
  if (sink_to_stop.get()) {
    DCHECK_EQ(sink_ptr, sink_to_stop.get());
    sink_to_stop->Stop();
  }
}

AudioRendererSinkCache::CacheContainer::iterator
AudioRendererSinkCache::FindCacheEntry_Locked(
    const LocalFrameToken& source_frame_token,
    const std::string& device_id,
    bool unused_only) {
  return std::find_if(
      cache_.begin(), cache_.end(),
      [source_frame_token, &device_id, unused_only](const CacheEntry& val) {
        if (val.used && unused_only)
          return false;
        if (val.source_frame_token != source_frame_token)
          return false;
        if (media::AudioDeviceDescription::IsDefaultDevice(device_id) &&
            media::AudioDeviceDescription::IsDefaultDevice(val.device_id)) {
          // Both device IDs represent the same default device => do not
          // compare them;
          return true;
        }
        return val.device_id == device_id;
      });
}

void AudioRendererSinkCache::CacheOrStopUnusedSink(
    const LocalFrameToken& source_frame_token,
    const std::string& device_id,
    scoped_refptr<media::AudioRendererSink> sink) {
  if (!SinkIsHealthy(sink.get())) {
    TRACE_EVENT_INSTANT0("audio", "CacheOrStopUnusedSink: Unhealthy sink",
                         TRACE_EVENT_SCOPE_THREAD);
    // Since |sink| is not cached, we must make sure to Stop it now.
    sink->Stop();
    return;
  }

  CacheEntry cache_entry = {source_frame_token, device_id, std::move(sink),
                            false /* not used */};

  {
    base::AutoLock auto_lock(cache_lock_);
    cache_.push_back(cache_entry);
  }

  DeleteLaterIfUnused(cache_entry.sink.get());
}

void AudioRendererSinkCache::DropSinksForFrame(
    const LocalFrameToken& source_frame_token) {
  base::AutoLock auto_lock(cache_lock_);
  base::EraseIf(cache_, [source_frame_token](const CacheEntry& val) {
    if (val.source_frame_token == source_frame_token) {
      val.sink->Stop();
      return true;
    }
    return false;
  });
}

size_t AudioRendererSinkCache::GetCacheSizeForTesting() {
  return cache_.size();
}

}  // namespace blink
