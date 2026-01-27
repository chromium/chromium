// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/extension_script_streamer.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {
// Executed on the main thread only.
void EmitCompilationHistograms(
    scoped_refptr<BackgroundInlineScriptStreamer> script_streamer) {
  if (script_streamer->TimedOut()) {
    UMA_HISTOGRAM_TIMES(
        "Extensions.BackgroundCompileInjectedScripts."
        "ScriptCompilationTimeTimedOut",
        script_streamer->ElapsedTime());
    UMA_HISTOGRAM_MEMORY_KB(
        "Extensions.BackgroundCompileInjectedScripts.ScriptSizeTimedOut",
        script_streamer->script_length());
  } else {
    UMA_HISTOGRAM_TIMES(
        "Extensions.BackgroundCompileInjectedScripts."
        "ScriptCompilationTimeSuccess",
        script_streamer->ElapsedTime());
    UMA_HISTOGRAM_MEMORY_KB(
        "Extensions.BackgroundCompileInjectedScripts.ScriptSizeSuccess",
        script_streamer->script_length());
  }
}
}  // namespace

ExtensionScriptStreamer::ExtensionScriptStreamer(
    scoped_refptr<BackgroundInlineScriptStreamer>
        background_inline_script_streamer)
    : background_inline_script_streamer_(
          std::move(background_inline_script_streamer)) {}

InlineScriptStreamer* ExtensionScriptStreamer::GetInlineScriptStreamer() const {
  if (background_inline_script_streamer_.IsNull()) {
    return nullptr;
  }
  return MakeGarbageCollected<InlineScriptStreamer>(
      background_inline_script_streamer_.Get());
}

ExtensionScriptStreamer::~ExtensionScriptStreamer() {
  background_inline_script_streamer_.Reset();
}

ExtensionScriptStreamer::ExtensionScriptStreamer(
    const ExtensionScriptStreamer& other) {
  background_inline_script_streamer_ = other.background_inline_script_streamer_;
}

bool ExtensionScriptStreamer::CancelStreamingIfNotStarted() const {
  BackgroundInlineScriptStreamer* streamer =
      background_inline_script_streamer_.Get();
  CHECK(streamer);
  if (streamer->IsStarted()) {
    return false;
  }
  streamer->Cancel();
  return true;
}

// static
ExtensionScriptStreamer
ExtensionScriptStreamer::PostStreamingTaskToBackgroundThread(
    WebLocalFrame* web_frame,
    const WebString& content,
    const std::string_view& url,
    uint64_t script_id,
    base::TimeDelta wait_timeout) {
  String content_str = content;
  auto script_streamer = base::MakeRefCounted<BackgroundInlineScriptStreamer>(
      web_frame->GetAgentGroupScheduler()->Isolate(), content_str,
      v8::ScriptCompiler::CompileOptions::kNoCompileOptions, wait_timeout);
  scoped_refptr<base::SingleThreadTaskRunner> frame_task_runner =
      web_frame->GetTaskRunner(TaskType::kInternalDefault);
  worker_pool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      CrossThreadBindOnce(
          [](scoped_refptr<BackgroundInlineScriptStreamer> script_streamer,
             scoped_refptr<base::SingleThreadTaskRunner> frame_task_runner,
             uint64_t script_id, const std::string url) {
            TRACE_EVENT_BEGIN1(
                "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                "v8.parseOnBackground", "data",
                [&](perfetto::TracedValue context) {
                  inspector_parse_script_event::Data(std::move(context),
                                                     script_id, url.data());
                });

            script_streamer->Run();
            frame_task_runner->PostNonNestableTask(
                FROM_HERE,
                ConvertToBaseOnceCallback(CrossThreadBindOnce(
                    &EmitCompilationHistograms, std::move(script_streamer))));
            TRACE_EVENT_END0(
                "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                "v8.parseOnBackground");
          },
          script_streamer, std::move(frame_task_runner), script_id,
          std::string(url)));
  return ExtensionScriptStreamer(std::move(script_streamer));
}

}  // namespace blink
