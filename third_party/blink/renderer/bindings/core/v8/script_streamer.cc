// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "mojo/public/cpp/system/wait.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"

namespace blink {

// SourceStream implements the streaming interface towards V8. The main
// functionality is preparing the data to give to V8 on main thread, and
// actually giving the data (via GetMoreData which is called on a background
// thread).
class SourceStream : public v8::ScriptCompiler::ExternalSourceStream {
 public:
  SourceStream() = default;
  ~SourceStream() override = default;

  // Called by V8 on a background thread. Should block until we can return
  // some data. Ownership of the |src| data buffer is passed to the caller,
  // unless |src| is null.
  size_t GetMoreData(const uint8_t** src) override {
    DCHECK(!IsMainThread());
    CHECK(ready_to_run_.IsSet());

    if (load_state_ != ScriptStreamer::LoadingState::kLoading) {
      return 0;
    }

    if (cancelled_.IsSet()) {
      SetFinished(ScriptStreamer::LoadingState::kCancelled);
      return 0;
    }

    if (initial_data_) {
      CHECK_GT(initial_data_len_, 0u);
      if (src) {
        *src = initial_data_.release();
      } else {
        initial_data_.reset();
      }
      size_t len = initial_data_len_;
      initial_data_len_ = 0;
      return len;
    }

    CHECK(!initial_data_);
    CHECK_EQ(initial_data_len_, 0u);
    CHECK(data_pipe_.is_valid());

    // Start a new two-phase read, blocking until data is available.
    while (true) {
      const void* buffer;
      uint32_t num_bytes;
      MojoResult result = data_pipe_->BeginReadData(&buffer, &num_bytes,
                                                    MOJO_READ_DATA_FLAG_NONE);

      switch (result) {
        case MOJO_RESULT_OK: {
          // num_bytes could only be 0 if the handle was being read elsewhere.
          CHECK_GT(num_bytes, 0u);

          if (src) {
            auto copy_for_script_stream =
                std::make_unique<uint8_t[]>(num_bytes);
            memcpy(copy_for_script_stream.get(), buffer, num_bytes);
            *src = copy_for_script_stream.release();
          }

          // TODO(leszeks): It would be nice to get rid of this second copy, and
          // either share ownership of the chunks, or only give chunks back to
          // the client once the streaming completes.
          auto copy_for_resource = std::make_unique<char[]>(num_bytes);
          memcpy(copy_for_resource.get(), buffer, num_bytes);
          PostCrossThreadTask(
              *loading_task_runner_, FROM_HERE,
              CrossThreadBindOnce(
                  NotifyClientDidReceiveData, response_body_loader_client_,
                  WTF::Passed(std::move(copy_for_resource)), num_bytes));

          result = data_pipe_->EndReadData(num_bytes);
          CHECK_EQ(result, MOJO_RESULT_OK);

          return num_bytes;
        }

        case MOJO_RESULT_SHOULD_WAIT: {
          {
            TRACE_EVENT_END0(
                "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                "v8.parseOnBackgroundParsing");
            TRACE_EVENT_BEGIN0(
                "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                "v8.parseOnBackgroundWaiting");
            base::ScopedAllowBaseSyncPrimitives
                scoped_allow_base_sync_primitives;
            base::ScopedBlockingCall scoped_blocking_call(
                FROM_HERE, base::BlockingType::WILL_BLOCK);

            result = mojo::Wait(data_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE);
            TRACE_EVENT_END0(
                "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                "v8.parseOnBackgroundWaiting");
            TRACE_EVENT_BEGIN0(
                "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                "v8.parseOnBackgroundParsing");
          }

          if (result != MOJO_RESULT_OK) {
            // If the producer handle was closed, then treat as EOF.
            CHECK_EQ(result, MOJO_RESULT_FAILED_PRECONDITION);
            SetFinished(ScriptStreamer::LoadingState::kLoaded);
            return 0;
          }

          // We were blocked, so check for cancelation again.
          if (cancelled_.IsSet()) {
            SetFinished(ScriptStreamer::LoadingState::kCancelled);
            return 0;
          }

          // Loop to read the data.
          continue;
        }

        case MOJO_RESULT_FAILED_PRECONDITION:
          // If the producer handle was closed, then treat as EOF.
          SetFinished(ScriptStreamer::LoadingState::kLoaded);
          return 0;

        default:
          // Some other error occurred.
          SetFinished(ScriptStreamer::LoadingState::kFailed);
          return 0;
      }
    }
  }

  void DrainRemainingDataWithoutStreaming() {
    DCHECK(!IsMainThread());
    if (load_state_ == ScriptStreamer::LoadingState::kLoading) {
      // Keep reading data until we finish (returning 0). It won't be streaming
      // compiled any more, but it will continue being forwarded to the client.
      while (GetMoreData(nullptr) != 0) {
      }
    }
    CHECK_NE(load_state_, ScriptStreamer::LoadingState::kLoading);
  }

  void Cancel() {
    DCHECK(IsMainThread());
    // The script is no longer needed by the upper layers. Stop streaming
    // it. The next time GetMoreData is called (or woken up), it will return
    // 0, which will be interpreted as EOS by V8 and the parsing will
    // fail. ScriptStreamer::StreamingComplete will be called, and at that
    // point we will release the references to SourceStream.
    cancelled_.Set();
  }

  void TakeDataAndPipeOnMainThread(
      ScriptResource* resource,
      ScriptStreamer* streamer,
      mojo::ScopedDataPipeConsumerHandle data_pipe,
      ResponseBodyLoaderClient* response_body_loader_client,
      scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner) {
    DCHECK(IsMainThread());
    CHECK(data_pipe);
    CHECK(!ready_to_run_.IsSet());
    CHECK(!cancelled_.IsSet());

    // The Resource must still be alive; otherwise we should've cancelled
    // the streaming (if we have cancelled, the background thread is not
    // waiting).
    DCHECK(resource);

    const SharedBuffer* resource_buffer = resource->ResourceBuffer().get();

    CHECK(!initial_data_);
    CHECK_EQ(initial_data_len_, 0u);

    // Get the data that is already in the ResourceBuffer.
    const size_t length = resource_buffer->size();

    if (length > 0) {
      initial_data_.reset(new uint8_t[length]);

      bool success = resource_buffer->GetBytes(
          reinterpret_cast<void*>(initial_data_.get()), length);
      CHECK(success);

      initial_data_len_ = length;
    }

    data_pipe_ = std::move(data_pipe);
    response_body_loader_client_ = response_body_loader_client;
    loading_task_runner_ = loading_task_runner;

    CHECK(data_pipe_);
    ready_to_run_.Set();
  }

  ScriptStreamer::LoadingState LoadingState() const { return load_state_; }

 private:
  static void NotifyClientDidReceiveData(
      ResponseBodyLoaderClient* response_body_loader_client,
      std::unique_ptr<char[]> data,
      uint32_t data_size) {
    // The response_body_loader_client is held weakly, so it may be dead by the
    // time this callback is called. If so, we can simply drop this chunk.
    if (!response_body_loader_client)
      return;

    response_body_loader_client->DidReceiveData(
        base::make_span(data.get(), data_size));
  }

  void SetFinished(ScriptStreamer::LoadingState state) { load_state_ = state; }

  // TODO(leszeks): Make this a DCHECK-only flag.
  base::AtomicFlag ready_to_run_;
  base::AtomicFlag cancelled_;

  // Only used by background thread
  ScriptStreamer::LoadingState load_state_ =
      ScriptStreamer::LoadingState::kLoading;

  // The initial data that was already on the Resource, rather than being read
  // directly from the data pipe.
  std::unique_ptr<uint8_t[]> initial_data_;
  size_t initial_data_len_ = 0;

  mojo::ScopedDataPipeConsumerHandle data_pipe_;
  CrossThreadWeakPersistent<ResponseBodyLoaderClient>
      response_body_loader_client_;
  scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SourceStream);
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

bool ScriptStreamer::IsStreamingStarted() const {
  DCHECK(IsMainThread());
  return !!stream_;
}

bool ScriptStreamer::IsStreamingSuppressed() const {
  DCHECK(IsMainThread());
  return suppressed_reason_ != NotStreamingReason::kInvalid;
}

bool ScriptStreamer::IsLoaded() const {
  DCHECK(IsMainThread());
  return loading_state_ != LoadingState::kLoading;
}

bool ScriptStreamer::CanStartStreaming() const {
  DCHECK(IsMainThread());
  return !IsStreamingStarted() && !IsStreamingSuppressed();
}

bool ScriptStreamer::IsFinished() const {
  DCHECK(IsMainThread());
  // We are finished when we know that we won't start streaming later (either
  // because we are streaming already or streaming was suppressed).
  return IsLoaded() && !CanStartStreaming();
}

bool ScriptStreamer::IsClientDetached() const {
  DCHECK(IsMainThread());
  return !response_body_loader_client_;
}

void ScriptStreamer::StreamingCompleteOnBackgroundThread(LoadingState state) {
  DCHECK(!IsMainThread());

  // notifyFinished might already be called, or it might be called in the
  // future (if the parsing finishes earlier because of a parse error).
  PostCrossThreadTask(
      *loading_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&ScriptStreamer::StreamingComplete,
                          WrapCrossThreadPersistent(this), state));

  // The task might be the only remaining reference to the ScriptStreamer, and
  // there's no way to guarantee that this function has returned before the task
  // is ran, so we should not access the "this" object after posting the task.
}

void ScriptStreamer::Cancel() {
  DCHECK(IsMainThread());
  // The upper layer doesn't need the script any more, but streaming might
  // still be ongoing. Tell SourceStream to try to cancel it whenever it gets
  // the control the next time. It can also be that V8 has already completed
  // its operations and streamingComplete will be called soon.
  response_body_loader_client_.Release();
  script_resource_.Release();
  if (stream_)
    stream_->Cancel();
  CHECK(IsClientDetached());
}

void ScriptStreamer::SuppressStreaming(NotStreamingReason reason) {
  DCHECK(IsMainThread());
  CHECK_EQ(suppressed_reason_, NotStreamingReason::kInvalid);
  CHECK_NE(reason, NotStreamingReason::kInvalid);
  suppressed_reason_ = reason;
}

void ScriptStreamer::RunScriptStreamingTask(
    std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask> task,
    ScriptStreamer* streamer,
    SourceStream* stream) {
  // TODO(leszeks): Add flow event data again
  TRACE_EVENT_BEGIN1(
      "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
      "v8.parseOnBackground", "data",
      inspector_parse_script_event::Data(streamer->ScriptResourceIdentifier(),
                                         streamer->ScriptURLString()));

  TRACE_EVENT_BEGIN0(
      "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
      "v8.parseOnBackgroundParsing");
  // Running the task can and will block: SourceStream::GetSomeData will get
  // called and it will block and wait for data from the network.
  task->Run();

  // V8 may have exited early due to a parsing error, so make sure we finish
  // draining the datapipe to the client.
  // TODO(leszeks): This could be done asynchronously, using a mojo watcher.
  stream->DrainRemainingDataWithoutStreaming();

  TRACE_EVENT_END0(
      "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
      "v8.parseOnBackgroundParsing");

  // Send a single callback back to the streamer signifying that the streaming
  // is complete, and how it completed (success/fail/cancelled). The streamer
  // will forward the state to the client on the main thread. We don't send the
  // success/fail/cancelled client callback in separate tasks, as they can kill
  // the (context-specific) task runner, which would make this StreamingComplete
  // afterward fail to post.
  streamer->StreamingCompleteOnBackgroundThread(stream->LoadingState());

  TRACE_EVENT_END0(
      "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
      "v8.parseOnBackground");

  // TODO(crbug.com/1021571); Remove this once the last event stops being
  // dropped.
  TRACE_EVENT_END0(
      "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
      "v8.parseOnBackground2");
}

bool ScriptStreamer::HasEnoughDataForStreaming(size_t resource_buffer_size) {
  if (base::FeatureList::IsEnabled(features::kSmallScriptStreaming)) {
    return resource_buffer_size >= kMaximumLengthOfBOM;
  } else {
    // Only stream larger scripts.
    return resource_buffer_size >= small_script_threshold_;
  }
}

// Try to start a task streaming the script from the datapipe, with the task
// taking ownership of the datapipe and weak ownership of the client. Returns
// true if streaming succeeded and false otherwise.
//
// Streaming may fail to start because:
//
//   * The encoding is invalid (not UTF-8 or one-byte data)
//   * The script is too small (see HasEnoughDataForStreaming)
//   * There is a code cache for this script already
//   * V8 failed to create a script streamer
//
// If this method returns true, the datapipe handle will be cleared and the
// streaming task becomes responsible for draining the datapipe and forwarding
// data to the client. Otherwise, we should continue as if this were a no-op.
bool ScriptStreamer::TryStartStreamingTask() {
  DCHECK(IsMainThread());
  if (!CanStartStreaming())
    return false;

  // Even if the first data chunk is small, the script can still be big enough -
  // wait until the next data chunk comes before deciding whether to start the
  // streaming.
  if (!script_resource_->ResourceBuffer() ||
      !HasEnoughDataForStreaming(script_resource_->ResourceBuffer()->size())) {
    CHECK(!IsLoaded());
    return false;
  }

  {
    // Check for BOM (byte order marks), because that might change our
    // understanding of the data encoding.
    char maybe_bom[kMaximumLengthOfBOM] = {};
    if (!script_resource_->ResourceBuffer()->GetBytes(maybe_bom,
                                                      kMaximumLengthOfBOM)) {
      NOTREACHED();
      return false;
    }

    std::unique_ptr<TextResourceDecoder> decoder(
        std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
            TextResourceDecoderOptions::kPlainTextContent,
            WTF::TextEncoding(script_resource_->Encoding()))));
    decoder->CheckForBOM(maybe_bom, kMaximumLengthOfBOM);

    // The encoding may change when we see the BOM. Check for BOM now
    // and update the encoding from the decoder when necessary. Suppress
    // streaming if the encoding is unsupported.
    //
    // Also note that have at least s_smallScriptThreshold worth of
    // data, which is more than enough for detecting a BOM.
    if (!ConvertEncoding(decoder->Encoding().GetName(), &encoding_)) {
      SuppressStreaming(NotStreamingReason::kEncodingNotSupported);
      return false;
    }
  }

  if (V8CodeCache::HasCodeCache(script_resource_->CacheHandler())) {
    // The resource has a code cache entry, so it's unnecessary to stream
    // and parse the code.
    // TODO(leszeks): Can we even reach this code path with data pipes?
    stream_ = nullptr;
    source_.reset();
    SuppressStreaming(ScriptStreamer::NotStreamingReason::kHasCodeCache);
    return false;
  }

  DCHECK(!stream_);
  DCHECK(!source_);
  auto stream_ptr = std::make_unique<SourceStream>();
  stream_ = stream_ptr.get();
  // |source_| takes ownership of |stream_|, and will keep |stream_| alive until
  // |source_| is destructed.
  source_ = std::make_unique<v8::ScriptCompiler::StreamedSource>(
      std::move(stream_ptr), encoding_);

  std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask>
      script_streaming_task(
          base::WrapUnique(v8::ScriptCompiler::StartStreamingScript(
              V8PerIsolateData::MainThreadIsolate(), source_.get(),
              compile_options_)));
  if (!script_streaming_task) {
    // V8 cannot stream the script.
    stream_ = nullptr;
    source_.reset();
    SuppressStreaming(NotStreamingReason::kV8CannotStream);
    return false;
  }

  TRACE_EVENT_WITH_FLOW1(
      TRACE_DISABLED_BY_DEFAULT("v8.compile"), "v8.streamingCompile.start",
      this, TRACE_EVENT_FLAG_FLOW_OUT, "data",
      inspector_parse_script_event::Data(this->ScriptResourceIdentifier(),
                                         this->ScriptURLString()));

  stream_->TakeDataAndPipeOnMainThread(
      script_resource_, this, std::move(data_pipe_),
      response_body_loader_client_.Get(), loading_task_runner_);

  // This reset will also cancel the watcher.
  watcher_.reset();

  // Script streaming tasks are high priority, as they can block the parser,
  // and they can (and probably will) block during their own execution as
  // they wait for more input.
  // TODO(leszeks): Decrease the priority of these tasks where possible.
  worker_pool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      CrossThreadBindOnce(RunScriptStreamingTask,
                          WTF::Passed(std::move(script_streaming_task)),
                          WrapCrossThreadPersistent(this),
                          WTF::CrossThreadUnretained(stream_)));

  return true;
}

ScriptStreamer::ScriptStreamer(
    ScriptResource* script_resource,
    mojo::ScopedDataPipeConsumerHandle data_pipe,
    ResponseBodyLoaderClient* response_body_loader_client,
    v8::ScriptCompiler::CompileOptions compile_options,
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner)
    : script_resource_(script_resource),
      response_body_loader_client_(response_body_loader_client),
      data_pipe_(std::move(data_pipe)),
      compile_options_(compile_options),
      script_url_string_(script_resource->Url().Copy().GetString()),
      script_resource_identifier_(script_resource->InspectorId()),
      // Unfortunately there's no dummy encoding value in the enum; let's use
      // one we don't stream.
      encoding_(v8::ScriptCompiler::StreamedSource::TWO_BYTE),
      loading_task_runner_(std::move(loading_task_runner)) {
  watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      loading_task_runner_);

  watcher_->Watch(data_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                  MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                  WTF::BindRepeating(&ScriptStreamer::OnDataPipeReadable,
                                     WrapWeakPersistent(this)));

  MojoResult ready_result;
  mojo::HandleSignalsState ready_state;
  MojoResult rv = watcher_->Arm(&ready_result, &ready_state);
  if (rv == MOJO_RESULT_OK)
    return;

  DCHECK_EQ(MOJO_RESULT_FAILED_PRECONDITION, rv);
  OnDataPipeReadable(ready_result, ready_state);
}

void ScriptStreamer::OnDataPipeReadable(MojoResult result,
                                        const mojo::HandleSignalsState& state) {
  if (IsClientDetached())
    return;

  switch (result) {
    case MOJO_RESULT_OK:
      // All good, so read the data that we were notified that we received.
      break;

    case MOJO_RESULT_CANCELLED:
      // The consumer handle got closed, which means this script is done
      // loading, and did so without streaming (otherwise the watcher wouldn't
      // have been armed, and the handle ownership would have passed to the
      // streaming task.
      watcher_.reset();
      LoadCompleteWithoutStreaming(LoadingState::kCancelled,
                                   NotStreamingReason::kLoadingCancelled);
      return;

    case MOJO_RESULT_FAILED_PRECONDITION:
      // This means the producer finished and we never started streaming. This
      // must be because we suppressed streaming earlier, or never got enough
      // data to start streaming.
      CHECK(IsStreamingSuppressed() || !script_resource_->ResourceBuffer() ||
            !HasEnoughDataForStreaming(
                script_resource_->ResourceBuffer()->size()));
      watcher_.reset();
      // Pass kScriptTooSmall for the !IsStreamingSuppressed() case, it won't
      // override an existing streaming reason.
      LoadCompleteWithoutStreaming(LoadingState::kLoaded,
                                   NotStreamingReason::kScriptTooSmall);
      return;

    case MOJO_RESULT_SHOULD_WAIT:
      NOTREACHED();
      return;

    default:
      // Some other error occurred.
      watcher_.reset();
      LoadCompleteWithoutStreaming(LoadingState::kFailed,
                                   NotStreamingReason::kErrorOccurred);
      return;
  }
  CHECK(state.readable());
  CHECK(data_pipe_);

  const void* data;
  uint32_t data_size;
  MojoReadDataFlags flags_to_pass = MOJO_READ_DATA_FLAG_NONE;
  MojoResult begin_read_result =
      data_pipe_->BeginReadData(&data, &data_size, flags_to_pass);
  // There should be data, so this read should succeed.
  CHECK_EQ(begin_read_result, MOJO_RESULT_OK);

  response_body_loader_client_->DidReceiveData(
      base::make_span(reinterpret_cast<const char*>(data), data_size));

  MojoResult end_read_result = data_pipe_->EndReadData(data_size);

  CHECK_EQ(end_read_result, MOJO_RESULT_OK);

  if (TryStartStreamingTask()) {
    return;
  }

  // TODO(leszeks): Depending on how small the chunks are, we may want to
  // loop until a certain number of bytes are synchronously read rather than
  // going back to the scheduler.
  watcher_->ArmOrNotify();
}

ScriptStreamer::~ScriptStreamer() = default;

void ScriptStreamer::Prefinalize() {
  // Reset and cancel the watcher. This has to be called in the prefinalizer,
  // rather than relying on the destructor, as accesses by the watcher of the
  // script resource between prefinalization and destruction are invalid. See
  // https://crbug.com/905975#c34 for more details.
  watcher_.reset();

  // Cancel any on-going streaming.
  Cancel();
}

void ScriptStreamer::Trace(Visitor* visitor) const {
  visitor->Trace(script_resource_);
  visitor->Trace(response_body_loader_client_);
}

void ScriptStreamer::StreamingComplete(LoadingState loading_state) {
  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("v8.compile"), "v8.streamingCompile.complete",
      this, TRACE_EVENT_FLAG_FLOW_IN, "streaming_suppressed",
      IsStreamingSuppressed(), "data",
      inspector_parse_script_event::Data(this->ScriptResourceIdentifier(),
                                         this->ScriptURLString()));

  // The background task is completed; do the necessary ramp-down in the main
  // thread.
  DCHECK(IsMainThread());

  AdvanceLoadingState(loading_state);

  // Sending a finished notification to the client also indicates that streaming
  // completed.
  SendClientLoadFinishedCallback();
}

void ScriptStreamer::LoadCompleteWithoutStreaming(
    LoadingState state,
    NotStreamingReason no_streaming_reason) {
  // We might have previously suppressed streaming, in which case we want to
  // keep the previous reason and not re-suppress.
  if (!IsStreamingSuppressed()) {
    SuppressStreaming(no_streaming_reason);
  }
  AdvanceLoadingState(state);
  SendClientLoadFinishedCallback();
}

void ScriptStreamer::SendClientLoadFinishedCallback() {
  // Don't do anything if we're detached, there's no client to send signals to.
  if (IsClientDetached())
    return;

  CHECK(IsFinished());

  switch (loading_state_) {
    case LoadingState::kLoading:
      CHECK(false);
      break;
    case LoadingState::kCancelled:
      response_body_loader_client_->DidCancelLoadingBody();
      break;
    case LoadingState::kFailed:
      response_body_loader_client_->DidFailLoadingBody();
      break;
    case LoadingState::kLoaded:
      response_body_loader_client_->DidFinishLoadingBody();
      break;
  }

  response_body_loader_client_.Release();
}

void ScriptStreamer::AdvanceLoadingState(LoadingState new_state) {
  switch (loading_state_) {
    case LoadingState::kLoading:
      CHECK(new_state == LoadingState::kLoaded ||
            new_state == LoadingState::kFailed ||
            new_state == LoadingState::kCancelled);
      break;
    case LoadingState::kLoaded:
    case LoadingState::kFailed:
    case LoadingState::kCancelled:
      CHECK(false);
      break;
  }

  loading_state_ = new_state;
  CheckState();
}

void ScriptStreamer::CheckState() const {
  switch (loading_state_) {
    case LoadingState::kLoading:
      // If we are still loading, we either
      //   1) Are still waiting for enough data to come in to start streaming,
      //   2) Have already started streaming, or
      //   3) Have suppressed streaming.
      // TODO(leszeks): This check, with the current implementation, always
      // returns true. We should either try to check something stronger, or get
      // rid of it.
      CHECK(CanStartStreaming() || IsStreamingStarted() ||
            IsStreamingSuppressed());
      break;
    case LoadingState::kLoaded:
    case LoadingState::kFailed:
    case LoadingState::kCancelled:
      // Otherwise, if we aren't still loading, we either
      //   1) Have already started streaming, or
      //   2) Have suppressed streaming.
      CHECK(IsStreamingStarted() || IsStreamingSuppressed());
      break;
  }
}

}  // namespace blink
