// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/webdx_feature_tracing.h"

#include <string_view>

#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "third_party/blink/public/common/use_counter/webdx_feature_maps.h"
#include "third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/use_counter_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/dynamic_annotations.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

using WebDXFeature = mojom::blink::WebDXFeature;
using WebFeature = mojom::blink::WebFeature;
using CSSSampleId = mojom::blink::CSSSampleId;
using UseCounterFeatureType = mojom::blink::UseCounterFeatureType;

std::optional<WebDXFeature> MapToWebDXFeature(
    const UseCounterFeature& feature) {
  switch (feature.type()) {
    case UseCounterFeatureType::kWebFeature: {
      auto web_feature = static_cast<WebFeature>(feature.value());
      const auto& map = GetWebFeatureToWebDXFeatureMap();
      if (auto it = map.find(web_feature); it != map.end()) {
        return it->second;
      }
    } break;
    case UseCounterFeatureType::kCssProperty: {
      auto css_property = static_cast<CSSSampleId>(feature.value());
      const auto& map = GetCSSPropertiesToWebDXFeatureMap();
      if (auto it = map.find(css_property); it != map.end()) {
        return it->second;
      }
    } break;
    case UseCounterFeatureType::kAnimatedCssProperty: {
      auto css_property = static_cast<CSSSampleId>(feature.value());
      const auto& map = GetAnimatedCSSPropertiesToWebDXFeatureMap();
      if (auto it = map.find(css_property); it != map.end()) {
        return it->second;
      }
    } break;
    case UseCounterFeatureType::kWebDXFeature:
      return static_cast<WebDXFeature>(feature.value());
    default:
      break;
  }
  return std::nullopt;
}

// Attempts to capture source location.
// Prioritizes the V8 stack trace if JS is executing.
SourceLocation* GetSourceLocation(const LocalFrame* frame) {
  if (v8::Isolate* isolate = v8::Isolate::TryGetCurrent();
      isolate && isolate->InContext()) {
    return CaptureSourceLocation(CurrentExecutionContext(isolate));
  }
  return CaptureSourceLocation();
}

}  // namespace

std::string WebDXFeatureEnumToString(WebDXFeature feature) {
  std::string name = base::ToString(feature);
  // Obsolete features don't have an equivalent web-feature id.
  // Returning them as empty string.
  if (name.empty() || name.starts_with("kOBSOLETE_")) {
    return std::string();
  }

  std::string_view name_view(name);
  if (name_view.starts_with("kDRAFT_")) {
    name_view.remove_prefix(7);
  } else if (name_view.starts_with("k")) {
    name_view.remove_prefix(1);
  }

  std::string result;
  // Reserve space for the original length plus some potential dashes.
  result.reserve(name_view.length() + 4);
  for (size_t i = 0; i < name_view.length(); i++) {
    // Reverse the logic in
    // third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom
    // 1. Replace underscores with dashes.
    // 2. Add dashes before capital letters (except for the first character).
    // 3. Lowercase the entire string.
    const char c = name_view[i];
    if (c == '_') {
      result.push_back('-');
      continue;
    }
    if (i > 0 && IsAsciiUpper(c)) {
      result.push_back('-');
    }
    result.push_back(ToAsciiLower(c));
  }
  return result;
}

void MaybeEmitWebDXFeatureTraceEvent(const UseCounterFeature& feature,
                                     const LocalFrame* source_frame) {
  static const unsigned char* trace_category_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("blink.webdx_feature_usage");
  if (!*trace_category_enabled) {
    return;
  }

  std::optional<WebDXFeature> webdx_feature = MapToWebDXFeature(feature);
  if (!webdx_feature) {
    return;
  }

  const std::string feature_name = WebDXFeatureEnumToString(*webdx_feature);
  if (feature_name.empty()) {
    return;
  }

  String url;
  int script_id = -1;
  int line = -1;
  int column = -1;
  SourceLocation* source_location = GetSourceLocation(source_frame);
  if (source_location && !source_location->IsUnknown()) {
    url = source_location->Url();
    script_id = source_location->ScriptId();
    line = source_location->LineNumber();
    column = source_location->ColumnNumber();
  }

  // If the specific source location is unavailable we use the document's URL
  // as a fallback.
  if (url.empty() && source_frame && source_frame->DomWindow() &&
      source_frame->DomWindow()->document()) {
    url = source_frame->DomWindow()->document()->Url().GetString();
  }

  TRACE_EVENT_INSTANT("blink.webdx_feature_usage", "WebDXFeatureUsage",
                      "feature", feature_name, "url", url.Utf8(), "scriptId",
                      script_id, "lineNumber", line, "columnNumber", column);
}

}  // namespace blink
