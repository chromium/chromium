// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HIT_TEST_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HIT_TEST_RESULT_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class TransformationMatrix;
class XRHitTestSource;
class XRPose;
class XRSpace;

class XRHitTestResult : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRHitTestResult(XRHitTestSource* hit_test_source,
                  const TransformationMatrix& pose);

  XRPose* getPose(XRSpace* relative_to);

  void Trace(blink::Visitor* visitor) override;

 private:
  Member<XRHitTestSource> hit_test_source_;
  std::unique_ptr<TransformationMatrix> pose_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HIT_TEST_RESULT_H_
