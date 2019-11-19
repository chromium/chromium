// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REPORT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/report_body.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// The constants are implemented as static members of a class to have an unique
// address and not violate ODR.
struct CORE_EXPORT ReportType {
  static constexpr const char kDeprecation[] = "deprecation";
  static constexpr const char kFeaturePolicyViolation[] =
      "feature-policy-violation";
  static constexpr const char kIntervention[] = "intervention";
  static constexpr const char kCSPViolation[] = "csp-violation";
};

class CORE_EXPORT Report : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Report(const String& type, const String& url, ReportBody* body)
      : type_(type), url_(url), body_(body) {}

  ~Report() override = default;

  String type() const { return type_; }
  String url() const { return url_; }
  ReportBody* body() const { return body_; }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(body_);
    ScriptWrappable::Trace(visitor);
  }

  ScriptValue toJSON(ScriptState* script_state) const;

 private:
  const String type_;
  const String url_;
  Member<ReportBody> body_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REPORT_H_
