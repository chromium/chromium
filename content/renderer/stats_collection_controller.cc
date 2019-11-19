// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/stats_collection_controller.h"

#include "base/json/json_writer.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string_util.h"
#include "content/common/renderer_host.mojom.h"
#include "content/public/common/service_names.mojom.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

// static
gin::WrapperInfo StatsCollectionController::kWrapperInfo = {
    gin::kEmbedderNativeGin
};

// static
void StatsCollectionController::Install(blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<StatsCollectionController> controller =
      gin::CreateHandle(isolate, new StatsCollectionController());
  if (controller.IsEmpty())
    return;
  v8::Local<v8::Object> global = context->Global();
  global
      ->Set(context, gin::StringToV8(isolate, "statsCollectionController"),
            controller.ToV8())
      .Check();
}

StatsCollectionController::StatsCollectionController() {}

StatsCollectionController::~StatsCollectionController() {}

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

}  // namespace content
