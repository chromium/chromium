// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_server_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

PerformanceServerTiming::PerformanceServerTiming(const String& name,
                                                 double duration,
                                                 const String& description)
    : name_(name), duration_(duration), description_(description) {}

PerformanceServerTiming::~PerformanceServerTiming() = default;

ScriptValue PerformanceServerTiming::toJSONForBinding(
    ScriptState* script_state) const {
  V8ObjectBuilder builder(script_state);
  builder.AddString("name", name());
  builder.AddNumber("duration", duration());
  builder.AddString("description", description());
  return builder.GetScriptValue();
}

Vector<mojom::blink::ServerTimingInfoPtr>
PerformanceServerTiming::ParseServerTimingToMojo(
    const ResourceTimingInfo& info) {
  const ResourceResponse& response = info.FinalResponse();
  return ParseServerTimingFromHeaderValueToMojo(
      response.HttpHeaderField(http_names::kServerTiming));
}

Vector<mojom::blink::ServerTimingInfoPtr>
PerformanceServerTiming::ParseServerTimingFromHeaderValueToMojo(
    const String& value) {
  std::unique_ptr<ServerTimingHeaderVector> headers =
      ParseServerTimingHeader(value);
  Vector<mojom::blink::ServerTimingInfoPtr> result;
  result.reserve(headers->size());
  for (const auto& header : *headers) {
    result.emplace_back(mojom::blink::ServerTimingInfo::New(
        header->Name(), header->Duration(), header->Description()));
  }
  return result;
}

HeapVector<Member<PerformanceServerTiming>>
PerformanceServerTiming::ParseServerTiming(const ResourceTimingInfo& info) {
  HeapVector<Member<PerformanceServerTiming>> result;
  const ResourceResponse& response = info.FinalResponse();
  std::unique_ptr<ServerTimingHeaderVector> headers = ParseServerTimingHeader(
      response.HttpHeaderField(http_names::kServerTiming));
  result.reserve(headers->size());
  for (const auto& header : *headers) {
    result.push_back(MakeGarbageCollected<PerformanceServerTiming>(
        header->Name(), header->Duration(), header->Description()));
  }
  return result;
}

HeapVector<Member<PerformanceServerTiming>>
PerformanceServerTiming::FromParsedServerTiming(
    const Vector<mojom::blink::ServerTimingInfoPtr>& entries) {
  HeapVector<Member<PerformanceServerTiming>> result;
  for (const auto& entry : entries) {
    result.push_back(MakeGarbageCollected<PerformanceServerTiming>(
        entry->name, entry->duration, entry->description));
  }
  return result;
}

}  // namespace blink
