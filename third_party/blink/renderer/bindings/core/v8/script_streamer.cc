// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"

#include <atomic>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/state_transitions.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/wait.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/v8_compile_hints_histograms.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_common.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_consumer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_for_streaming.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_producer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_local_compile_hints_consumer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"

namespace WTF {

template <>
struct CrossThreadCopier<mojo_base::BigBuffer> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = mojo_base::BigBuffer;
  static Type Copy(Type&& value) { return std::move(value); }
};

}  // namespace WTF

namespace blink {
namespace {

v8::ScriptType ScriptTypeForStreamingTask(ScriptResource* script_resource) {
  switch (script_resource->GetInitialRequestScriptType()) {
    case mojom::blink::ScriptType::kModule:
      return v8::ScriptType::kModule;
    case mojom::blink::ScriptType::kClassic: {
      // <link rel=preload as=script ref=module.mjs> is a common pattern instead
      // of <link rel=modulepreload>. Try streaming parsing as module instead in
      // these cases (https://crbug.com/1178198).
      if (script_resource->IsUnusedPreload()) {
        if (script_resource->Url().GetPath().ToString().EndsWithIgnoringCase(
                ".mjs")) {
          return v8::ScriptType::kModule;
        }
      }
      return v8::ScriptType::kClassic;
    }
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace

// SourceStream implements the streaming interface towards V8. The main
// functionality is preparing the data to give to V8 on main thread, and
// actually giving the data (via GetMoreData which is called on a background
// thread).
class SourceStream : public v8::ScriptCompiler::ExternalSourceStream {
 public:
  SourceStream() = default;

  SourceStream(const SourceStream&) = delete;
  SourceStream& operator=(const SourceStream&) = delete;

  ~SourceStream() override = default;

  // Called by V8 on a background thread. Should block until we can return
  // some data. Ownership of the |src| data buffer is passed to the caller,
  // unless |src| is null.
  size_t GetMoreData(const uint8_t** src) override {
    DCHECK(!IsMainThread());
    CHECK(ready_to_run_.IsSet());

    if (load_state_ != ResourceScriptStreamer::LoadingState::kLoading) {
      return 0;
    }

    if (cancelled_.IsSet()) {
      SetFinished(ResourceScriptStreamer::LoadingState::kCancelled);
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
      base::span<const uint8_t> buffer;
      MojoResult result =
          data_pipe_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);

      switch (result) {
        case MOJO_RESULT_OK: {
          // num_bytes could only be 0 if the handle was being read elsewhere.
          CHECK_GT(buffer.size(), 0u);

          if (src) {
            auto copy_for_script_stream =
                base::HeapArray<uint8_t>::CopiedFrom(buffer);
            *src = std::move(copy_for_script_stream).leak().data();
          }

          // TODO(leszeks): It would be nice to get rid of this second copy, and
          // either share ownership of the chunks, or only give chunks back to
          // the client once the streaming completes.
          Vector<char> copy_for_decoder;
          copy_for_decoder.AppendSpan(base::as_chars(buffer));
          if (absl::holds_alternative<ScriptDecoder*>(script_decoder_)) {
            absl::get<ScriptDecoder*>(script_decoder_)
                ->DidReceiveData(std::move(copy_for_decoder));
          } else {
            CHECK(absl::holds_alternative<ScriptDecoderWithClient*>(
                script_decoder_));
            absl::get<ScriptDecoderWithClient*>(script_decoder_)
                ->DidReceiveData(std::move(copy_for_decoder),
                                 /*send_to_client=*/true);
          }

          result = data_pipe_->EndReadData(buffer.size());
          CHECK_EQ(result, MOJO_RESULT_OK);

          return buffer.size();
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
            SetFinished(ResourceScriptStreamer::LoadingState::kLoaded);
            return 0;
          }

          // We were blocked, so check for cancelation again.
          if (cancelled_.IsSet()) {
            SetFinished(ResourceScriptStreamer::LoadingState::kCancelled);
            return 0;
          }

          // Loop to read the data.
          continue;
        }

        case MOJO_RESULT_FAILED_PRECONDITION:
          // If the producer handle was closed, then treat as EOF.
          SetFinished(ResourceScriptStreamer::LoadingState::kLoaded);
          return 0;

        default:
          // Some other error occurred.
          SetFinished(ResourceScriptStreamer::LoadingState::kFailed);
          return 0;
      }
    }
  }

  void DrainRemainingDataWithoutStreaming() {
    DCHECK(!IsMainThread());
    if (load_state_ == ResourceScriptStreamer::LoadingState::kLoading) {
      // Keep reading data until we finish (returning 0). It won't be streaming
      // compiled any more, but it will continue being forwarded to the client.
      while (GetMoreData(nullptr) != 0) {
      }
    }
    CHECK_NE(load_state_, ResourceScriptStreamer::LoadingState::kLoading);
  }

  void Cancel() {
    // The script is no longer needed by the upper layers. Stop streaming
    // it. The next time GetMoreData is called (or woken up), it will return
    // 0, which will be interpreted as EOS by V8 and the parsing will
    // fail. ResourceScriptStreamer::StreamingComplete will be called, and at
    // that point we will release the references to SourceStream.
    cancelled_.Set();
  }

  void TakeDataAndPipeOnMainThread(ScriptResource* resource,
                                   ResourceScriptStreamer* streamer,
                                   mojo::ScopedDataPipeConsumerHandle data_pipe,
                                   ScriptDecoderWithClient* script_decoder) {
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
    script_decoder_ = script_decoder;

    CHECK(data_pipe_);
    ready_to_run_.Set();
  }

  void TakeDataAndPipeOnBackgroundThread(
      mojo::ScopedDataPipeConsumerHandle data_pipe,
      ScriptDecoder* script_decoder) {
    CHECK(data_pipe);
    CHECK(!ready_to_run_.IsSet());
    CHECK(!cancelled_.IsSet());
    CHECK(!initial_data_);
    CHECK_EQ(initial_data_len_, 0u);
    data_pipe_ = std::move(data_pipe);
    script_decoder_ = script_decoder;
    ready_to_run_.Set();
  }

  mojo::ScopedDataPipeConsumerHandle ReleaseDataPipe() {
    mojo::ScopedDataPipeConsumerHandle body = std::move(data_pipe_);
    data_pipe_.reset();
    return body;
  }

  ResourceScriptStreamer::LoadingState LoadingState() const {
    return load_state_;
  }

 private:
  void SetFinished(ResourceScriptStreamer::LoadingState state) {
    load_state_ = state;
    data_pipe_.reset();
  }

  // TODO(leszeks): Make this a DCHECK-only flag.
  base::AtomicFlag ready_to_run_;
  base::AtomicFlag cancelled_;

  // Only used by background thread
  ResourceScriptStreamer::LoadingState load_state_ =
      ResourceScriptStreamer::LoadingState::kLoading;

  // The initial data that was already on the Resource, rather than being read
  // directly from the data pipe.
  std::unique_ptr<uint8_t[]> initial_data_;
  size_t initial_data_len_ = 0;

  mojo::ScopedDataPipeConsumerHandle data_pipe_;
  absl::variant<ScriptDecoderWithClient*, ScriptDecoder*> script_decoder_;
};

std::tuple<ScriptStreamer*, ScriptStreamer::NotStreamingReason>
ScriptStreamer::TakeFrom(ScriptResource* script_resource,
                         mojom::blink::ScriptType expected_type) {
  const v8::ScriptType expected_script_type =
      expected_type == mojom::blink::ScriptType::kClassic
          ? v8::ScriptType::kClassic
          : v8::ScriptType::kModule;
  NotStreamingReason not_streamed_reason = script_resource->NoStreamerReason();
  ScriptStreamer* streamer = script_resource->TakeStreamer();
  if (streamer) {
    if (streamer->IsStreamingSuppressed()) {
      not_streamed_reason = streamer->StreamingSuppressedReason();
      streamer = nullptr;
    } else {
      // When `streamer` is a BackgroundResourceScriptStreamer,
      // `not_streamed_reason` is kBackgroundResponseProcessorWillBeUsed`.
      CHECK(not_streamed_reason == NotStreamingReason::kInvalid ||
            not_streamed_reason ==
                NotStreamingReason::kBackgroundResponseProcessorWillBeUsed);
      not_streamed_reason = NotStreamingReason::kInvalid;
      if (streamer->GetScriptType() != expected_script_type) {
        streamer = nullptr;
        not_streamed_reason = NotStreamingReason::kErrorScriptTypeMismatch;
      }
    }
    return std::make_tuple(streamer, not_streamed_reason);
  }
  return std::make_tuple(nullptr, not_streamed_reason);
}

namespace {

enum class StreamedBoolean {
  // Must match BooleanStreamed in enums.xml.
  kNotStreamed = 0,
  kStreamed = 1,
  kMaxValue = kStreamed
};

void RecordStartedStreamingHistogram(ScriptSchedulingType type,
                                     bool did_use_streamer) {
  StreamedBoolean streamed = did_use_streamer ? StreamedBoolean::kStreamed
                                              : StreamedBoolean::kNotStreamed;
  switch (type) {
    case ScriptSchedulingType::kParserBlocking: {
      UMA_HISTOGRAM_ENUMERATION(
          "WebCore.Scripts.ParsingBlocking.StartedStreaming", streamed);
      break;
    }
    case ScriptSchedulingType::kDefer: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Deferred.StartedStreaming",
                                streamed);
      break;
    }
    case ScriptSchedulingType::kAsync: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Async.StartedStreaming",
                                streamed);
      break;
    }
    default: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Other.StartedStreaming",
                                streamed);
      break;
    }
  }
}

void RecordNotStreamingReasonHistogram(
    ScriptSchedulingType type,
    ScriptStreamer::NotStreamingReason reason) {
  switch (type) {
    case ScriptSchedulingType::kParserBlocking: {
      UMA_HISTOGRAM_ENUMERATION(
          "WebCore.Scripts.ParsingBlocking.NotStreamingReason", reason);
      break;
    }
    case ScriptSchedulingType::kDefer: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Deferred.NotStreamingReason",
                                reason);
      break;
    }
    case ScriptSchedulingType::kAsync: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Async.NotStreamingReason",
                                reason);
      break;
    }
    default: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Other.NotStreamingReason",
                                reason);
      break;
    }
  }
}

}  // namespace

void ScriptStreamer::RecordStreamingHistogram(
    ScriptSchedulingType type,
    bool can_use_streamer,
    ScriptStreamer::NotStreamingReason reason) {
  RecordStartedStreamingHistogram(type, can_use_streamer);
  if (!can_use_streamer) {
    DCHECK_NE(ScriptStreamer::NotStreamingReason::kInvalid, reason);
    RecordNotStreamingReasonHistogram(type, reason);
  }
}

bool ScriptStreamer::ConvertEncoding(
    const AtomicString& encoding_name,
    v8::ScriptCompiler::StreamedSource::Encoding* encoding) {
  // Here's a list of encodings we can use for streaming. These are
  // the canonical names.
  if (encoding_name == "windows-1252" || encoding_name == "ISO-8859-1" ||
      encoding_name == "US-ASCII") {
    *encoding = v8::ScriptCompiler::StreamedSource::WINDOWS_1252;
    return true;
  }
  if (encoding_name == "UTF-8") {
    *encoding = v8::ScriptCompiler::StreamedSource::UTF8;
    return true;
  }
  // We don't stream other encodings; especially we don't stream two
  // byte scripts to avoid the handling of endianness. Most scripts
  // are Latin1 or UTF-8 anyway, so this should be enough for most
  // real world purposes.
  return false;
}

v8_compile_hints::V8LocalCompileHintsConsumer*
ResourceScriptStreamer::GetV8LocalCompileHintsConsumerForTest() const {
  return compile_hints_->GetV8LocalCompileHintsConsumerForTest();
}

bool ResourceScriptStreamer::IsStreamingStarted() const {
  DCHECK(IsMainThread());
  return !!stream_;
}

bool ResourceScriptStreamer::IsStreamingSuppressed() const {
  DCHECK(IsMainThread());
  return suppressed_reason_ != NotStreamingReason::kInvalid;
}

bool ResourceScriptStreamer::IsLoaded() const {
  DCHECK(IsMainThread());
  return loading_state_ != LoadingState::kLoading;
}

bool ResourceScriptStreamer::CanStartStreaming() const {
  DCHECK(IsMainThread());
  return !IsStreamingStarted() && !IsStreamingSuppressed();
}

bool ResourceScriptStreamer::IsFinished() const {
  DCHECK(IsMainThread());
  // We are finished when we know that we won't start streaming later (either
  // because we are streaming already or streaming was suppressed).
  return IsLoaded() && !CanStartStreaming();
}

bool ResourceScriptStreamer::IsClientDetached() const {
  DCHECK(IsMainThread());
  return !response_body_loader_client_;
}

void ResourceScriptStreamer::StreamingCompleteOnBackgroundThread(
    LoadingState state) {
  DCHECK(!IsMainThread());

  // notifyFinished might already be called, or it might be called in the
  // future (if the parsing finishes earlier because of a parse error).
  script_decoder_->FinishDecode(
      CrossThreadBindOnce(&ResourceScriptStreamer::StreamingComplete,
                          WrapCrossThreadPersistent(this), state));

  // The task might be the only remaining reference to the ScriptStreamer, and
  // there's no way to guarantee that this function has returned before the task
  // is ran, so we should not access the "this" object after posting the task.
}

void ResourceScriptStreamer::Cancel() {
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

void ResourceScriptStreamer::SuppressStreaming(NotStreamingReason reason) {
  DCHECK(IsMainThread());
  CHECK_EQ(suppressed_reason_, NotStreamingReason::kInvalid);
  CHECK_NE(reason, NotStreamingReason::kInvalid);
  suppressed_reason_ = reason;
}

void ResourceScriptStreamer::RunScriptStreamingTask(
    std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask> task,
    ResourceScriptStreamer* streamer,
    SourceStream* stream) {
  // TODO(leszeks): Add flow event data again
  TRACE_EVENT_BEGIN1(
      "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
      "v8.parseOnBackground", "data", [&](perfetto::TracedValue context) {
        inspector_parse_script_event::Data(std::move(context),
                                           streamer->ScriptResourceIdentifier(),
                                           streamer->ScriptURLString());
      });

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

// Try to start a task streaming the script from the datapipe, with the task
// taking ownership of the datapipe and weak ownership of the client. Returns
// true if streaming succeeded and false otherwise.
//
// Streaming may fail to start because:
//
//   * The encoding is invalid (not UTF-8 or one-byte data)
//   * The script is too small to check for a byte-order marker
//   * There is a code cache for this script already
//   * V8 failed to create a script streamer
//
// If this method returns true, the datapipe handle will be cleared and the
// streaming task becomes responsible for draining the datapipe and forwarding
// data to the client. Otherwise, we should continue as if this were a no-op.
bool ResourceScriptStreamer::TryStartStreamingTask() {
  DCHECK(IsMainThread());
  if (!CanStartStreaming())
    return false;

  // Skip non-JS modules based on the mime-type.
  // TODO(crbug/1132413),TODO(crbug/1061857): Disable streaming for non-JS
  // based the specific import statements.
  if (script_type_ == v8::ScriptType::kModule &&
      !MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
          script_resource_->GetResponse().HttpContentType())) {
    SuppressStreaming(NotStreamingReason::kNonJavascriptModule);
    return false;
  }

  // Even if the first data chunk is small, the script can still be big enough -
  // wait until the next data chunk comes before deciding whether to start the
  // streaming.
  if (!script_resource_->ResourceBuffer() ||
      script_resource_->ResourceBuffer()->size() < kMaximumLengthOfBOM) {
    CHECK(!IsLoaded());
    return false;
  }

  {
    // Check for BOM (byte order marks), because that might change our
    // understanding of the data encoding.
    char maybe_bom[kMaximumLengthOfBOM] = {};
    if (!script_resource_->ResourceBuffer()->GetBytes(maybe_bom,
                                                      kMaximumLengthOfBOM)) {
      NOTREACHED_IN_MIGRATION();
      return false;
    }

    std::unique_ptr<TextResourceDecoder> decoder(
        std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
            TextResourceDecoderOptions::kPlainTextContent,
            WTF::TextEncoding(script_resource_->Encoding()))));
    decoder->CheckForBOM(maybe_bom);

    // The encoding may change when we see the BOM. Check for BOM now
    // and update the encoding from the decoder when necessary. Suppress
    // streaming if the encoding is unsupported.
    if (!ConvertEncoding(decoder->Encoding().GetName(), &encoding_)) {
      SuppressStreaming(NotStreamingReason::kEncodingNotSupported);
      return false;
    }
  }

  if (script_resource_->CacheHandler()) {
    // Exclude scripts for which we're not going to generate metadata.
    V8CodeCache::RecordCacheGetStatistics(script_resource_->CacheHandler());
  }

  // Here we can't call Check on the cache handler because it requires the
  // script source, which would require having already loaded the script. It is
  // OK at this point to disable streaming even though we might end up rejecting
  // the cached data later, because we expect that the cached data is usually
  // acceptable. If we detect a content mismatch once the content is loaded,
  // then we reset the code cache entry to just a timestamp, so this condition
  // will allow streaming the next time we load the resource.
  if (V8CodeCache::HasCodeCache(script_resource_->CacheHandler(),
                                CachedMetadataHandler::kAllowUnchecked)) {
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

  const bool has_hot_timestamp =
      V8CodeCache::HasHotTimestamp(script_resource_->CacheHandler());
  compile_hints_ =
      v8_compile_hints::CompileHintsForStreaming::Builder(
          script_resource_->GetV8CrowdsourcedCompileHintsProducer(),
          script_resource_->GetV8CrowdsourcedCompileHintsConsumer(),
          script_resource_->Url(),
          script_resource_
              ->GetV8CompileHintsMagicCommentRuntimeFeatureEnabled())
          .Build((V8CodeCache::HasCompileHints(
                      script_resource_->CacheHandler(),
                      CachedMetadataHandler::kAllowUnchecked) &&
                  has_hot_timestamp)
                     ? V8CodeCache::GetCachedMetadataForCompileHints(
                           script_resource_->CacheHandler(),
                           CachedMetadataHandler::kAllowUnchecked)
                     : nullptr,
                 has_hot_timestamp);
  CHECK(compile_hints_);

  v8::Isolate* isolate = script_resource_->GetIsolateOrNull();
  if (!isolate) {
    stream_ = nullptr;
    source_.reset();
    SuppressStreaming(NotStreamingReason::kContextNotValid);
    return false;
  }

  // Isolate is valid to pass to another thread because it is the main thread
  // isolate that is never destroyed.
  std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask>
      script_streaming_task =
          base::WrapUnique(v8::ScriptCompiler::StartStreaming(
              isolate, source_.get(), script_type_,
              compile_hints_->compile_options(),
              compile_hints_->GetCompileHintCallback(),
              compile_hints_->GetCompileHintCallbackData()));

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
      [&](perfetto::TracedValue context) {
        inspector_parse_script_event::Data(
            std::move(context), ScriptResourceIdentifier(), ScriptURLString());
      });

  stream_->TakeDataAndPipeOnMainThread(
      script_resource_, this, std::move(data_pipe_), script_decoder_.get());

  // This reset will also cancel the watcher.
  watcher_.reset();

  // Script streaming tasks are high priority, as they can block the parser,
  // and they can (and probably will) block during their own execution as
  // they wait for more input.
  // TODO(leszeks): Decrease the priority of these tasks where possible.
  worker_pool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      CrossThreadBindOnce(RunScriptStreamingTask,
                          std::move(script_streaming_task),
                          WrapCrossThreadPersistent(this),
                          WTF::CrossThreadUnretained(stream_)));

  return true;
}

v8::ScriptType ResourceScriptStreamer::GetScriptType() const {
  return script_type_;
}

ResourceScriptStreamer::ResourceScriptStreamer(
    ScriptResource* script_resource,
    mojo::ScopedDataPipeConsumerHandle data_pipe,
    ResponseBodyLoaderClient* response_body_loader_client,
    std::unique_ptr<TextResourceDecoder> decoder,
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner)
    : script_resource_(script_resource),
      response_body_loader_client_(response_body_loader_client),
      script_decoder_(
          ScriptDecoderWithClient::Create(response_body_loader_client,
                                          std::move(decoder),
                                          loading_task_runner)),
      data_pipe_(std::move(data_pipe)),
      script_url_string_(script_resource->Url().GetString()),
      script_resource_identifier_(script_resource->InspectorId()),
      // Unfortunately there's no dummy encoding value in the enum; let's use
      // one we don't stream.
      encoding_(v8::ScriptCompiler::StreamedSource::TWO_BYTE),
      script_type_(ScriptTypeForStreamingTask(script_resource)) {
  watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      loading_task_runner);

  watcher_->Watch(
      data_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      WTF::BindRepeating(&ResourceScriptStreamer::OnDataPipeReadable,
                         WrapWeakPersistent(this)));

  MojoResult ready_result;
  mojo::HandleSignalsState ready_state;
  MojoResult rv = watcher_->Arm(&ready_result, &ready_state);
  if (rv == MOJO_RESULT_OK)
    return;

  DCHECK_EQ(MOJO_RESULT_FAILED_PRECONDITION, rv);
  OnDataPipeReadable(ready_result, ready_state);
}

void ResourceScriptStreamer::OnDataPipeReadable(
    MojoResult result,
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
            script_resource_->ResourceBuffer()->size() < kMaximumLengthOfBOM);
      watcher_.reset();
      // Pass kScriptTooSmall for the !IsStreamingSuppressed() case, it won't
      // override an existing streaming reason.
      LoadCompleteWithoutStreaming(LoadingState::kLoaded,
                                   NotStreamingReason::kScriptTooSmall);
      return;

    case MOJO_RESULT_SHOULD_WAIT:
      NOTREACHED_IN_MIGRATION();
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

  base::span<const uint8_t> data;
  MojoReadDataFlags flags_to_pass = MOJO_READ_DATA_FLAG_NONE;
  MojoResult begin_read_result = data_pipe_->BeginReadData(flags_to_pass, data);
  // There should be data, so this read should succeed.
  CHECK_EQ(begin_read_result, MOJO_RESULT_OK);

  std::string_view chars = base::as_string_view(data);
  response_body_loader_client_->DidReceiveData(chars);
  script_decoder_->DidReceiveData(Vector<char>(chars),
                                  /*send_to_client=*/false);

  MojoResult end_read_result = data_pipe_->EndReadData(data.size());

  CHECK_EQ(end_read_result, MOJO_RESULT_OK);

  if (TryStartStreamingTask()) {
    return;
  }

  // TODO(leszeks): Depending on how small the chunks are, we may want to
  // loop until a certain number of bytes are synchronously read rather than
  // going back to the scheduler.
  watcher_->ArmOrNotify();
}

ResourceScriptStreamer::~ResourceScriptStreamer() = default;

void ResourceScriptStreamer::Prefinalize() {
  // Reset and cancel the watcher. This has to be called in the prefinalizer,
  // rather than relying on the destructor, as accesses by the watcher of the
  // script resource between prefinalization and destruction are invalid. See
  // https://crbug.com/905975#c34 for more details.
  watcher_.reset();

  // Cancel any on-going streaming.
  Cancel();
}

void ResourceScriptStreamer::Trace(Visitor* visitor) const {
  visitor->Trace(script_resource_);
  visitor->Trace(response_body_loader_client_);
  ScriptStreamer::Trace(visitor);
}

void ResourceScriptStreamer::StreamingComplete(LoadingState loading_state) {
  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("v8.compile"), "v8.streamingCompile.complete",
      this, TRACE_EVENT_FLAG_FLOW_IN, "streaming_suppressed",
      IsStreamingSuppressed(), "data", [&](perfetto::TracedValue context) {
        inspector_parse_script_event::Data(
            std::move(context), ScriptResourceIdentifier(), ScriptURLString());
      });

  // The background task is completed; do the necessary ramp-down in the main
  // thread.
  DCHECK(IsMainThread());

  AdvanceLoadingState(loading_state);

  // Sending a finished notification to the client also indicates that streaming
  // completed.
  SendClientLoadFinishedCallback();
}

void ResourceScriptStreamer::LoadCompleteWithoutStreaming(
    LoadingState state,
    NotStreamingReason no_streaming_reason) {
  // We might have previously suppressed streaming, in which case we want to
  // keep the previous reason and not re-suppress.
  if (!IsStreamingSuppressed()) {
    SuppressStreaming(no_streaming_reason);
  }
  AdvanceLoadingState(state);

  // Make sure decoding is finished before finishing the load.
  script_decoder_->FinishDecode(CrossThreadBindOnce(
      &ResourceScriptStreamer::SendClientLoadFinishedCallback,
      WrapCrossThreadPersistent(this)));
}

void ResourceScriptStreamer::SendClientLoadFinishedCallback() {
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

void ResourceScriptStreamer::AdvanceLoadingState(LoadingState new_state) {
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

void ResourceScriptStreamer::CheckState() const {
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

class InlineSourceStream final
    : public v8::ScriptCompiler::ExternalSourceStream {
 public:
  explicit InlineSourceStream(const String& text) : text_(text) {}
  ~InlineSourceStream() override = default;

  size_t GetMoreData(const uint8_t** src) override {
    if (!text_) {
      // The V8 scanner requires a valid pointer when using TWO_BYTE sources,
      // even if the length is 0.
      *src = new uint8_t[0];
      return 0;
    }

    auto text_bytes = text_.RawByteSpan();
    size_t size = text_bytes.size();
    auto data_copy = base::HeapArray<uint8_t>::CopiedFrom(text_bytes);
    text_ = String();

    *src = std::move(data_copy).leak().data();
    return size;
  }

 private:
  String text_;
};

BackgroundInlineScriptStreamer::BackgroundInlineScriptStreamer(
    v8::Isolate* isolate,
    const String& text,
    v8::ScriptCompiler::CompileOptions compile_options) {
  auto stream = std::make_unique<InlineSourceStream>(text);
  source_ = std::make_unique<v8::ScriptCompiler::StreamedSource>(
      std::move(stream), text.Is8Bit()
                             ? v8::ScriptCompiler::StreamedSource::ONE_BYTE
                             : v8::ScriptCompiler::StreamedSource::TWO_BYTE);

  // We don't generate code caches for inline scripts, so we never pass the
  // kFollowCompileHintsMagicComment compile option.
  CHECK((compile_options &
         v8::ScriptCompiler::kFollowCompileHintsMagicComment) == 0);
  task_ = base::WrapUnique(v8::ScriptCompiler::StartStreaming(
      isolate, source_.get(), v8::ScriptType::kClassic, compile_options));
}

void BackgroundInlineScriptStreamer::Run() {
  TRACE_EVENT0("blink", "BackgroundInlineScriptStreamer::Run");
  if (cancelled_.IsSet())
    return;

  started_.Set();
  task_->Run();
  task_.reset();

  // We signal an event here instead of posting a task to the main thread
  // because it's possible the task wouldn't be run by the time the script
  // streamer is needed. This allows us to compile the inline script right up to
  // when it is needed. If the script hasn't finished compiling, the main thread
  // will block while it finishes on the worker thread. The worker thread should
  // have already gotten a head start, so this should block the main thread for
  // less time than the compile would have taken.
  event_.Signal();
}

v8::ScriptCompiler::StreamedSource* BackgroundInlineScriptStreamer::Source(
    v8::ScriptType expected_type) {
  TRACE_EVENT0("blink", "BackgroundInlineScriptStreamer::Source");
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("WebCore.Scripts.InlineStreamerWaitTime");
  DCHECK(IsMainThread());
  DCHECK_EQ(expected_type, v8::ScriptType::kClassic);
  static const base::FeatureParam<base::TimeDelta> kWaitTimeoutParam{
      &features::kPrecompileInlineScripts, "inline-script-timeout",
      base::Milliseconds(0)};
  // Make sure the script has finished compiling in the background. See comment
  // above in Run().
  bool signaled = event_.TimedWait(kWaitTimeoutParam.Get());
  base::UmaHistogramBoolean("WebCore.Scripts.InlineStreamerTimedOut",
                            !signaled);
  if (!signaled)
    return nullptr;
  return source_.get();
}

// static
InlineScriptStreamer* InlineScriptStreamer::From(
    scoped_refptr<BackgroundInlineScriptStreamer> streamer) {
  return MakeGarbageCollected<InlineScriptStreamer>(std::move(streamer));
}

namespace {

enum class BackgroundProcessorState {
  kWaitingForResponse = 0,
  kResponseReceived,
  kCheckingEncoding,
  kWaitingForDataPipeReadable,
  kWaitingForParseResult,
  kWaitingForConsumeCodeCacheResultAndDecodedScript,
  kWaitingForConsumeCodeCacheResult,
  kWaitingForDecodedScript,
  kStreamingSupressed,
  kFinished,
};

#if DCHECK_IS_ON()
std::ostream& operator<<(std::ostream& o, const BackgroundProcessorState& s) {
  return o << static_cast<unsigned>(s);
}
#endif  // DCHECK_IS_ON()

std::unique_ptr<v8::ScriptCompiler::ConsumeCodeCacheTask>
MaybeCreateConsumeCodeCacheTask(std::optional<mojo_base::BigBuffer>& big_buffer,
                                const String& encoding,
                                v8::Isolate* isolate,
                                bool& has_code_cache,
                                v8::ScriptType script_type) {
  CHECK(!has_code_cache);
  if (script_type == v8::ScriptType::kModule) {
    // Currently ModuleScript doesn't support off-thread cache consumption.
    return nullptr;
  }
  if (!big_buffer) {
    return nullptr;
  }
  scoped_refptr<CachedMetadata> metadata =
      CachedMetadata::CreateFromSerializedData(*big_buffer);
  if (!metadata) {
    return nullptr;
  }
  std::unique_ptr<v8::ScriptCompiler::ConsumeCodeCacheTask> task;
  if (V8CodeCache::HasCodeCache(*metadata, encoding)) {
    has_code_cache = true;
    if (features::kBackgroundCodeCacheDecoderStart.Get()) {
      task.reset(v8::ScriptCompiler::StartConsumingCodeCacheOnBackground(
          isolate, V8CodeCache::CreateCachedData(metadata)));
    }
  }
  absl::variant<Vector<uint8_t>, mojo_base::BigBuffer> drained_data =
      std::move(*metadata).DrainSerializedData();
  CHECK(absl::holds_alternative<mojo_base::BigBuffer>(drained_data));
  big_buffer = std::move(absl::get<mojo_base::BigBuffer>(drained_data));
  return task;
}

std::unique_ptr<v8_compile_hints::CompileHintsForStreaming>
BuildCompileHintsForStreaming(
    v8_compile_hints::CompileHintsForStreaming::Builder& builder,
    std::optional<mojo_base::BigBuffer>& big_buffer,
    const String& encoding) {
  // Same as the HasCodeCache() method above, this method creates a
  // CachedMetadata from the the passed BigBuffer and passes it to
  // V8CodeCache::HasHotCompileHints(). And then takes the BigBuffer from the
  // CachedMetadata and set it back to the input argument `big_buffer`.
  scoped_refptr<CachedMetadata> metadata =
      big_buffer ? CachedMetadata::CreateFromSerializedData(*big_buffer)
                 : nullptr;

  V8CodeCache::RecordCacheGetStatistics(metadata.get(), encoding);
  std::unique_ptr<v8_compile_hints::CompileHintsForStreaming> result =
      std::move(builder).Build(
          (metadata && V8CodeCache::HasHotCompileHints(*metadata, encoding))
              ? metadata
              : nullptr,
          metadata && V8CodeCache::HasHotTimestamp(*metadata, encoding));
  if (metadata) {
    absl::variant<Vector<uint8_t>, mojo_base::BigBuffer> drained_data =
        std::move(*metadata).DrainSerializedData();
    CHECK(absl::holds_alternative<mojo_base::BigBuffer>(drained_data));
    big_buffer = std::move(absl::get<mojo_base::BigBuffer>(drained_data));
  }
  return result;
}

}  // namespace

BackgroundResourceScriptStreamer::Result::Result(
    String decoded_data,
    std::unique_ptr<ParkableStringImpl::SecureDigest> digest,
    std::unique_ptr<v8::ScriptCompiler::StreamedSource> streamed_source)
    : decoded_data(std::move(decoded_data)),
      digest(std::move(digest)),
      streamed_source(std::move(streamed_source)) {}

BackgroundResourceScriptStreamer::Result::Result(
    String decoded_data,
    std::unique_ptr<ParkableStringImpl::SecureDigest> digest,
    std::unique_ptr<v8::ScriptCompiler::ConsumeCodeCacheTask>
        consume_code_cache_task)
    : decoded_data(std::move(decoded_data)),
      digest(std::move(digest)),
      consume_code_cache_task(std::move(consume_code_cache_task)) {}

class BackgroundResourceScriptStreamer::BackgroundProcessor final
    : public BackgroundResponseProcessor {
 public:
  BackgroundProcessor(
      v8::ScriptType script_type,
      const String script_url_string,
      uint64_t script_resource_identifier,
      v8::Isolate* isolate,
      WTF::TextEncoding encoding,
      std::unique_ptr<v8_compile_hints::CompileHintsForStreaming::Builder>
          compile_hints_builder,
      CrossThreadWeakHandle<BackgroundResourceScriptStreamer> streamer_handle);
  BackgroundProcessor(const BackgroundProcessor&) = delete;
  BackgroundProcessor& operator=(const BackgroundProcessor&) = delete;
  ~BackgroundProcessor() override;

  bool MaybeStartProcessingResponse(
      network::mojom::URLResponseHeadPtr& head,
      mojo::ScopedDataPipeConsumerHandle& body,
      std::optional<mojo_base::BigBuffer>& cached_metadata_buffer,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      Client* client) override;

  v8::ScriptType script_type() const { return script_type_; }
  bool IsStreamingSuppressed();

 private:
  static void RunScriptStreamingTask(
      const String script_url_string,
      uint64_t script_resource_identifier,
      std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask>
          script_streaming_task,
      std::unique_ptr<v8::ScriptCompiler::StreamedSource> streamed_source,
      SourceStream* source_stream_ptr,
      ScriptDecoderPtr script_decoder,
      std::unique_ptr<v8_compile_hints::CompileHintsForStreaming> compile_hints,
      base::WeakPtr<BackgroundProcessor> background_processor_weak_ptr);
  static void RunConsumingCodeCacheTask(
      const String script_url_string,
      uint64_t script_resource_identifier,
      std::unique_ptr<v8::ScriptCompiler::ConsumeCodeCacheTask>
          consume_code_cache_task,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      mojo_base::BigBuffer cached_metadata,
      base::WeakPtr<BackgroundProcessor> background_processor_weak_ptr,
      const uint64_t trace_id);

  void SetState(BackgroundProcessorState state);

  void OnDataPipeReadable(MojoResult ready_result,
                          const mojo::HandleSignalsState& ready_state);
  bool TryStartStreamingTask(MojoResult result,
                             const mojo::HandleSignalsState& state);

  void OnFinishStreaming(
      std::unique_ptr<v8::ScriptCompiler::StreamedSource> streamed_source,
      ScriptDecoderPtr script_decoder,
      ScriptDecoder::Result result);

  void OnFinishCodeCacheConsumer(
      std::unique_ptr<v8::ScriptCompiler::ConsumeCodeCacheTask>
          consume_code_cache_task,
      mojo_base::BigBuffer cached_metadata);
  void OnFinishScriptDecode(ScriptDecoder::Result result);
  void OnFinishCodeCacheConsumerScriptDecode();

  void SuppressStreaming(NotStreamingReason reason);

  const v8::ScriptType script_type_;
  // Keep the script URL string for event tracing.
  const String script_url_string_;
  // Keep the script resource identifier for event tracing.
  const uint64_t script_resource_identifier_;

  v8::Isolate* isolate_;
  WTF::TextEncoding encoding_;

  SourceStream* source_stream_ptr_ = nullptr;

  // For CompileHints
  std::unique_ptr<v8_compile_hints::CompileHintsForStreaming::Builder>
      compile_hints_builder_;
  std::unique_ptr<v8_compile_hints::CompileHintsForStreaming> compile_hints_;

  CrossThreadWeakHandle<BackgroundResourceScriptStreamer> streamer_handle_;

  // Used for reading first few bytes of the body to detecting BOM.
  std::unique_ptr<mojo::SimpleWatcher> watcher_;

  network::mojom::URLResponseHeadPtr head_;
  mojo::ScopedDataPipeConsumerHandle body_;
  std::optional<mojo_base::BigBuffer> cached_metadata_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  DataPipeScriptDecoderPtr data_pipe_script_decoder_;

  std::unique_ptr<v8::ScriptCompiler::ConsumeCodeCacheTask>
      consume_code_cache_task_;
  std::optional<ScriptDecoder::Result> decoder_result_;

  Client* client_;

  NotStreamingReason suppressed_reason_ = NotStreamingReason::kInvalid;

  BackgroundProcessorState state_ =
      BackgroundProcessorState::kWaitingForResponse;

  SEQUENCE_CHECKER(background_sequence_checker_);
  base::WeakPtrFactory<BackgroundProcessor> weak_factory_{this};
};

class BackgroundResourceScriptStreamer::BackgroundProcessorFactory final
    : public BackgroundResponseProcessorFactory {
 public:
  BackgroundProcessorFactory(
      ScriptResource* script_resource,
      CrossThreadWeakHandle<BackgroundResourceScriptStreamer> streamer_handle)
      : script_type_(ScriptTypeForStreamingTask(script_resource)),
        script_url_string_(script_resource->Url().GetString()),
        script_resource_identifier_(script_resource->InspectorId()),
        isolate_(script_resource->GetIsolateOrNull()),
        encoding_(script_resource->Encoding()),
        compile_hints_builder_(
            std::make_unique<
                v8_compile_hints::CompileHintsForStreaming::Builder>(
                script_resource->GetV8CrowdsourcedCompileHintsProducer(),
                script_resource->GetV8CrowdsourcedCompileHintsConsumer(),
                script_resource->Url(),
                script_resource
                    ->GetV8CompileHintsMagicCommentRuntimeFeatureEnabled())),
        streamer_handle_(std::move(streamer_handle)) {}
  BackgroundProcessorFactory(const BackgroundProcessorFactory&) = delete;
  BackgroundProcessorFactory& operator=(const BackgroundProcessorFactory&) =
      delete;
  ~BackgroundProcessorFactory() override = default;

  std::unique_ptr<BackgroundResponseProcessor> Create() && override {
    return std::make_unique<BackgroundProcessor>(
        script_type_, script_url_string_, script_resource_identifier_, isolate_,
        encoding_, std::move(compile_hints_builder_),
        std::move(streamer_handle_));
  }

  const v8::ScriptType script_type_;
  const String script_url_string_;
  const uint64_t script_resource_identifier_;
  v8::Isolate* isolate_;
  const WTF::TextEncoding encoding_;
  std::unique_ptr<v8_compile_hints::CompileHintsForStreaming::Builder>
      compile_hints_builder_;
  CrossThreadWeakHandle<BackgroundResourceScriptStreamer> streamer_handle_;
};

BackgroundResourceScriptStreamer::BackgroundProcessor::BackgroundProcessor(
    v8::ScriptType script_type,
    const String script_url_string,
    uint64_t script_resource_identifier,
    v8::Isolate* isolate,
    WTF::TextEncoding encoding,
    std::unique_ptr<v8_compile_hints::CompileHintsForStreaming::Builder>
        compile_hints_builder,
    CrossThreadWeakHandle<BackgroundResourceScriptStreamer> streamer_handle)
    : script_type_(script_type),
      script_url_string_(script_url_string),
      script_resource_identifier_(script_resource_identifier),
      isolate_(isolate),
      encoding_(encoding),
      compile_hints_builder_(std::move(compile_hints_builder)),
      streamer_handle_(std::move(streamer_handle)) {}

BackgroundResourceScriptStreamer::BackgroundProcessor::~BackgroundProcessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
  watcher_.reset();
  if (source_stream_ptr_) {
    source_stream_ptr_->Cancel();
  }
}

void BackgroundResourceScriptStreamer::BackgroundProcessor::SetState(
    BackgroundProcessorState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
#if DCHECK_IS_ON()
  using S = BackgroundProcessorState;
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      base::StateTransitions<S>, transitions,
      ({
          {S::kWaitingForResponse,
           {// The BackgroundProcessor receives the response.
            S::kResponseReceived}},
          {S::kResponseReceived,
           {// The mime type of the response is not supported, or the received
            // metadata contains code cache.
            S::kStreamingSupressed,
            // There is no data in the data pipe that can be read synchronously.
            S::kWaitingForDataPipeReadable,
            // There is some data in the data pipe, so let's try to check the
            // encoding.
            S::kCheckingEncoding,
            // There is a code cache metadata, so start to consume the
            // code cache. This state is used only when
            // BackgroundCodeCacheDecoderStart is enabled.
            S::kWaitingForConsumeCodeCacheResultAndDecodedScript}},
          {S::kCheckingEncoding,
           {// Finished loading all body data which is smaller than
            // kMaximumLengthOfBOM, or error occurred while reading the data
            // pipe, or the detected encoding is not supported.
            S::kStreamingSupressed,
            // The data in the passed data pipe is too small to detect the
            // encoding.
            S::kWaitingForDataPipeReadable,
            // Started the parser on another thread.
            S::kWaitingForParseResult}},
          {S::kWaitingForDataPipeReadable,
           {// There is some data in the data pipe, so let's try to check the
            // encoding.
            S::kCheckingEncoding}},
          {S::kWaitingForParseResult,
           {// The background parser finished.
            S::kFinished}},
          {S::kWaitingForConsumeCodeCacheResultAndDecodedScript,
           {// Received the result from the script decoder.
            S::kWaitingForConsumeCodeCacheResult,
            // Received the result from the code cache consumer.
            S::kWaitingForDecodedScript}},
          {S::kWaitingForConsumeCodeCacheResult,
           {// Received the result from the code cache consumer.
            S::kFinished}},
          {S::kWaitingForDecodedScript,
           {// Received the result from the script decoder.
            S::kFinished}},
      }));
  DCHECK_STATE_TRANSITION(&transitions, state_, state);
#endif  // DCHECK_IS_ON()
  state_ = state;
}

bool BackgroundResourceScriptStreamer::BackgroundProcessor::
    MaybeStartProcessingResponse(
        network::mojom::URLResponseHeadPtr& head,
        mojo::ScopedDataPipeConsumerHandle& body,
        std::optional<mojo_base::BigBuffer>& cached_metadata,
        scoped_refptr<base::SequencedTaskRunner> background_task_runner,
        Client* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
  CHECK(background_task_runner->RunsTasksInCurrentSequence());
  TRACE_EVENT1("v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "BackgroundProcessor::MaybeStartProcessingResponse", "url",
               script_url_string_.Utf8());
  CHECK(body);
  CHECK(head);
  SetState(BackgroundProcessorState::kResponseReceived);

  client_ = client;

  if (script_type_ == v8::ScriptType::kModule) {
    std::string mime_type;
    if (!head->headers->GetMimeType(&mime_type) ||
        !MIMETypeRegistry::IsSupportedJavaScriptMIMEType(String(mime_type))) {
      SuppressStreaming(NotStreamingReason::kNonJavascriptModuleBackground);
      return false;
    }
  }
  if (!head->charset.empty()) {
    WTF::TextEncoding new_encoding = WTF::TextEncoding(String(head->charset));
    if (new_encoding.IsValid()) {
      encoding_ = new_encoding;
    }
  }

  head_ = std::move(head);
  body_ = std::move(body);
  cached_metadata_ = std::move(cached_metadata);
  background_task_runner_ = background_task_runner;

  bool has_code_cache = false;
  if (auto consume_code_cache_task = MaybeCreateConsumeCodeCacheTask(
          cached_metadata_, encoding_.GetName(), isolate_, has_code_cache,
          script_type())) {
    const uint64_t trace_id =
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this));
    TRACE_EVENT_WITH_FLOW1(
        "v8," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
        "v8.deserializeOnBackground.start", TRACE_ID_LOCAL(trace_id),
        TRACE_EVENT_FLAG_FLOW_OUT, "data", [&](perfetto::TracedValue context) {
          inspector_deserialize_script_event::Data(std::move(context),
                                                   script_resource_identifier_,
                                                   script_url_string_);
        });
    CHECK(features::kBackgroundCodeCacheDecoderStart.Get());
    V8CodeCache::RecordCacheGetStatistics(
        V8CodeCache::GetMetadataType::kCodeCache);
    SetState(BackgroundProcessorState::
                 kWaitingForConsumeCodeCacheResultAndDecodedScript);
    data_pipe_script_decoder_ = DataPipeScriptDecoder::Create(
        std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
            TextResourceDecoderOptions::kPlainTextContent, encoding_)),
        background_task_runner_,
        CrossThreadBindOnce(&BackgroundProcessor::OnFinishScriptDecode,
                            weak_factory_.GetWeakPtr()));
    data_pipe_script_decoder_->Start(std::move(body_));
    // The cached metadata must be passed to the worker thread to avoid UAF,
    // because `this` is deleted when the request is canceled.
    worker_pool::PostTask(
        FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
        CrossThreadBindOnce(
            &BackgroundProcessor::RunConsumingCodeCacheTask, script_url_string_,
            script_resource_identifier_, std::move(consume_code_cache_task),
            background_task_runner_, std::move(*cached_metadata_),
            weak_factory_.GetWeakPtr(), trace_id));
    return true;
  }

  // TODO(40244488): Remove this when BackgroundCodeCacheDecoderStart feature
  // is removed.
  if (has_code_cache) {
    // There is a code cache, but the BackgroundCodeCacheDecoderStart feature is
    // disabled.
    CHECK(!features::kBackgroundCodeCacheDecoderStart.Get());
    head = std::move(head_);
    body = std::move(body_);
    cached_metadata = std::move(cached_metadata_);
    SuppressStreaming(NotStreamingReason::kHasCodeCacheBackground);
    V8CodeCache::RecordCacheGetStatistics(
        V8CodeCache::GetMetadataType::kCodeCache);
    return false;
  }

  compile_hints_ = BuildCompileHintsForStreaming(
      *compile_hints_builder_, cached_metadata_, encoding_.GetName());
  CHECK(compile_hints_);

  watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
  watcher_->Watch(body_.get(), MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
                  MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                  WTF::BindRepeating(&BackgroundProcessor::OnDataPipeReadable,
                                     weak_factory_.GetWeakPtr()));
  MojoResult ready_result;
  mojo::HandleSignalsState ready_state;
  MojoResult rv = watcher_->Arm(&ready_result, &ready_state);
  if (rv == MOJO_RESULT_OK) {
    // There is no data in the data pipe, so let's wait until new data is
    // available to read. BackgroundProcessor::OnDataPipeReadable() will be
    // called when new data is available or the data pipe is closed.
    SetState(BackgroundProcessorState::kWaitingForDataPipeReadable);
    return true;
  }
  // The following code is executed when there is some data in the data pipe or
  // the data pipe is closed. To reduce the cost of PostTask, we check the data
  // pipe synchronously here.
  DCHECK_EQ(MOJO_RESULT_FAILED_PRECONDITION, rv);
  if (TryStartStreamingTask(ready_result, ready_state)) {
    CHECK_EQ(state_, BackgroundProcessorState::kWaitingForParseResult);
    // Trere is enough data in the data pipe to detect the encoding, and
    // ScriptStreamingTask has been started on the background thread.
    return true;
  }
  if (watcher_) {
    CHECK(!IsStreamingSuppressed());
    CHECK_EQ(state_, BackgroundProcessorState::kWaitingForDataPipeReadable);
    // The data in the data pipe is too small to detect the encoding. So call
    // ArmOrNotify() and let's wait until new data is available to read.
    watcher_->ArmOrNotify();
    return true;
  }
  CHECK(IsStreamingSuppressed());
  CHECK_EQ(state_, BackgroundProcessorState::kStreamingSupressed);
  // We checked the data in the data pipe synchronously, and the detected
  // encoding is not supported. So reset `head`, `body`, `cached_metadata` and
  // return false here.
  head = std::move(head_);
  body = std::move(body_);
  cached_metadata = std::move(cached_metadata_);
  return false;
}

void BackgroundResourceScriptStreamer::BackgroundProcessor::OnDataPipeReadable(
    MojoResult ready_result,
    const mojo::HandleSignalsState& ready_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
  TRACE_EVENT0("v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "BackgroundProcessor::OnDataPipeReadable");
  CHECK_EQ(state_, BackgroundProcessorState::kWaitingForDataPipeReadable);
  if (TryStartStreamingTask(ready_result, ready_state)) {
    CHECK_EQ(state_, BackgroundProcessorState::kWaitingForParseResult);
    return;
  }
  if (watcher_) {
    CHECK(!IsStreamingSuppressed());
    CHECK_EQ(state_, BackgroundProcessorState::kWaitingForDataPipeReadable);
    // The data in the data pipe is  too small to detect the encoding. So call
    // ArmOrNotify().
    watcher_->ArmOrNotify();
    return;
  }
  CHECK(IsStreamingSuppressed());
  CHECK_EQ(state_, BackgroundProcessorState::kStreamingSupressed);
  // We checked the data in the data pipe asynchronously, and the detected
  // encoding is not supported or some error occurred while reading the data
  // pipe or the data was too small. So call
  // DidFinishBackgroundResponseProcessor() with `head_`, `body_`,
  // `cached_metadata_` which were passed at MaybeStartProcessingResponse().
  client_->DidFinishBackgroundResponseProcessor(
      std::move(head_), std::move(body_), std::move(cached_metadata_));
}

bool BackgroundResourceScriptStreamer::BackgroundProcessor::
    TryStartStreamingTask(MojoResult result,
                          const mojo::HandleSignalsState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
  TRACE_EVENT0("v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "BackgroundProcessor::TryStartStreamingTask");
  SetState(BackgroundProcessorState::kCheckingEncoding);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // The data is smaller than kMaximumLengthOfBOM.
      watcher_.reset();
      SuppressStreaming(NotStreamingReason::kScriptTooSmallBackground);
      return false;
    case MOJO_RESULT_SHOULD_WAIT:
      NOTREACHED();
    default:
      // Some other error occurred.
      watcher_.reset();
      SuppressStreaming(NotStreamingReason::kErrorOccurredBackground);
      return false;
  }
  CHECK(state.readable());
  base::span<const uint8_t> data;
  constexpr uint32_t kMaximumLengthOfBOM = 4;
  MojoResult begin_read_result =
      body_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, data);
  CHECK_EQ(begin_read_result, MOJO_RESULT_OK);
  CHECK_GT(data.size(), 0u);
  if (data.size() < kMaximumLengthOfBOM) {
    MojoResult end_read_result = body_->EndReadData(0);
    CHECK_EQ(end_read_result, MOJO_RESULT_OK);
    // We keep `watcher_` to read more data.
    CHECK(watcher_);
    SetState(BackgroundProcessorState::kWaitingForDataPipeReadable);
    return false;
  }
  watcher_.reset();

  std::unique_ptr<TextResourceDecoder> decoder(
      std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
          TextResourceDecoderOptions::kPlainTextContent, encoding_)));
  decoder->CheckForBOM(base::as_chars(data.first(kMaximumLengthOfBOM)));
  MojoResult end_read_result = body_->EndReadData(0);
  CHECK_EQ(end_read_result, MOJO_RESULT_OK);
  v8::ScriptCompiler::StreamedSource::Encoding script_source_encoding =
      v8::ScriptCompiler::StreamedSource::UTF8;
  if (!ScriptStreamer::ConvertEncoding(decoder->Encoding().GetName(),
                                       &script_source_encoding)) {
    SuppressStreaming(NotStreamingReason::kEncodingNotSupportedBackground);
    return false;
  }

  ScriptDecoderPtr script_decoder = ScriptDecoder::Create(
      std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
          TextResourceDecoderOptions::kPlainTextContent, encoding_)),
      background_task_runner_);
  auto source_stream = std::make_unique<SourceStream>();
  source_stream_ptr_ = source_stream.get();
  source_stream->TakeDataAndPipeOnBackgroundThread(std::move(body_),
                                                   script_decoder.get());
  std::unique_ptr<v8::ScriptCompiler::StreamedSource> streamed_source =
      std::make_unique<v8::ScriptCompiler::StreamedSource>(
          std::move(source_stream), script_source_encoding);

  CHECK(compile_hints_);
  std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask>
      script_streaming_task =
          base::WrapUnique(v8::ScriptCompiler::StartStreaming(
              isolate_, streamed_source.get(), script_type_,
              compile_hints_->compile_options(),
              compile_hints_->GetCompileHintCallback(),
              compile_hints_->GetCompileHintCallbackData()));
  if (!script_streaming_task) {
    // V8 can't stream the script.
    body_ = source_stream_ptr_->ReleaseDataPipe();
    source_stream_ptr_ = nullptr;
    SuppressStreaming(NotStreamingReason::kV8CannotStream);
    return false;
  }
  SetState(BackgroundProcessorState::kWaitingForParseResult);
  worker_pool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      CrossThreadBindOnce(
          &BackgroundProcessor::RunScriptStreamingTask, script_url_string_,
          script_resource_identifier_, std::move(script_streaming_task),
          std::move(streamed_source), CrossThreadUnretained(source_stream_ptr_),
          std::move(script_decoder), std::move(compile_hints_),
          weak_factory_.GetWeakPtr()));
  return true;
}

// static
void BackgroundResourceScriptStreamer::BackgroundProcessor::
    RunScriptStreamingTask(
        const String script_url_string,
        uint64_t script_resource_identifier,
        std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask>
            script_streaming_task,
        std::unique_ptr<v8::ScriptCompiler::StreamedSource> streamed_source,
        SourceStream* source_stream_ptr,
        ScriptDecoderPtr script_decoder,
        std::unique_ptr<v8_compile_hints::CompileHintsForStreaming>
            compile_hints,
        base::WeakPtr<BackgroundProcessor> background_processor_weak_ptr) {
  TRACE_EVENT1("v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "BackgroundProcessor::RunScriptStreamingTask", "url",
               script_url_string.Utf8());
  TRACE_EVENT_BEGIN1(
      "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
      "v8.parseOnBackground", "data", [&](perfetto::TracedValue context) {
        inspector_parse_script_event::Data(
            std::move(context), script_resource_identifier, script_url_string);
      });
  TRACE_EVENT_BEGIN0(
      "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
      "v8.parseOnBackgroundParsing");
  CHECK(script_streaming_task) << "BackgroundProcessor::RunScriptStreamingTask";
  script_streaming_task->Run();
  source_stream_ptr->DrainRemainingDataWithoutStreaming();
  TRACE_EVENT_END0(
      "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
      "v8.parseOnBackgroundParsing");
  TRACE_EVENT_END0(
      "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
      "v8.parseOnBackground");

  ScriptDecoder* decoder = script_decoder.get();
  decoder->FinishDecode(CrossThreadBindOnce(
      &BackgroundProcessor::OnFinishStreaming,
      std::move(background_processor_weak_ptr), std::move(streamed_source),
      std::move(script_decoder)));
}

void BackgroundResourceScriptStreamer::BackgroundProcessor::OnFinishStreaming(
    std::unique_ptr<v8::ScriptCompiler::StreamedSource> streamed_source,
    ScriptDecoderPtr script_decoder,
    ScriptDecoder::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
  TRACE_EVENT1("v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "BackgroundProcessor::OnFinishStreaming", "url",
               script_url_string_.Utf8());
  source_stream_ptr_ = nullptr;
  CHECK_EQ(state_, BackgroundProcessorState::kWaitingForParseResult);
  SetState(BackgroundProcessorState::kFinished);
  client_->PostTaskToMainThread(CrossThreadBindOnce(
      &BackgroundResourceScriptStreamer::OnResult,
      MakeUnwrappingCrossThreadWeakHandle(std::move(streamer_handle_)),
      std::make_unique<Result>(std::move(result.decoded_data),
                               std::move(result.digest),
                               std::move(streamed_source)),
      suppressed_reason_));
  client_->DidFinishBackgroundResponseProcessor(std::move(head_),
                                                std::move(result.raw_data),
                                                std::move(cached_metadata_));
}

// static
void BackgroundResourceScriptStreamer::BackgroundProcessor::
    RunConsumingCodeCacheTask(
        const String script_url_string,
        uint64_t script_resource_identifier,
        std::unique_ptr<v8::ScriptCompiler::ConsumeCodeCacheTask>
            consume_code_cache_task,
        scoped_refptr<base::SequencedTaskRunner> background_task_runner,
        mojo_base::BigBuffer cached_metadata,
        base::WeakPtr<BackgroundProcessor> background_processor_weak_ptr,
        const uint64_t trace_id) {
  TRACE_EVENT_WITH_FLOW1(
      "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
      "v8.deserializeOnBackground", TRACE_ID_LOCAL(trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "data",
      [&](perfetto::TracedValue context) {
        inspector_deserialize_script_event::Data(
            std::move(context), script_resource_identifier, script_url_string);
      });
  // Run the cache consumption task.
  consume_code_cache_task->Run();
  PostCrossThreadTask(
      *background_task_runner, FROM_HERE,
      CrossThreadBindOnce(&BackgroundProcessor::OnFinishCodeCacheConsumer,
                          std::move(background_processor_weak_ptr),
                          std::move(consume_code_cache_task),
                          std::move(cached_metadata)));
}

void BackgroundResourceScriptStreamer::BackgroundProcessor::
    OnFinishCodeCacheConsumer(
        std::unique_ptr<v8::ScriptCompiler::ConsumeCodeCacheTask>
            consume_code_cache_task,
        mojo_base::BigBuffer cached_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
  consume_code_cache_task_ = std::move(consume_code_cache_task);
  cached_metadata_ = std::move(cached_metadata);
  if (state_ == BackgroundProcessorState::kWaitingForConsumeCodeCacheResult) {
    OnFinishCodeCacheConsumerScriptDecode();
    return;
  }
  CHECK_EQ(state_, BackgroundProcessorState::
                       kWaitingForConsumeCodeCacheResultAndDecodedScript);
  CHECK(features::kBackgroundCodeCacheDecoderStart.Get());
  SetState(BackgroundProcessorState::kWaitingForDecodedScript);
}

void BackgroundResourceScriptStreamer::BackgroundProcessor::
    OnFinishScriptDecode(ScriptDecoder::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
  decoder_result_ = std::move(result);
  if (state_ == BackgroundProcessorState::kWaitingForDecodedScript) {
    OnFinishCodeCacheConsumerScriptDecode();
    return;
  }
  CHECK_EQ(state_, BackgroundProcessorState::
                       kWaitingForConsumeCodeCacheResultAndDecodedScript);
  CHECK(features::kBackgroundCodeCacheDecoderStart.Get());
  SetState(BackgroundProcessorState::kWaitingForConsumeCodeCacheResult);
}

void BackgroundResourceScriptStreamer::BackgroundProcessor::
    OnFinishCodeCacheConsumerScriptDecode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
  TRACE_EVENT1("v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "BackgroundProcessor::OnFinishCodeCacheConsumerScriptDecode",
               "url", script_url_string_.Utf8());
  CHECK(features::kBackgroundCodeCacheDecoderStart.Get());
  CHECK(consume_code_cache_task_);
  CHECK(decoder_result_);
  SetState(BackgroundProcessorState::kFinished);
  client_->PostTaskToMainThread(CrossThreadBindOnce(
      &BackgroundResourceScriptStreamer::OnResult,
      MakeUnwrappingCrossThreadWeakHandle(std::move(streamer_handle_)),
      std::make_unique<Result>(std::move(decoder_result_->decoded_data),
                               std::move(decoder_result_->digest),
                               std::move(consume_code_cache_task_)),
      NotStreamingReason::kHasCodeCacheBackground));
  client_->DidFinishBackgroundResponseProcessor(
      std::move(head_), std::move(decoder_result_->raw_data),
      std::move(cached_metadata_));
}

bool BackgroundResourceScriptStreamer::BackgroundProcessor::
    IsStreamingSuppressed() {
  return suppressed_reason_ != NotStreamingReason::kInvalid;
}

void BackgroundResourceScriptStreamer::BackgroundProcessor::SuppressStreaming(
    NotStreamingReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
  CHECK_EQ(suppressed_reason_, NotStreamingReason::kInvalid);
  CHECK_NE(reason, NotStreamingReason::kInvalid);
  SetState(BackgroundProcessorState::kStreamingSupressed);
  suppressed_reason_ = reason;

  client_->PostTaskToMainThread(CrossThreadBindOnce(
      &BackgroundResourceScriptStreamer::OnResult,
      MakeUnwrappingCrossThreadWeakHandle(std::move(streamer_handle_)),
      /*result=*/nullptr, suppressed_reason_));
}

BackgroundResourceScriptStreamer::BackgroundResourceScriptStreamer(
    ScriptResource* script_resource)
    : script_resource_(script_resource),
      script_type_(ScriptTypeForStreamingTask(script_resource)) {}

BackgroundResourceScriptStreamer::~BackgroundResourceScriptStreamer() = default;

void BackgroundResourceScriptStreamer::Trace(Visitor* visitor) const {
  visitor->Trace(script_resource_);
  ScriptStreamer::Trace(visitor);
}

v8::ScriptCompiler::StreamedSource* BackgroundResourceScriptStreamer::Source(
    v8::ScriptType expected_type) {
  CHECK(result_);
  CHECK(!IsStreamingSuppressed());
  CHECK_EQ(expected_type, script_type_);
  return result_->streamed_source.get();
}

std::unique_ptr<BackgroundResponseProcessorFactory>
BackgroundResourceScriptStreamer::CreateBackgroundResponseProcessorFactory() {
  return std::make_unique<BackgroundProcessorFactory>(
      script_resource_, MakeCrossThreadWeakHandle(this));
}

ParkableString BackgroundResourceScriptStreamer::TakeDecodedData() {
  CHECK(result_);
  CHECK(suppressed_reason_ == NotStreamingReason::kInvalid ||
        suppressed_reason_ == NotStreamingReason::kHasCodeCacheBackground);
  return ParkableString(result_->decoded_data.Impl(),
                        std::move(result_->digest));
}

std::unique_ptr<v8::ScriptCompiler::ConsumeCodeCacheTask>
BackgroundResourceScriptStreamer::TakeConsumeCodeCacheTask() {
  CHECK(result_);
  CHECK_EQ(suppressed_reason_, NotStreamingReason::kHasCodeCacheBackground);
  return std::move(result_->consume_code_cache_task);
}

v8::ScriptType BackgroundResourceScriptStreamer::GetScriptType() const {
  return script_type_;
}

void BackgroundResourceScriptStreamer::OnResult(
    std::unique_ptr<Result> result,
    NotStreamingReason suppressed_reason) {
  result_ = std::move(result);
  suppressed_reason_ = suppressed_reason;
  CHECK_EQ(!!result_, suppressed_reason_ == NotStreamingReason::kInvalid ||
                          (features::kBackgroundCodeCacheDecoderStart.Get() &&
                           suppressed_reason_ ==
                               NotStreamingReason::kHasCodeCacheBackground));
}

}  // namespace blink
