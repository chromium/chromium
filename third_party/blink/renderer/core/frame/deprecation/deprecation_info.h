// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DEPRECATION_DEPRECATION_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DEPRECATION_DEPRECATION_INFO_H_

#include "third_party/blink/renderer/core/frame/web_feature_forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

extern const char kNotDeprecated[];

class DeprecationInfo final {
 public:
  static const DeprecationInfo WithTranslation(WebFeature feature,
                                               const String& type) {
    return DeprecationInfo(feature, type);
  }

  static const DeprecationInfo NotDeprecated(WebFeature feature) {
    return DeprecationInfo(feature, kNotDeprecated);
  }

  const WebFeature feature_;
  const String type_;

 private:
  DeprecationInfo(WebFeature feature, String type)
      : feature_(feature), type_(type) {}
};

// The implementation is generated using deprecation.json5 and placed in
// gen/third_party/blink/renderer/core/frame/deprecation/deprecation_info.cc
const DeprecationInfo GetDeprecationInfo(WebFeature feature);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DEPRECATION_DEPRECATION_INFO_H_
