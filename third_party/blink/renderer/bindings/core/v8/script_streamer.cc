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
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
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

    if (finished_) {
      return 0;
    }

    if (cancelled_.IsSet()) {
      SendLoadingFinishedCallback(
          &ResponseBodyLoaderClient::DidCancelLoadingBody);
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
            SendLoadingFinishedCallback(
                &ResponseBodyLoaderClient::DidFinishLoadingBody);
            return 0;
          }

          // We were blocked, so check for cancelation again.
          if (cancelled_.IsSet()) {
            SendLoadingFinishedCallback(
                &ResponseBodyLoaderClient::DidCancelLoadingBody);
            return 0;
          }

          // Loop to read the data.
          continue;
        }

        case MOJO_RESULT_FAILED_PRECONDITION:
          // If the producer handle was closed, then treat as EOF.
          SendLoadingFinishedCallback(
              &ResponseBodyLoaderClient::DidFinishLoadingBody);
          return 0;

        default:
          // Some other error occurred.
          SendLoadingFinishedCallback(
              &ResponseBodyLoaderClient::DidFailLoadingBody);
          return 0;
      }
    }
  }

  void DrainRemainingDataWithoutStreaming() {
    DCHECK(!IsMainThread());
    if (!finished_) {
      // Keep reading data until we finish (returning 0). It won't be streaming
      // compiled any more, but it will continue being forwarded to the client.
      while (GetMoreData(nullptr) != 0) {
      }
    }
    CHECK(finished_);
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

 private:
  static void NotifyClientDidReceiveData(
      ResponseBodyLoaderClient* response_body_loader_client,
      std::unique_ptr<char[]> data,
      uint32_t data_size) {
    response_body_loader_client->DidReceiveData(
        base::make_span(data.get(), data_size));
  }

  void SendLoadingFinishedCallback(
      void (ResponseBodyLoaderClient::*callback)()) {
    DCHECK(!IsMainThread());
    CHECK(!finished_);
    PostCrossThreadTask(
        *loading_task_runner_, FROM_HERE,
        CrossThreadBindOnce(callback, response_body_loader_client_));
    finished_ = true;
  }

  // TODO(leszeks): Make this a DCHECK-only flag.
  base::AtomicFlag ready_to_run_;
  base::AtomicFlag cancelled_;

  // Only used by background thread
  bool finished_ = false;

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
                      CrossThreadBindOnce(&ScriptStreamer::StreamingComplete,
                                          WrapCrossThreadPersistent(this)));

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

  streamer->StreamingCompleteOnBackgroundThread();

  TRACE_EVENT_END0(
      "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
      "v8.parseOnBackground");

  // TODO(crbug.com/1021571); Remove this once the last event stops being
  // dropped.
  TRACE_EVENT_END0(
      "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
      "v8.parseOnBackground2");
}

}  // namespace

bool ScriptStreamer::HasEnoughDataForStreaming(size_t resource_buffer_size) {
  if (base::FeatureList::IsEnabled(features::kSmallScriptStreaming)) {
    return resource_buffer_size >= kMaximumLengthOfBOM;
  } else {
    // Only stream larger scripts.
    return resource_buffer_size >= small_script_threshold_;
  }
}

// Try to start streaming the script from the given datapipe, taking ownership
// of the datapipe and weak ownership of the client. Returns true if streaming
// succeeded and false otherwise.
//
// Streaming may fail to start because:
//
//   * The encoding is invalid (not UTF-8 or one-byte data)
//   * The script is too small (see HasEnoughDataForStreaming)
//   * There is a code cache for this script already
//   * V8 failed to create a script streamer
//
// If this method returns true, the datapipe handle will be cleared and the
// streamer becomes responsible for draining the datapipe and forwarding data
// to the client. Otherwise, the caller should continue as if this were a no-op.
bool ScriptStreamer::TryStartStreaming(
    mojo::ScopedDataPipeConsumerHandle* data_pipe,
    ResponseBodyLoaderClient* response_body_loader_client) {
  DCHECK(IsMainThread());
  if (streaming_suppressed_)
    return false;
  if (stream_)
    return false;

  DCHECK(!have_enough_data_for_streaming_);

  // Even if the first data chunk is small, the script can still be big
  // enough - wait until the next data chunk comes before deciding whether
  // to start the streaming.
  DCHECK(script_resource_->ResourceBuffer());
  if (!HasEnoughDataForStreaming(script_resource_->ResourceBuffer()->size()))
    return false;
  have_enough_data_for_streaming_ = true;

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
      SuppressStreaming(kEncodingNotSupported);
      return false;
    }
  }

  if (V8CodeCache::HasCodeCache(script_resource_->CacheHandler())) {
    // The resource has a code cache entry, so it's unnecessary to stream
    // and parse the code.
    // TODO(leszeks): Can we even reach this code path with data pipes?
    SuppressStreaming(ScriptStreamer::kHasCodeCache);
    stream_ = nullptr;
    source_.reset();
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
    SuppressStreaming(kV8CannotStream);
    stream_ = nullptr;
    source_.reset();
    return false;
  }

  TRACE_EVENT_WITH_FLOW1(
      TRACE_DISABLED_BY_DEFAULT("v8.compile"), "v8.streamingCompile.start",
      this, TRACE_EVENT_FLAG_FLOW_OUT, "data",
      inspector_parse_script_event::Data(this->ScriptResourceIdentifier(),
                                         this->ScriptURLString()));

  stream_->TakeDataAndPipeOnMainThread(
      script_resource_, this, std::move(*data_pipe),
      response_body_loader_client, loading_task_runner_);

  // Script streaming tasks are high priority, as they can block the parser,
  // and they can (and probably will) block during their own execution as
  // they wait for more input.
  // TODO(leszeks): Decrease the priority of these tasks where possible.
  worker_pool::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      CrossThreadBindOnce(RunScriptStreamingTask,
                          WTF::Passed(std::move(script_streaming_task)),
                          WrapCrossThreadPersistent(this),
                          WTF::CrossThreadUnretained(stream_)));

  return true;
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

  loading_finished_ = true;

  NotifyFinishedToClient();
}

ScriptStreamer::ScriptStreamer(
    ScriptResource* script_resource,
    v8::ScriptCompiler::CompileOptions compile_options,
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner)
    : script_resource_(script_resource),
      detached_(false),
      stream_(nullptr),
      loading_finished_(false),
      parsing_finished_(false),
      have_enough_data_for_streaming_(false),
      streaming_suppressed_(false),
      suppressed_reason_(kInvalid),
      compile_options_(compile_options),
      script_url_string_(script_resource->Url().Copy().GetString()),
      script_resource_identifier_(script_resource->InspectorId()),
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
  visitor->Trace(script_resource_);
}

void ScriptStreamer::StreamingComplete() {
  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("v8.compile"), "v8.streamingCompile.complete",
      this, TRACE_EVENT_FLAG_FLOW_IN, "streaming_suppressed",
      streaming_suppressed_, "data",
      inspector_parse_script_event::Data(this->ScriptResourceIdentifier(),
                                         this->ScriptURLString()));

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

  script_resource_->StreamingFinished();
}

ScriptStreamer* ScriptStreamer::Create(
    ScriptResource* resource,
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
    NotStreamingReason* not_streaming_reason) {
  DCHECK(IsMainThread());
  *not_streaming_reason = kInvalid;
  if (!resource->Url().ProtocolIsInHTTPFamily()) {
    *not_streaming_reason = kNotHTTP;
    return nullptr;
  }
  if (resource->IsLoaded() && !resource->ResourceBuffer()) {
    // This happens for already loaded resources, e.g. if resource
    // validation fails. In that case, the loading subsystem will discard
    // the resource buffer.
    *not_streaming_reason = kNoResourceBuffer;
    return nullptr;
  }
  // We cannot filter out short scripts, even if we wait for the HTTP headers
  // to arrive: the Content-Length HTTP header is not sent for chunked
  // downloads.

  return MakeGarbageCollected<ScriptStreamer>(
      resource, v8::ScriptCompiler::kNoCompileOptions,
      std::move(loading_task_runner));
}

}  // namespace blink
