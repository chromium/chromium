// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_stats_report.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"

namespace blink {

namespace {

v8::Local<v8::Value> WebRTCStatsToValue(ScriptState* script_state,
                                        const WebRTCStats* stats) {
  V8ObjectBuilder builder(script_state);

  builder.AddString("id", stats->Id());
  builder.AddNumber("timestamp", stats->Timestamp());
  builder.AddString("type", stats->GetType());

  auto add_vector = [&builder](const WebString& name, auto web_vector) {
    Vector<typename decltype(web_vector)::value_type> vector(web_vector.size());
    std::move(web_vector.begin(), web_vector.end(), vector.begin());
    builder.Add(name, vector);
  };

  for (size_t i = 0; i < stats->MembersCount(); ++i) {
    std::unique_ptr<WebRTCStatsMember> member = stats->GetMember(i);
    if (!member->IsDefined())
      continue;
    WebString name = member->GetName();
    switch (member->GetType()) {
      case kWebRTCStatsMemberTypeBool:
        builder.AddBoolean(name, member->ValueBool());
        break;
      case kWebRTCStatsMemberTypeInt32:
        builder.AddNumber(name, static_cast<double>(member->ValueInt32()));
        break;
      case kWebRTCStatsMemberTypeUint32:
        builder.AddNumber(name, static_cast<double>(member->ValueUint32()));
        break;
      case kWebRTCStatsMemberTypeInt64:
        builder.AddNumber(name, static_cast<double>(member->ValueInt64()));
        break;
      case kWebRTCStatsMemberTypeUint64:
        builder.AddNumber(name, static_cast<double>(member->ValueUint64()));
        break;
      case kWebRTCStatsMemberTypeDouble:
        builder.AddNumber(name, member->ValueDouble());
        break;
      case kWebRTCStatsMemberTypeString:
        builder.AddString(name, member->ValueString());
        break;
      case kWebRTCStatsMemberTypeSequenceBool: {
        WebVector<int> sequence = member->ValueSequenceBool();
        Vector<bool> vector(sequence.size());
        std::copy(sequence.begin(), sequence.end(), vector.begin());
        builder.Add(name, vector);
        break;
      }
      case kWebRTCStatsMemberTypeSequenceInt32:
        add_vector(name, member->ValueSequenceInt32());
        break;
      case kWebRTCStatsMemberTypeSequenceUint32:
        add_vector(name, member->ValueSequenceUint32());
        break;
      case kWebRTCStatsMemberTypeSequenceInt64:
        add_vector(name, member->ValueSequenceInt64());
        break;
      case kWebRTCStatsMemberTypeSequenceUint64:
        add_vector(name, member->ValueSequenceUint64());
        break;
      case kWebRTCStatsMemberTypeSequenceDouble:
        add_vector(name, member->ValueSequenceDouble());
        break;
      case kWebRTCStatsMemberTypeSequenceString:
        add_vector(name, member->ValueSequenceString());
        break;
      default:
        NOTREACHED();
    }
  }

  v8::Local<v8::Object> v8_object = builder.V8Value();
  if (v8_object.IsEmpty()) {
    NOTREACHED();
    return v8::Undefined(script_state->GetIsolate());
  }
  return v8_object;
}

class RTCStatsReportIterationSource final
    : public PairIterable<String, v8::Local<v8::Value>>::IterationSource {
 public:
  RTCStatsReportIterationSource(std::unique_ptr<WebRTCStatsReport> report)
      : report_(std::move(report)) {}

  bool Next(ScriptState* script_state,
            String& key,
            v8::Local<v8::Value>& value,
            ExceptionState& exception_state) override {
    std::unique_ptr<WebRTCStats> stats = report_->Next();
    if (!stats)
      return false;
    key = stats->Id();
    value = WebRTCStatsToValue(script_state, stats.get());
    return true;
  }

 private:
  std::unique_ptr<WebRTCStatsReport> report_;
};

}  // namespace

RTCStatsReport::RTCStatsReport(std::unique_ptr<WebRTCStatsReport> report)
    : report_(std::move(report)) {}

uint32_t RTCStatsReport::size() const {
  return base::saturated_cast<uint32_t>(report_->Size());
}

PairIterable<String, v8::Local<v8::Value>>::IterationSource*
RTCStatsReport::StartIteration(ScriptState*, ExceptionState&) {
  return new RTCStatsReportIterationSource(report_->CopyHandle());
}

bool RTCStatsReport::GetMapEntry(ScriptState* script_state,
                                 const String& key,
                                 v8::Local<v8::Value>& value,
                                 ExceptionState&) {
  std::unique_ptr<WebRTCStats> stats = report_->GetStats(key);
  if (!stats)
    return false;
  value = WebRTCStatsToValue(script_state, stats.get());
  return true;
}

}  // namespace blink
