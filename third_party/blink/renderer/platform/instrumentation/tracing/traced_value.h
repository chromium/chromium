// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_TRACING_TRACED_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_TRACING_TRACED_VALUE_H_

#include "base/macros.h"
#include "base/trace_event/traced_value.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Thin wrapper around base::trace_event::TracedValue.
class PLATFORM_EXPORT TracedValue final
    : public base::trace_event::ConvertableToTraceFormat {
 public:
  TracedValue();
  ~TracedValue() override;

  void EndDictionary();
  void EndArray();

  void SetInteger(const char* name, int value);
  void SetDouble(const char* name, double value);
  void SetBoolean(const char* name, bool value);
  void SetString(const char* name, const String& value);
  void SetValue(const char* name, TracedValue* value);
  void BeginArray(const char* name);
  void BeginDictionary(const char* name);

  void SetIntegerWithCopiedName(const char* name, int value);
  void SetDoubleWithCopiedName(const char* name, double value);
  void SetBooleanWithCopiedName(const char* name, bool value);
  void SetStringWithCopiedName(const char* name, const String& value);
  void BeginArrayWithCopiedName(const char* name);
  void BeginDictionaryWithCopiedName(const char* name);

  void PushInteger(int);
  void PushDouble(double);
  void PushBoolean(bool);
  void PushString(const String&);
  void BeginArray();
  void BeginDictionary();

  String ToString() const;

 private:
  // ConvertableToTraceFormat

  void AppendAsTraceFormat(std::string*) const final;
  bool AppendToProto(ProtoAppender* appender) final;
  void EstimateTraceMemoryOverhead(
      base::trace_event::TraceEventMemoryOverhead*) final;

  base::trace_event::TracedValue traced_value_;

  DISALLOW_COPY_AND_ASSIGN(TracedValue);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_TRACING_TRACED_VALUE_H_
