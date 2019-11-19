// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_stats_report.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"

namespace blink {

namespace {

v8::Local<v8::Value> RTCStatsToValue(ScriptState* script_state,
                                     const RTCStats* stats) {
  V8ObjectBuilder builder(script_state);

  builder.AddString("id", stats->Id());
  builder.AddNumber("timestamp", stats->Timestamp());
  builder.AddString("type", stats->GetType());

  auto add_vector = [&builder](const WebString& name, auto web_vector) {
    Vector<typename decltype(web_vector)::value_type> vector(
        SafeCast<wtf_size_t>(web_vector.size()));
    std::move(web_vector.begin(), web_vector.end(), vector.begin());
    builder.Add(name, vector);
  };

  for (size_t i = 0; i < stats->MembersCount(); ++i) {
    std::unique_ptr<RTCStatsMember> member = stats->GetMember(i);
    if (!member->IsDefined())
      continue;
    WebString name = member->GetName();
    switch (member->GetType()) {
      case webrtc::RTCStatsMemberInterface::kBool:
        builder.AddBoolean(name, member->ValueBool());
        break;
      case webrtc::RTCStatsMemberInterface::kInt32:
        builder.AddNumber(name, static_cast<double>(member->ValueInt32()));
        break;
      case webrtc::RTCStatsMemberInterface::kUint32:
        builder.AddNumber(name, static_cast<double>(member->ValueUint32()));
        break;
      case webrtc::RTCStatsMemberInterface::kInt64:
        builder.AddNumber(name, static_cast<double>(member->ValueInt64()));
        break;
      case webrtc::RTCStatsMemberInterface::kUint64:
        builder.AddNumber(name, static_cast<double>(member->ValueUint64()));
        break;
      case webrtc::RTCStatsMemberInterface::kDouble:
        builder.AddNumber(name, member->ValueDouble());
        break;
      case webrtc::RTCStatsMemberInterface::kString:
        builder.AddString(name, member->ValueString());
        break;
      case webrtc::RTCStatsMemberInterface::kSequenceBool: {
        WebVector<int> sequence = member->ValueSequenceBool();
        Vector<bool> vector(SafeCast<wtf_size_t>(sequence.size()));
        std::copy(sequence.begin(), sequence.end(), vector.begin());
        builder.Add(name, vector);
        break;
      }
      case webrtc::RTCStatsMemberInterface::kSequenceInt32:
        add_vector(name, member->ValueSequenceInt32());
        break;
      case webrtc::RTCStatsMemberInterface::kSequenceUint32:
        add_vector(name, member->ValueSequenceUint32());
        break;
      case webrtc::RTCStatsMemberInterface::kSequenceInt64:
        add_vector(name, member->ValueSequenceInt64());
        break;
      case webrtc::RTCStatsMemberInterface::kSequenceUint64:
        add_vector(name, member->ValueSequenceUint64());
        break;
      case webrtc::RTCStatsMemberInterface::kSequenceDouble:
        add_vector(name, member->ValueSequenceDouble());
        break;
      case webrtc::RTCStatsMemberInterface::kSequenceString:
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
  RTCStatsReportIterationSource(std::unique_ptr<RTCStatsReportPlatform> report)
      : report_(std::move(report)) {}

  bool Next(ScriptState* script_state,
            String& key,
            v8::Local<v8::Value>& value,
            ExceptionState& exception_state) override {
    std::unique_ptr<RTCStats> stats = report_->Next();
    if (!stats)
      return false;
    key = stats->Id();
    value = RTCStatsToValue(script_state, stats.get());
    return true;
  }

 private:
  std::unique_ptr<RTCStatsReportPlatform> report_;
};

}  // namespace

WebVector<webrtc::NonStandardGroupId> GetExposedGroupIds(
    const ScriptState* script_state) {
  const ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());
  WebVector<webrtc::NonStandardGroupId> enabled_origin_trials;
  if (RuntimeEnabledFeatures::RtcAudioJitterBufferMaxPacketsEnabled(context)) {
    enabled_origin_trials.emplace_back(
        webrtc::NonStandardGroupId::kRtcAudioJitterBufferMaxPackets);
  }
  if (RuntimeEnabledFeatures::RTCStatsRelativePacketArrivalDelayEnabled(
          context)) {
    enabled_origin_trials.emplace_back(
        webrtc::NonStandardGroupId::kRtcStatsRelativePacketArrivalDelay);
  }
  return enabled_origin_trials;
}

RTCStatsReport::RTCStatsReport(std::unique_ptr<RTCStatsReportPlatform> report)
    : report_(std::move(report)) {}

uint32_t RTCStatsReport::size() const {
  return base::saturated_cast<uint32_t>(report_->Size());
}

PairIterable<String, v8::Local<v8::Value>>::IterationSource*
RTCStatsReport::StartIteration(ScriptState*, ExceptionState&) {
  return MakeGarbageCollected<RTCStatsReportIterationSource>(
      report_->CopyHandle());
}

bool RTCStatsReport::GetMapEntry(ScriptState* script_state,
                                 const String& key,
                                 v8::Local<v8::Value>& value,
                                 ExceptionState&) {
  std::unique_ptr<RTCStats> stats = report_->GetStats(key);
  if (!stats)
    return false;
  value = RTCStatsToValue(script_state, stats.get());
  return true;
}

}  // namespace blink
