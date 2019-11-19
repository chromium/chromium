// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FEATURE_POLICY_VIOLATION_REPORT_BODY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FEATURE_POLICY_VIOLATION_REPORT_BODY_H_

#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/frame/location_report_body.h"

namespace blink {

class CORE_EXPORT FeaturePolicyViolationReportBody : public LocationReportBody {
  DEFINE_WRAPPERTYPEINFO();

 public:
  FeaturePolicyViolationReportBody(const String& feature_id,
                                   const String& message,
                                   const String& disposition)
      : feature_id_(feature_id), message_(message), disposition_(disposition) {}

  FeaturePolicyViolationReportBody(
      const String& feature_id,
      const String& message,
      const String& disposition,
      // url of the resource that violate the feature policy.
      const String& resource_url)
      : LocationReportBody(resource_url),
        feature_id_(feature_id),
        message_(message),
        disposition_(disposition) {}

  String featureId() const { return feature_id_; }
  String disposition() const { return disposition_; }
  String message() const { return message_; }
  void BuildJSONValue(V8ObjectBuilder& builder) const override;

  ~FeaturePolicyViolationReportBody() override = default;

 private:
  const String feature_id_;
  const String message_;
  const String disposition_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FEATURE_POLICY_VIOLATION_REPORT_BODY_H_
