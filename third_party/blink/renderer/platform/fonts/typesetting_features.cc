// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/typesetting_features.h"

#include <array>
#include <utility>

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

constexpr std::array<const char*, TypesettingFeatures::kValueCount>
    kFeatureNames = {"Kerning", "Ligatures", "Caps"};

}  // namespace

String ToString(TypesettingFeatures features) {
  StringBuilder builder;
  StringView delimiter;
  for (TypesettingFeature feature : features) {
    builder.Append(delimiter);
    builder.Append(kFeatureNames[std::to_underlying(feature)]);
    delimiter = ",";
  }
  return builder.ReleaseString();
}

}  // namespace blink
