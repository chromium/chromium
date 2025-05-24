// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/typesetting_features.h"

#include <array>

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

std::array<const char*, kMaxTypesettingFeatureIndex + 1> kFeatureNames = {
    "Kerning", "Ligatures", "Caps"};

}  // namespace

String ToString(TypesettingFeatures features) {
  StringBuilder builder;
  int featureCount = 0;
  for (int i = 0; i <= kMaxTypesettingFeatureIndex; i++) {
    if (features & (1 << i)) {
      if (featureCount++ > 0)
        builder.Append(",");
      builder.Append(kFeatureNames[i]);
    }
  }
  return builder.ToString();
}

}  // namespace blink
