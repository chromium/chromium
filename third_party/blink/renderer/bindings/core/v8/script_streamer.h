// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_STREAMER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_STREAMER_H_

#include <memory>
#include <tuple>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/script/script_scheduling_type.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace mojo {
class SimpleWatcher;
}

namespace blink {

class ScriptResource;
class SourceStream;
class ResponseBodyLoaderClient;

// ScriptStreamer streams incomplete script data to V8 so that it can be parsed
// while it's loaded. ScriptResource holds a reference to ScriptStreamer. If the
// Document and the ClassicPendingScript are destroyed while the streaming is in
// progress, and ScriptStreamer handles it gracefully.
class CORE_EXPORT ScriptStreamer final
    : public GarbageCollected<ScriptStreamer> {
  USING_PRE_FINALIZER(ScriptStreamer, Prefinalize);

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

    // Pseudo values that should never be seen in reported metrics
    kMaxValue = kDisabledByFeatureList,
    kInvalid = -1,
  };

  ScriptStreamer(
      ScriptResource* resource,
      mojo::ScopedDataPipeConsumerHandle data_pipe,
      ResponseBodyLoaderClient* response_body_loader_client,
      scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner);
  ~ScriptStreamer();
  void Trace(Visitor*) const;

  static std::tuple<ScriptStreamer*, NotStreamingReason> TakeFrom(
      ScriptResource*);
  static void RecordStreamingHistogram(ScriptSchedulingType type,
                                       bool can_use_streamer,
                                       ScriptStreamer::NotStreamingReason);

  // Returns false if we cannot stream the given encoding.
  static bool ConvertEncoding(const char* encoding_name,
                              v8::ScriptCompiler::StreamedSource::Encoding*);

  bool IsStreamingStarted() const;     // Have we actually started streaming?
  bool CanStartStreaming() const;      // Can we still start streaming later?
  bool IsLoaded() const;               // Has loading finished?
  bool IsFinished() const;             // Has loading & streaming finished?
  bool IsStreamingSuppressed() const;  // Has streaming been suppressed?

  v8::ScriptCompiler::StreamedSource* Source() { return source_.get(); }

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

  static void SetSmallScriptThresholdForTesting(size_t threshold) {
    small_script_threshold_ = threshold;
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
  // streamed. Non-const for testing.
  static size_t small_script_threshold_;
  // Maximum size of the BOM marker.
  static constexpr size_t kMaximumLengthOfBOM = 4;

  static void RunScriptStreamingTask(
      std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask> task,
      ScriptStreamer* streamer,
      SourceStream* stream);

  void OnDataPipeReadable(MojoResult result,
                          const mojo::HandleSignalsState& state);

  // Given the data we have collected already, try to start an actual V8
  // streaming task. Returns true if the task was started.
  bool TryStartStreamingTask();

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

  scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ScriptStreamer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_STREAMER_H_
