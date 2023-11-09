// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_STREAMER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_STREAMER_H_

#include <memory>
#include <tuple>

#include "base/check_op.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_consumer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_local_compile_hints_consumer.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/script/script_scheduling_type.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "v8/include/v8.h"

namespace mojo {
class SimpleWatcher;
}

namespace blink {

class ScriptResource;
class SourceStream;
class ResponseBodyLoaderClient;

// Base class for streaming scripts. Subclasses should expose streamed script
// data by overriding the Source() method.
class CORE_EXPORT ScriptStreamer : public GarbageCollected<ScriptStreamer> {
 public:
  // For tracking why some scripts are not streamed. Not streaming is part of
  // normal operation (e.g., script already loaded, script too small) and
  // doesn't necessarily indicate a failure.
  enum class NotStreamingReason {
    kAlreadyLoaded,  // DEPRECATED
    kNotHTTP,
    kRevalidate,
    kContextNotValid,  // DEPRECATED
    kEncodingNotSupported,
    kThreadBusy,  // DEPRECATED
    kV8CannotStream,
    kScriptTooSmall,
    kNoResourceBuffer,
    kHasCodeCache,
    kStreamerNotReadyOnGetSource,  // DEPRECATED
    kInlineScript,
    kDidntTryToStartStreaming,  // DEPRECATED
    kErrorOccurred,
    kStreamingDisabled,
    kSecondScriptResourceUse,
    kWorkerTopLevelScript,
    kModuleScript,
    kNoDataPipe,
    kLoadingCancelled,
    kNonJavascriptModule,
    kDisabledByFeatureList,
    kErrorScriptTypeMismatch,

    // Pseudo values that should never be seen in reported metrics
    kMaxValue = kErrorScriptTypeMismatch,
    kInvalid = -1,
  };

  virtual ~ScriptStreamer() = default;

  virtual v8::ScriptCompiler::StreamedSource* Source(
      v8::ScriptType expected_type) = 0;
  virtual void Trace(Visitor*) const {}

  static void RecordStreamingHistogram(ScriptSchedulingType type,
                                       bool can_use_streamer,
                                       ScriptStreamer::NotStreamingReason);

  // Returns false if we cannot stream the given encoding.
  static bool ConvertEncoding(const char* encoding_name,
                              v8::ScriptCompiler::StreamedSource::Encoding*);
};

// ResourceScriptStreamer streams incomplete script data to V8 so that it can be
// parsed while it's loaded. ScriptResource holds a reference to
// ResourceScriptStreamer. If the Document and the ClassicPendingScript are
// destroyed while the streaming is in progress, and ScriptStreamer handles it
// gracefully.
class CORE_EXPORT ResourceScriptStreamer final : public ScriptStreamer {
  USING_PRE_FINALIZER(ResourceScriptStreamer, Prefinalize);

 public:
  ResourceScriptStreamer(
      ScriptResource* resource,
      mojo::ScopedDataPipeConsumerHandle data_pipe,
      ResponseBodyLoaderClient* response_body_loader_client,
      std::unique_ptr<TextResourceDecoder> decoder,
      scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner);

  ResourceScriptStreamer(const ResourceScriptStreamer&) = delete;
  ResourceScriptStreamer& operator=(const ResourceScriptStreamer&) = delete;

  ~ResourceScriptStreamer() override;
  void Trace(Visitor*) const override;

  // Get a successful ScriptStreamer for the given ScriptResource.
  // If
  // - there was no streamer,
  // - or streaming was suppressed,
  // - or the expected_type does not match the one with which the ScripStramer
  //    was started,
  // nullptr instead of a valid ScriptStreamer is returned.
  static std::tuple<ResourceScriptStreamer*, NotStreamingReason> TakeFrom(
      ScriptResource* resource,
      mojom::blink::ScriptType expected_type);

  bool IsStreamingStarted() const;     // Have we actually started streaming?
  bool CanStartStreaming() const;      // Can we still start streaming later?
  bool IsLoaded() const;               // Has loading finished?
  bool IsFinished() const;             // Has loading & streaming finished?
  bool IsStreamingSuppressed() const;  // Has streaming been suppressed?

  // ScriptStreamer implementation:
  v8::ScriptCompiler::StreamedSource* Source(
      v8::ScriptType expected_type) override {
    DCHECK_EQ(expected_type, script_type_);
    DCHECK(IsFinished());
    DCHECK(!IsStreamingSuppressed());
    return source_.get();
  }

  // Called when the script is not needed any more (e.g., loading was
  // cancelled). After calling cancel, ClassicPendingScript can drop its
  // reference to ScriptStreamer, and ScriptStreamer takes care of eventually
  // deleting itself (after the V8 side has finished too).
  void Cancel();

  NotStreamingReason StreamingSuppressedReason() const {
    CheckState();
    return suppressed_reason_;
  }

  const String& ScriptURLString() const { return script_url_string_; }
  uint64_t ScriptResourceIdentifier() const {
    return script_resource_identifier_;
  }

  v8_compile_hints::V8LocalCompileHintsConsumer*
  GetV8LocalCompileHintsConsumer() const {
    return local_compile_hints_consumer_.get();
  }

 private:
  friend class SourceStream;

  // Valid loading state transitions:
  //
  //               kLoading
  //          .--------|---------.
  //          |        |         |
  //          v        v         v
  //      kLoaded   kFailed  kCancelled
  enum class LoadingState { kLoading, kLoaded, kFailed, kCancelled };

  v8::ScriptType GetScriptType() const;

  static const char* str(LoadingState state) {
    switch (state) {
      case LoadingState::kLoading:
        return "Loading";
      case LoadingState::kLoaded:
        return "Loaded";
      case LoadingState::kFailed:
        return "Failed";
      case LoadingState::kCancelled:
        return "Cancelled";
    }
  }

  // Scripts whose first data chunk is smaller than this constant won't be
  // streamed, unless small script streaming is enabled.
  static constexpr size_t kSmallScriptThreshold = 30 * 1024;
  // Maximum size of the BOM marker.
  static constexpr size_t kMaximumLengthOfBOM = 4;

  static void RunScriptStreamingTask(
      std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask> task,
      ResourceScriptStreamer* streamer,
      SourceStream* stream);

  void OnDataPipeReadable(MojoResult result,
                          const mojo::HandleSignalsState& state);

  // Given the data we have collected already, try to start an actual V8
  // streaming task. Returns true if the task was started.
  bool TryStartStreamingTask();

  static v8::ScriptType ScriptTypeForStreamingTask(ScriptResource*);

  void Prefinalize();

  // When the streaming is suppressed, the data is not given to V8, but
  // ScriptStreamer still watches the resource load and notifies the upper
  // layers when loading is finished. It is used in situations when we have
  // started streaming but then we detect we don't want to stream (e.g., when
  // we have the code cache for the script) and we still want to parse and
  // execute it when it has finished loading.
  void SuppressStreaming(NotStreamingReason reason);

  // Called by ScriptStreamingTask when it has streamed all data to V8 and V8
  // has processed it.
  void StreamingCompleteOnBackgroundThread(LoadingState loading_state);

  // The four methods below should not be called synchronously, as they can
  // trigger script resource client callbacks.

  // Streaming completed with loading in the given |state|.
  void StreamingComplete(LoadingState loading_state);
  // Loading completed in the given state, without ever starting streaming.
  void LoadCompleteWithoutStreaming(LoadingState loading_state,
                                    NotStreamingReason no_streaming_reason);
  // Helper for the above methods to notify the client that loading has
  // completed in the given state. Streaming is guaranteed to either have
  // completed or be suppressed.
  void SendClientLoadFinishedCallback();

  bool HasEnoughDataForStreaming(size_t resource_buffer_size);

  // Has the script streamer been detached from its client. If true, then we can
  // safely abort loading and not output any more data.
  bool IsClientDetached() const;

  void AdvanceLoadingState(LoadingState new_state);
  void CheckState() const;

  LoadingState loading_state_ = LoadingState::kLoading;

  Member<ScriptResource> script_resource_;
  Member<ResponseBodyLoaderClient> response_body_loader_client_;

  // |script_decoder_| should only be accessed on the decoding thread.
  class ScriptDecoder;
  struct ScriptDecoderDeleter {
    void operator()(const ScriptDecoder* ptr);
  };
  using ScriptDecoderPtr = std::unique_ptr<ScriptDecoder, ScriptDecoderDeleter>;
  ScriptDecoderPtr script_decoder_;

  // Fields active during asynchronous (non-streaming) reads.
  mojo::ScopedDataPipeConsumerHandle data_pipe_;
  std::unique_ptr<mojo::SimpleWatcher> watcher_;

  // Fields active during streaming.
  SourceStream* stream_ = nullptr;
  std::unique_ptr<v8::ScriptCompiler::StreamedSource> source_;

  // The reason that streaming is disabled
  NotStreamingReason suppressed_reason_ = NotStreamingReason::kInvalid;

  // Keep the script URL string for event tracing.
  const String script_url_string_;

  // Keep the script resource dentifier for event tracing.
  const uint64_t script_resource_identifier_;

  // Encoding of the streamed script. Saved for sanity checking purposes.
  v8::ScriptCompiler::StreamedSource::Encoding encoding_;

  v8::ScriptType script_type_;

  // For transmitting crowdsourced compile hints to V8 while streaming.
  std::unique_ptr<v8_compile_hints::V8CrowdsourcedCompileHintsConsumer::
                      DataAndScriptNameHash>
      crowdsourced_compile_hint_callback_data_;

  // For transmitting local compile hints to V8 while streaming.
  std::unique_ptr<v8_compile_hints::V8LocalCompileHintsConsumer>
      local_compile_hints_consumer_;
};

// BackgroundInlineScriptStreamer allows parsing and compiling inline scripts in
// the background before they have been parsed by the HTML parser. Use
// InlineScriptStreamer::From() to create a ScriptStreamer from this class.
class CORE_EXPORT BackgroundInlineScriptStreamer final
    : public WTF::ThreadSafeRefCounted<BackgroundInlineScriptStreamer> {
 public:
  BackgroundInlineScriptStreamer(
      const String& text,
      v8::ScriptCompiler::CompileOptions compile_options);

  void Run();
  bool IsStarted() const { return started_.IsSet(); }
  void Cancel() { cancelled_.Set(); }

  // This may return false if V8 failed to create a background streaming task.
  bool CanStream() const { return task_.get(); }

  v8::ScriptCompiler::StreamedSource* Source(v8::ScriptType expected_type);

 private:
  friend class WTF::ThreadSafeRefCounted<BackgroundInlineScriptStreamer>;
  ~BackgroundInlineScriptStreamer() = default;

  std::unique_ptr<v8::ScriptCompiler::StreamedSource> source_;
  std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask> task_;
  base::WaitableEvent event_;
  base::AtomicFlag started_;
  base::AtomicFlag cancelled_;
};

// ScriptStreamer is garbage collected so must be created on the main thread.
// This class wraps a BackgroundInlineScriptStreamer to be used on the main
// thread.
class CORE_EXPORT InlineScriptStreamer final : public ScriptStreamer {
 public:
  static InlineScriptStreamer* From(
      scoped_refptr<BackgroundInlineScriptStreamer> streamer);

  explicit InlineScriptStreamer(
      scoped_refptr<BackgroundInlineScriptStreamer> streamer)
      : streamer_(std::move(streamer)) {}

  v8::ScriptCompiler::StreamedSource* Source(
      v8::ScriptType expected_type) override {
    return streamer_->Source(expected_type);
  }

  void Trace(Visitor* visitor) const override {
    ScriptStreamer::Trace(visitor);
  }

 private:
  scoped_refptr<BackgroundInlineScriptStreamer> streamer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_STREAMER_H_
