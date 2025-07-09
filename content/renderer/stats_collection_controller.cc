// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/stats_collection_controller.h"

#include "base/json/json_writer.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string_util.h"
#include "content/common/renderer_host.mojom.h"
#include "content/renderer/render_thread_impl.h"
#include "gin/object_template_builder.h"
#include "gin/public/wrappable_pointer_tags.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-cppgc.h"

namespace content {

// static
void StatsCollectionController::Install(blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  auto* controller = cppgc::MakeGarbageCollected<StatsCollectionController>(
      isolate->GetCppHeap()->GetAllocationHandle());
  v8::Local<v8::Object> wrapper =
      controller->GetWrapper(isolate).ToLocalChecked();
  v8::Local<v8::Object> global = context->Global();
  global
      ->Set(context, gin::StringToV8(isolate, "statsCollectionController"),
            wrapper)
      .Check();
}

gin::ObjectTemplateBuilder StatsCollectionController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<StatsCollectionController>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("getHistogram", &StatsCollectionController::GetHistogram)
      .SetMethod("getBrowserHistogram",
                 &StatsCollectionController::GetBrowserHistogram);
}

std::string StatsCollectionController::GetHistogram(
    const std::string& histogram_name) {
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(histogram_name);
  std::string output;
  if (!histogram) {
    output = "{}";
  } else {
    histogram->WriteJSON(&output, base::JSON_VERBOSITY_LEVEL_FULL);
  }
  return output;
}

std::string StatsCollectionController::GetBrowserHistogram(
    const std::string& histogram_name) {
  std::string histogram_json;
  RenderThreadImpl::current()->GetRendererHost()->GetBrowserHistogram(
      histogram_name, &histogram_json);

  return histogram_json;
}

const gin::WrapperInfo* StatsCollectionController::wrapper_info() const {
  return &kWrapperInfo;
}

}  // namespace content
