// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/dactyloscoper.h"

#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/svg/svg_string_list_tear_off.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"
#include "v8/include/v8-function-callback.h"

namespace blink {

Dactyloscoper::Dactyloscoper() = default;

namespace {

bool ShouldSample(WebFeature feature) {
  return IdentifiabilityStudySettings::Get()->ShouldSampleSurface(
      IdentifiableSurface::FromTypeAndToken(
          IdentifiableSurface::Type::kWebFeature, feature));
}

using CalledJsApi = perfetto::protos::pbzero::BlinkHighEntropyAPI::CalledJsApi;
using JSFunctionArgument =
    perfetto::protos::pbzero::BlinkHighEntropyAPI::JSFunctionArgument;
using ArgumentType = perfetto::protos::pbzero::BlinkHighEntropyAPI::
    JSFunctionArgument::ArgumentType;
using ChromeTrackEvent = perfetto::protos::pbzero::ChromeTrackEvent;
using HighEntropyAPI = perfetto::protos::pbzero::BlinkHighEntropyAPI;
using ExecutionContextProto = perfetto::protos::pbzero::BlinkExecutionContext;
using SourceLocationProto = perfetto::protos::pbzero::BlinkSourceLocation;
using FontLookup = perfetto::protos::pbzero::BlinkHighEntropyAPI::FontLookup;
using FontLookupType =
    perfetto::protos::pbzero::BlinkHighEntropyAPI::FontLookup::FontLookupType;

ArgumentType GetArgumentType(v8::Local<v8::Value> value) {
  if (value->IsUndefined()) {
    return ArgumentType::UNDEFINED;
  }
  if (value->IsNull()) {
    return ArgumentType::NULL_TYPE;
  }
  if (value->IsBigInt()) {
    return ArgumentType::BIGINT;
  }
  if (value->IsBoolean()) {
    return ArgumentType::BOOLEAN;
  }
  if (value->IsFunction()) {
    return ArgumentType::FUNCTION;
  }
  if (value->IsNumber()) {
    return ArgumentType::NUMBER;
  }
  if (value->IsString()) {
    return ArgumentType::STRING;
  }
  if (value->IsSymbol()) {
    return ArgumentType::SYMBOL;
  }
  if (value->IsObject()) {
    return ArgumentType::OBJECT;
  }

  return ArgumentType::UNKNOWN_TYPE;
}

// Returns the stringified object on success and an empty string on failure
String V8ValueToString(v8::Local<v8::Context> current_context,
                       v8::Isolate* isolate,
                       const v8::Local<v8::Value>& value) {
  v8::Local<v8::String> v8_string;

  if (!value->ToDetailString(current_context).ToLocal(&v8_string)) {
    return String("");
  }

  return ToBlinkString<String>(isolate, v8_string, kDoNotExternalize);
}

FontLookupType ToTypeProto(Dactyloscoper::FontLookupType lookup_type) {
  switch (lookup_type) {
    case Dactyloscoper::FontLookupType::kUniqueOrFamilyName:
      return FontLookupType::FONT_LOOKUP_UNIQUE_OR_FAMILY_NAME;
    case Dactyloscoper::FontLookupType::kUniqueNameOnly:
      return FontLookupType::FONT_LOOKUP_UNIQUE_NAME_ONLY;
  }
}

}  // namespace

// static
void Dactyloscoper::RecordDirectSurface(ExecutionContext* context,
                                        WebFeature feature,
                                        const IdentifiableToken& value) {
  if (!context || !ShouldSample(feature))
    return;

  IdentifiabilityMetricBuilder(context->UkmSourceID())
      .AddWebFeature(feature, value)
      .Record(context->UkmRecorder());
}

// static
void Dactyloscoper::RecordDirectSurface(ExecutionContext* context,
                                        WebFeature feature,
                                        const String& str) {
  if (!context || !ShouldSample(feature))
    return;
  Dactyloscoper::RecordDirectSurface(context, feature,
                                     IdentifiabilitySensitiveStringToken(str));
}

// static
void Dactyloscoper::RecordDirectSurface(
    ExecutionContext* context,
    WebFeature feature,
    const bindings::EnumerationBase& value) {
  if (!context || !ShouldSample(feature)) {
    return;
  }
  Dactyloscoper::RecordDirectSurface(
      context, feature, IdentifiabilitySensitiveStringToken(value.AsString()));
}

// static
void Dactyloscoper::RecordDirectSurface(ExecutionContext* context,
                                        WebFeature feature,
                                        const Vector<String>& strs) {
  if (!context || !ShouldSample(feature))
    return;
  IdentifiableTokenBuilder builder;
  for (const auto& str : strs) {
    builder.AddToken(IdentifiabilitySensitiveStringToken(str));
  }
  Dactyloscoper::RecordDirectSurface(context, feature, builder.GetToken());
}

// static
void Dactyloscoper::RecordDirectSurface(ExecutionContext* context,
                                        WebFeature feature,
                                        const DOMArrayBufferView* buffer) {
  if (!context || !ShouldSample(feature))
    return;
  IdentifiableTokenBuilder builder;
  if (buffer && buffer->byteLength() > 0) {
    builder.AddBytes(buffer->ByteSpan());
  }
  Dactyloscoper::RecordDirectSurface(context, feature, builder.GetToken());
}

// static
void Dactyloscoper::RecordDirectSurface(ExecutionContext* context,
                                        WebFeature feature,
                                        SVGStringListTearOff* strings) {
  RecordDirectSurface(context, feature, strings->Values());
}

// static
void Dactyloscoper::TraceFontLookup(ExecutionContext* execution_context,
                                    const AtomicString& name,
                                    const FontDescription& font_description,
                                    Dactyloscoper::FontLookupType lookup_type) {
  TRACE_EVENT_INSTANT(
      TRACE_DISABLED_BY_DEFAULT("identifiability.high_entropy_api"),
      "HighEntropyFontLookup", [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<ChromeTrackEvent>();

        HighEntropyAPI& high_entropy_api = *(event->set_high_entropy_api());

        ExecutionContextProto* proto_context =
            high_entropy_api.set_execution_context();
        execution_context->WriteIntoTrace(ctx.Wrap(proto_context));

        std::unique_ptr<SourceLocation> source_location =
            CaptureSourceLocation(execution_context);
        SourceLocationProto* proto_source_location =
            high_entropy_api.set_source_location();
        source_location->WriteIntoTrace(ctx.Wrap(proto_source_location));

        FontLookup& font_lookup = *(high_entropy_api.set_font_lookup());
        font_lookup.set_type(ToTypeProto(lookup_type));
        font_lookup.set_name(name.Utf8());
        FontSelectionRequest font_selection_request =
            font_description.GetFontSelectionRequest();
        font_lookup.set_weight(font_selection_request.weight.RawValue());
        font_lookup.set_width(font_selection_request.width.RawValue());
        font_lookup.set_slope(font_selection_request.slope.RawValue());
      });
}

Dactyloscoper::HighEntropyTracer::HighEntropyTracer(
    const char* called_api_name,
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  TRACE_EVENT_BEGIN(
      TRACE_DISABLED_BY_DEFAULT("identifiability.high_entropy_api"),
      "HighEntropyJavaScriptAPICall", [&](perfetto::EventContext ctx) {
        v8::Isolate* isolate = info.GetIsolate();
        v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
        ExecutionContext* execution_context =
            ExecutionContext::From(current_context);

        if (!execution_context) {
          return;
        }

        auto* event = ctx.event<ChromeTrackEvent>();

        HighEntropyAPI& high_entropy_api = *(event->set_high_entropy_api());

        ExecutionContextProto* proto_context =
            high_entropy_api.set_execution_context();
        execution_context->WriteIntoTrace(ctx.Wrap(proto_context));

        CalledJsApi& called_api = *(high_entropy_api.set_called_api());
        called_api.set_identifier(called_api_name);

        for (int i = 0; i < info.Length(); ++i) {
          JSFunctionArgument& arg = *(called_api.add_func_arguments());
          arg.set_type(GetArgumentType(info[i]));
          arg.set_value(
              V8ValueToString(current_context, isolate, info[i]).Utf8());
        }

        std::unique_ptr<SourceLocation> source_location =
            CaptureSourceLocation(execution_context);
        SourceLocationProto* proto_source_location =
            high_entropy_api.set_source_location();
        source_location->WriteIntoTrace(ctx.Wrap(proto_source_location));
      });
}

Dactyloscoper::HighEntropyTracer::~HighEntropyTracer() {
  TRACE_EVENT_END(
      TRACE_DISABLED_BY_DEFAULT("identifiability.high_entropy_api"));
}

}  // namespace blink
