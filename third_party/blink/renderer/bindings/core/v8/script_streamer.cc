// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_streamer_thread.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/script/classic_pending_script.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/background_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"

namespace blink {

// For passing data between the main thread (producer) and the streamer thread
// (consumer). The main thread prepares the data (copies it from Resource) and
// the streamer thread feeds it to V8.
class SourceStreamDataQueue {
  WTF_MAKE_NONCOPYABLE(SourceStreamDataQueue);

 public:
  SourceStreamDataQueue() : finished_(false), have_data_(mutex_) {}
  ~SourceStreamDataQueue() { DiscardQueuedData(); }

  void Clear() {
    MutexLocker locker(mutex_);
    finished_ = false;
    DiscardQueuedData();
  }

  void Produce(const uint8_t* data, size_t length) {
    MutexLocker locker(mutex_);
    DCHECK(!finished_);
    data_.push_back(std::make_pair(data, length));
    have_data_.Signal();
  }

  void Finish() {
    MutexLocker locker(mutex_);
    finished_ = true;
    have_data_.Signal();
  }

  void Consume(const uint8_t** data, size_t* length) {
    MutexLocker locker(mutex_);
    while (!TryGetData(data, length))
      have_data_.Wait();
  }

 private:
  bool TryGetData(const uint8_t** data, size_t* length) {
    mutex_.AssertAcquired();
    if (!data_.IsEmpty()) {
      std::pair<const uint8_t*, size_t> next_data = data_.TakeFirst();
      *data = next_data.first;
      *length = next_data.second;
      return true;
    }
    if (finished_) {
      *length = 0;
      return true;
    }
    return false;
  }

  void DiscardQueuedData() {
    while (!data_.IsEmpty()) {
      std::pair<const uint8_t*, size_t> next_data = data_.TakeFirst();
      delete[] next_data.first;
    }
  }

  Deque<std::pair<const uint8_t*, size_t>> data_;
  bool finished_;
  Mutex mutex_;
  ThreadCondition have_data_;
};

// SourceStream implements the streaming interface towards V8. The main
// functionality is preparing the data to give to V8 on main thread, and
// actually giving the data (via GetMoreData which is called on a background
// thread).
class SourceStream : public v8::ScriptCompiler::ExternalSourceStream {
  WTF_MAKE_NONCOPYABLE(SourceStream);

 public:
  SourceStream()
      : v8::ScriptCompiler::ExternalSourceStream(),
        cancelled_(false),
        finished_(false),
        queue_lead_position_(0),
        queue_tail_position_(0) {}

  ~SourceStream() override = default;

  // Called by V8 on a background thread. Should block until we can return
  // some data.
  size_t GetMoreData(const uint8_t** src) override {
    DCHECK(!IsMainThread());
    {
      MutexLocker locker(mutex_);
      if (cancelled_)
        return 0;
    }
    size_t length = 0;
    // This will wait until there is data.
    data_queue_.Consume(src, &length);
    {
      MutexLocker locker(mutex_);
      if (cancelled_)
        return 0;
    }
    queue_lead_position_ += length;
    return length;
  }

  void DidFinishLoading() {
    DCHECK(IsMainThread());
    finished_ = true;
    data_queue_.Finish();
  }

  void DidReceiveData(ScriptResource* resource, ScriptStreamer* streamer) {
    DCHECK(IsMainThread());
    PrepareDataOnMainThread(resource, streamer);
  }

  void Cancel() {
    DCHECK(IsMainThread());
    // The script is no longer needed by the upper layers. Stop streaming
    // it. The next time GetMoreData is called (or woken up), it will return
    // 0, which will be interpreted as EOS by V8 and the parsing will
    // fail. ScriptStreamer::streamingComplete will be called, and at that
    // point we will release the references to SourceStream.
    {
      MutexLocker locker(mutex_);
      cancelled_ = true;
    }
    data_queue_.Finish();
  }

 private:
  void PrepareDataOnMainThread(ScriptResource* resource,
                               ScriptStreamer* streamer) {
    DCHECK(IsMainThread());

    if (cancelled_) {
      data_queue_.Finish();
      return;
    }

    // The Resource must still be alive; otherwise we should've cancelled
    // the streaming (if we have cancelled, the background thread is not
    // waiting).
    DCHECK(resource);

    if (V8CodeCache::HasCodeCache(resource->CacheHandler())) {
      // The resource has a code cache entry, so it's unnecessary to stream
      // and parse the code. Cancel the streaming and resume the non-streaming
      // code path which will consume the code cache.
      streamer->SuppressStreaming(ScriptStreamer::kHasCodeCache);
      Cancel();
      return;
    }

    if (!resource_buffer_) {
      // We don't have a buffer yet. Try to get it from the resource.
      resource_buffer_ = resource->ResourceBuffer();
    }

    FetchDataFromResourceBuffer();
  }

  void FetchDataFromResourceBuffer() {
    DCHECK(IsMainThread());
    MutexLocker locker(mutex_);

    DCHECK(!finished_);
    if (cancelled_) {
      data_queue_.Finish();
      return;
    }

    // Get as much data from the ResourceBuffer as we can in one chunk.
    const size_t length = resource_buffer_->size() - queue_tail_position_;

    uint8_t* const copied_data = new uint8_t[length];
    size_t pos = 0;

    for (auto it = resource_buffer_->GetIteratorAt(queue_tail_position_);
         it != resource_buffer_->end(); ++it) {
      memcpy(copied_data + pos, it->data(), it->size());
      pos += it->size();
    }
    DCHECK_EQ(pos, length);
    queue_tail_position_ = resource_buffer_->size();
    data_queue_.Produce(copied_data, length);
  }

  // For coordinating between the main thread and background thread tasks.
  // Guards m_cancelled and m_queueTailPosition.
  Mutex mutex_;

  // The shared buffer containing the resource data + state variables.
  // Used by both threads, guarded by m_mutex.
  bool cancelled_;
  bool finished_;

  scoped_refptr<const SharedBuffer>
      resource_buffer_;  // Only used by the main thread.

  // The queue contains the data to be passed to the V8 thread.
  //   queueLeadPosition: data we have handed off to the V8 thread.
  //   queueTailPosition: end of data we have enqued in the queue.
  //   bookmarkPosition: position of the bookmark.
  SourceStreamDataQueue data_queue_;  // Thread safe.
  size_t queue_lead_position_;        // Only used by v8 thread.
  size_t queue_tail_position_;  // Used by both threads; guarded by m_mutex.
};

size_t ScriptStreamer::small_script_threshold_ = 30 * 1024;

bool ScriptStreamer::ConvertEncoding(
    const char* encoding_name,
    v8::ScriptCompiler::StreamedSource::Encoding* encoding) {
  // Here's a list of encodings we can use for streaming. These are
  // the canonical names.
  if (strcmp(encoding_name, "windows-1252") == 0 ||
      strcmp(encoding_name, "ISO-8859-1") == 0 ||
      strcmp(encoding_name, "US-ASCII") == 0) {
    *encoding = v8::ScriptCompiler::StreamedSource::ONE_BYTE;
    return true;
  }
  if (strcmp(encoding_name, "UTF-8") == 0) {
    *encoding = v8::ScriptCompiler::StreamedSource::UTF8;
    return true;
  }
  // We don't stream other encodings; especially we don't stream two
  // byte scripts to avoid the handling of endianness. Most scripts
  // are Latin1 or UTF-8 anyway, so this should be enough for most
  // real world purposes.
  return false;
}

bool ScriptStreamer::IsFinished() const {
  DCHECK(IsMainThread());
  return loading_finished_ && (parsing_finished_ || streaming_suppressed_);
}

bool ScriptStreamer::IsStreamingFinished() const {
  DCHECK(IsMainThread());
  return parsing_finished_ || streaming_suppressed_;
}

void ScriptStreamer::StreamingCompleteOnBackgroundThread() {
  DCHECK(!IsMainThread());

  // notifyFinished might already be called, or it might be called in the
  // future (if the parsing finishes earlier because of a parse error).
  PostCrossThreadTask(*loading_task_runner_, FROM_HERE,
                      CrossThreadBind(&ScriptStreamer::StreamingComplete,
                                      WrapCrossThreadPersistent(this)));

  // The task might delete ScriptStreamer, so it's not safe to do anything
  // after posting it. Note that there's no way to guarantee that this
  // function has returned before the task is ran - however, we should not
  // access the "this" object after posting the task.
}

void ScriptStreamer::Cancel() {
  DCHECK(IsMainThread());
  // The upper layer doesn't need the script any more, but streaming might
  // still be ongoing. Tell SourceStream to try to cancel it whenever it gets
  // the control the next time. It can also be that V8 has already completed
  // its operations and streamingComplete will be called soon.
  detached_ = true;
  if (stream_)
    stream_->Cancel();
}

void ScriptStreamer::SuppressStreaming(NotStreamingReason reason) {
  DCHECK(IsMainThread());
  DCHECK(!loading_finished_);
  DCHECK_NE(reason, NotStreamingReason::kInvalid);

  // It can be that the parsing task has already finished (e.g., if there was
  // a parse error).
  streaming_suppressed_ = true;
  suppressed_reason_ = reason;
}

namespace {

void RunScriptStreamingTask(
    std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask> task,
    ScriptStreamer* streamer) {
  TRACE_EVENT1(
      "v8,devtools.timeline", "v8.parseOnBackground", "data",
      InspectorParseScriptEvent::Data(streamer->ScriptResourceIdentifier(),
                                      streamer->ScriptURLString()));
  // Running the task can and will block: SourceStream::GetSomeData will get
  // called and it will block and wait for data from the network.
  task->Run();
  streamer->StreamingCompleteOnBackgroundThread();
}

void RunBlockingScriptStreamingTask(
    std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask> task,
    ScriptStreamer* streamer,
    std::atomic_flag* blocking_task_started_or_cancelled) {
  if (blocking_task_started_or_cancelled->test_and_set())
    return;
  RunScriptStreamingTask(std::move(task), streamer);
}

void RunNonBlockingScriptStreamingTask(
    std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask> task,
    ScriptStreamer* streamer) {
  RunScriptStreamingTask(std::move(task), streamer);
}

}  // namespace

bool ScriptStreamer::HasEnoughDataForStreaming(size_t resource_buffer_size) {
  // Only stream larger scripts.
  return resource_buffer_size >= small_script_threshold_;
}

void ScriptStreamer::NotifyAppendData(ScriptResource* resource) {
  DCHECK(IsMainThread());
  if (streaming_suppressed_)
    return;
  if (!have_enough_data_for_streaming_) {
    // Even if the first data chunk is small, the script can still be big
    // enough - wait until the next data chunk comes before deciding whether
    // to start the streaming.
    DCHECK(resource->ResourceBuffer());
    if (!HasEnoughDataForStreaming(resource->ResourceBuffer()->size()))
      return;
    have_enough_data_for_streaming_ = true;

    {
      // Check for BOM (byte order marks), because that might change our
      // understanding of the data encoding.
      char maybe_bom[kMaximumLengthOfBOM] = {};
      if (!resource->ResourceBuffer()->GetBytes(maybe_bom,
                                                kMaximumLengthOfBOM)) {
        NOTREACHED();
        return;
      }

      std::unique_ptr<TextResourceDecoder> decoder(
          TextResourceDecoder::Create(TextResourceDecoderOptions(
              TextResourceDecoderOptions::kPlainTextContent,
              WTF::TextEncoding(resource->Encoding()))));
      decoder->CheckForBOM(maybe_bom, kMaximumLengthOfBOM);

      // The encoding may change when we see the BOM. Check for BOM now
      // and update the encoding from the decoder when necessary. Supress
      // streaming if the encoding is unsupported.
      //
      // Also note that have at least s_smallScriptThreshold worth of
      // data, which is more than enough for detecting a BOM.
      if (!ConvertEncoding(decoder->Encoding().GetName(), &encoding_)) {
        SuppressStreaming(kEncodingNotSupported);
        return;
      }
    }

    if (!RuntimeEnabledFeatures::ScheduledScriptStreamingEnabled() &&
        ScriptStreamerThread::Shared()->IsRunningTask()) {
      // If scheduled script streaming is disabled, we only have one thread for
      // running the tasks. A new task shouldn't be queued before the running
      // task completes, because the running task can block and wait for data
      // from the network.
      SuppressStreaming(kThreadBusy);
      return;
    }

    DCHECK(!stream_);
    DCHECK(!source_);
    stream_ = new SourceStream;
    // m_source takes ownership of m_stream.
    source_ = std::make_unique<v8::ScriptCompiler::StreamedSource>(stream_,
                                                                   encoding_);

    std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask>
        script_streaming_task(
            base::WrapUnique(v8::ScriptCompiler::StartStreamingScript(
                V8PerIsolateData::MainThreadIsolate(), source_.get(),
                compile_options_)));
    if (!script_streaming_task) {
      // V8 cannot stream the script.
      SuppressStreaming(kV8CannotStream);
      stream_ = nullptr;
      source_.reset();
      return;
    }

    if (RuntimeEnabledFeatures::ScheduledScriptStreamingEnabled()) {
      // Script streaming tasks are high priority, as they can block the parser,
      // and they can (and probably will) block during their own execution as
      // they wait for more input.
      //
      // Pass through the atomic cancellation token which is set to true by the
      // task when it is started, or set to true by the streamer if it wants to
      // cancel the task.
      //
      // TODO(leszeks): Decrease the priority of these tasks where possible.
      background_scheduler::PostOnBackgroundThreadWithTraits(
          FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
          CrossThreadBind(RunBlockingScriptStreamingTask,
                          WTF::Passed(std::move(script_streaming_task)),
                          WrapCrossThreadPersistent(this),
                          WTF::CrossThreadUnretained(
                              &blocking_task_started_or_cancelled_)));
    } else {
      blocking_task_started_or_cancelled_.test_and_set();
      ScriptStreamerThread::Shared()->PostTask(
          CrossThreadBind(&ScriptStreamerThread::RunScriptStreamingTask,
                          WTF::Passed(std::move(script_streaming_task)),
                          WrapCrossThreadPersistent(this)));
    }

  }
  if (stream_)
    stream_->DidReceiveData(resource, this);
}

void ScriptStreamer::NotifyFinished() {
  DCHECK(IsMainThread());
  // A special case: empty and small scripts. We didn't receive enough data to
  // start the streaming before this notification. In that case, there won't
  // be a "parsing complete" notification either, and we should not wait for
  // it.
  if (!have_enough_data_for_streaming_) {
    SuppressStreaming(kScriptTooSmall);
  }

  if (stream_) {
    // Mark the stream as finished loading before potentially re-posting the
    // task to avoid a race between this finish and the task's first read.
    stream_->DidFinishLoading();

    // If the corresponding blocking task hasn't started yet, cancel it and post
    // a non-blocking task, since we know now that all the data is received and
    // we will no longer block.
    //
    // TODO(874080): Once we have mutable task traits, simply unmark the
    // existing task as no longer MayBlock.
    if (RuntimeEnabledFeatures::ScheduledScriptStreamingEnabled() &&
        !blocking_task_started_or_cancelled_.test_and_set()) {
      std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask>
          script_streaming_task(
              base::WrapUnique(v8::ScriptCompiler::StartStreamingScript(
                  V8PerIsolateData::MainThreadIsolate(), source_.get(),
                  compile_options_)));

      // The task creation shouldn't fail, since it didn't fail before during
      // NotifyAppendData.
      CHECK(script_streaming_task);
      background_scheduler::PostOnBackgroundThreadWithTraits(
          FROM_HERE, {base::TaskPriority::USER_BLOCKING},
          CrossThreadBind(RunNonBlockingScriptStreamingTask,
                          WTF::Passed(std::move(script_streaming_task)),
                          WrapCrossThreadPersistent(this)));
    }
  }
  loading_finished_ = true;

  NotifyFinishedToClient();
}

ScriptStreamer::ScriptStreamer(
    ClassicPendingScript* script,
    v8::ScriptCompiler::CompileOptions compile_options,
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner)
    : pending_script_(script),
      detached_(false),
      stream_(nullptr),
      loading_finished_(false),
      parsing_finished_(false),
      have_enough_data_for_streaming_(false),
      streaming_suppressed_(false),
      suppressed_reason_(kInvalid),
      compile_options_(compile_options),
      script_url_string_(script->GetResource()->Url().Copy().GetString()),
      script_resource_identifier_(script->GetResource()->Identifier()),
      // Unfortunately there's no dummy encoding value in the enum; let's use
      // one we don't stream.
      encoding_(v8::ScriptCompiler::StreamedSource::TWO_BYTE),
      loading_task_runner_(std::move(loading_task_runner)) {}

ScriptStreamer::~ScriptStreamer() = default;

void ScriptStreamer::Prefinalize() {
  Cancel();
  prefinalizer_called_ = true;
}

void ScriptStreamer::Trace(blink::Visitor* visitor) {
  visitor->Trace(pending_script_);
}

void ScriptStreamer::StreamingComplete() {
  // The background task is completed; do the necessary ramp-down in the main
  // thread.
  DCHECK(IsMainThread());
  parsing_finished_ = true;

  // It's possible that the corresponding Resource was deleted before V8
  // finished streaming. In that case, the data or the notification is not
  // needed. In addition, if the streaming is suppressed, the non-streaming
  // code path will resume after the resource has loaded, before the
  // background task finishes.
  if (detached_ || streaming_suppressed_)
    return;

  // We have now streamed the whole script to V8 and it has parsed the
  // script. We're ready for the next step: compiling and executing the
  // script.
  NotifyFinishedToClient();
}

void ScriptStreamer::NotifyFinishedToClient() {
  DCHECK(IsMainThread());
  // Usually, the loading will be finished first, and V8 will still need some
  // time to catch up. But the other way is possible too: if V8 detects a
  // parse error, the V8 side can complete before loading has finished. Send
  // the notification after both loading and V8 side operations have
  // completed.
  if (!IsFinished())
    return;

  pending_script_->StreamingFinished();
}

void ScriptStreamer::StartStreaming(
    ClassicPendingScript* script,
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
    NotStreamingReason* not_streaming_reason) {
  DCHECK(IsMainThread());
  *not_streaming_reason = kInvalid;
  ScriptResource* resource = ToScriptResource(script->GetResource());
  if (!resource->Url().ProtocolIsInHTTPFamily()) {
    *not_streaming_reason = kNotHTTP;
    return;
  }
  if (resource->IsCacheValidator()) {
    // This happens e.g., during reloads. We're actually not going to load
    // the current Resource of the ClassicPendingScript but switch to another
    // Resource -> don't stream.
    *not_streaming_reason = kReload;
    return;
  }
  if (resource->IsLoaded() && !resource->ResourceBuffer()) {
    // This happens for already loaded resources, e.g. if resource
    // validation fails. In that case, the loading subsystem will discard
    // the resource buffer.
    *not_streaming_reason = kNoResourceBuffer;
    return;
  }
  // We cannot filter out short scripts, even if we wait for the HTTP headers
  // to arrive: the Content-Length HTTP header is not sent for chunked
  // downloads.

  ScriptStreamer* streamer =
      new ScriptStreamer(script, v8::ScriptCompiler::kNoCompileOptions,
                         std::move(loading_task_runner));

  // If this script was ready when streaming began, no callbacks will be
  // received to populate the data for the ScriptStreamer, so send them now.
  // Note that this script may be processing an asynchronous cache hit, in
  // which case ScriptResource::IsLoaded() will be true, but ready_state_ will
  // not be kReadyStreaming. In that case, ScriptStreamer can listen to the
  // async callbacks generated by the cache hit.
  if (script->IsReady()) {
    DCHECK(resource->IsLoaded());
    streamer->NotifyAppendData(resource);
    if (streamer->StreamingSuppressed()) {
      *not_streaming_reason = streamer->StreamingSuppressedReason();
      return;
    }
  }

  // The Resource might go out of scope if the script is no longer needed.
  // This makes ClassicPendingScript notify the ScriptStreamer when it is
  // destroyed.
  script->SetStreamer(streamer);

  if (script->IsReady())
    streamer->NotifyFinished();
}

}  // namespace blink
