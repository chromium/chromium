// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media/audio/audio_renderer_sink_cache.h"

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_renderer_sink.h"
#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

AudioRendererSinkCache* AudioRendererSinkCache::instance_ = nullptr;

class AudioRendererSinkCache::WindowObserver final
    : public GarbageCollected<AudioRendererSinkCache::WindowObserver>,
      public Supplement<LocalDOMWindow>,
      public ExecutionContextLifecycleObserver {
 public:
  static const char kSupplementName[];

  explicit WindowObserver(LocalDOMWindow& window)
      : Supplement<LocalDOMWindow>(window),
        ExecutionContextLifecycleObserver(&window) {}

  WindowObserver(const WindowObserver&) = delete;
  WindowObserver& operator=(const WindowObserver&) = delete;

  ~WindowObserver() override = default;

  void Trace(Visitor* visitor) const final {
    Supplement<LocalDOMWindow>::Trace(visitor);
    ExecutionContextLifecycleObserver::Trace(visitor);
  }

  // ExecutionContextLifecycleObserver implementation.
  void ContextDestroyed() override {
    if (auto* cache_instance = AudioRendererSinkCache::instance_)
      cache_instance->DropSinksForFrame(DomWindow()->GetLocalFrameToken());
  }
};

const char AudioRendererSinkCache::WindowObserver::kSupplementName[] =
    "AudioRendererSinkCache::WindowObserver";

namespace {

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
};

// static
void AudioRendererSinkCache::InstallWindowObserver(LocalDOMWindow& window) {
  if (Supplement<LocalDOMWindow>::From<WindowObserver>(window))
    return;
  Supplement<LocalDOMWindow>::ProvideTo(
      window, MakeGarbageCollected<WindowObserver>(window));
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
  {
    // Stop all of the sinks before destruction.
    base::AutoLock auto_lock(cache_lock_);
    for (auto& entry : cache_)
      entry.sink->Stop();
  }

  DCHECK(instance_ == this);
  instance_ = nullptr;
}

media::OutputDeviceInfo AudioRendererSinkCache::GetSinkInfo(
    const LocalFrameToken& source_frame_token,
    const std::string& device_id) {
  TRACE_EVENT_BEGIN2("audio", "AudioRendererSinkCache::GetSinkInfo",
                     "frame_token", source_frame_token.ToString(), "device id",
                     device_id);
  {
    base::AutoLock auto_lock(cache_lock_);
    auto cache_iter = FindCacheEntry_Locked(source_frame_token, device_id);
    if (cache_iter != cache_.end()) {
      // A matching cached sink is found.
      TRACE_EVENT_END1("audio", "AudioRendererSinkCache::GetSinkInfo", "result",
                       "Cache hit");
      return cache_iter->sink->GetOutputDeviceInfo();
    }
  }

  // No matching sink found, create a new one.
  scoped_refptr<media::AudioRendererSink> sink =
      create_sink_cb_.Run(source_frame_token, device_id);

  MaybeCacheSink(source_frame_token, device_id, sink);

  TRACE_EVENT_END1("audio", "AudioRendererSinkCache::GetSinkInfo", "result",
                   "Cache miss");
  // |sink| is ref-counted, so it's ok if it is removed from cache before we
  // get here.
  return sink->GetOutputDeviceInfo();
}

void AudioRendererSinkCache::DeleteLater(
    scoped_refptr<media::AudioRendererSink> sink) {
  PostDelayedCrossThreadTask(
      *cleanup_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &AudioRendererSinkCache::DeleteSink,
          // Unretained is safe here since this is a process-wide
          // singleton and tests will ensure lifetime.
          CrossThreadUnretained(this), WTF::RetainedRef(std::move(sink))),
      delete_timeout_);
}

void AudioRendererSinkCache::DeleteSink(
    const media::AudioRendererSink* sink_ptr) {
  DCHECK(sink_ptr);

  scoped_refptr<media::AudioRendererSink> sink_to_stop;

  {
    base::AutoLock auto_lock(cache_lock_);

    // Looking up the sink by its pointer.
    auto cache_iter = base::ranges::find(
        cache_, sink_ptr, [](const CacheEntry& val) { return val.sink.get(); });

    if (cache_iter == cache_.end())
      return;

    sink_to_stop = cache_iter->sink;
    cache_.erase(cache_iter);
  }  // Lock scope;

  // Stop the sink out of the lock scope.
  if (sink_to_stop) {
    DCHECK_EQ(sink_ptr, sink_to_stop.get());
    sink_to_stop->Stop();
  }
}

AudioRendererSinkCache::CacheContainer::iterator
AudioRendererSinkCache::FindCacheEntry_Locked(
    const LocalFrameToken& source_frame_token,
    const std::string& device_id) {
  cache_lock_.AssertAcquired();
  return base::ranges::find_if(
      cache_, [source_frame_token, &device_id](const CacheEntry& val) {
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

void AudioRendererSinkCache::MaybeCacheSink(
    const LocalFrameToken& source_frame_token,
    const std::string& device_id,
    scoped_refptr<media::AudioRendererSink> sink) {
  if (!SinkIsHealthy(sink.get())) {
    TRACE_EVENT_INSTANT0("audio", "MaybeCacheSink: Unhealthy sink",
                         TRACE_EVENT_SCOPE_THREAD);
    // Since |sink| is not cached, we must make sure to Stop it now.
    sink->Stop();
    return;
  }

  CacheEntry cache_entry = {source_frame_token, device_id, std::move(sink)};

  {
    base::AutoLock auto_lock(cache_lock_);
    cache_.push_back(cache_entry);
  }

  DeleteLater(cache_entry.sink);
}

void AudioRendererSinkCache::DropSinksForFrame(
    const LocalFrameToken& source_frame_token) {
  base::AutoLock auto_lock(cache_lock_);
  WTF::EraseIf(cache_, [source_frame_token](const CacheEntry& val) {
    if (val.source_frame_token == source_frame_token) {
      val.sink->Stop();
      return true;
    }
    return false;
  });
}

wtf_size_t AudioRendererSinkCache::GetCacheSizeForTesting() {
  base::AutoLock auto_lock(cache_lock_);
  return cache_.size();
}

}  // namespace blink
