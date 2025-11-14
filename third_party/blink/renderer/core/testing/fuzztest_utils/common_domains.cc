// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fuzztest_utils/common_domains.h"

#include <string>

#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {

fuzztest::Domain<std::string> AnyPositiveIntegerString() {
  return fuzztest::Map([](int i) { return base::NumberToString(i); },
                       fuzztest::Positive<int>());
}

fuzztest::Domain<std::string> AnyIntegerString() {
  return fuzztest::Map([](int i) { return base::NumberToString(i); },
                       fuzztest::Arbitrary<int>());
}

fuzztest::Domain<std::string> AnyTrueFalseString() {
  return fuzztest::ElementOf<std::string>({"true", "false"});
}

fuzztest::Domain<std::string> AnyColorValue() {
  // TODO(crbug.com/446479786): Consider generating fully random hex/rgb/hsl
  // values (e.g., #RRGGBB with random values, rgb(r,g,b) with arbitrary
  // integers) for more comprehensive fuzzing. Starting with a fixed set of
  // known color values for simplicity and to ensure we exercise
  // common/important color formats first.
  return fuzztest::ElementOf<std::string>(
      {// Named colors
       "red", "blue", "green", "black", "white", "yellow", "orange", "purple",
       "pink", "brown", "gray", "grey", "cyan", "magenta", "lime", "maroon",
       "navy", "olive", "teal", "silver", "aqua", "fuchsia",
       // Hex colors
       "#ff0000", "#00ff00", "#0000ff", "#ffffff", "#000000", "#ffff00",
       "#ff00ff", "#00ffff", "#808080", "#c0c0c0", "#800000", "#008000",
       "#000080", "#808000", "#800080", "#008080",
       // RGB/RGBA
       "rgb(255,0,0)", "rgb(0,255,0)", "rgb(0,0,255)", "rgb(128,128,128)",
       "rgba(255,0,0,0.5)", "rgba(0,255,0,0.8)", "rgba(0,0,255,0.3)",
       // HSL
       "hsl(0,100%,50%)", "hsl(120,100%,50%)", "hsl(240,100%,50%)",
       // Special values
       "transparent", "currentcolor", "inherit", "initial", "unset", "none",
       // URL references (for gradients, patterns)
       "url(#grad1)", "url(#pattern1)", "url(#linearGradient1)"});
}

fuzztest::Domain<std::string> AnyPlausibleIdRefValue() {
  return fuzztest::Map(
      [](const std::string& num) { return base::StrCat({"id_", num}); },
      AnyPositiveIntegerString());
}

fuzztest::Domain<std::string> AnyPlausibleIdRefListValue() {
  return fuzztest::Map(
      [](base::span<const std::string> ids) {
        return base::JoinString(ids, " ");
      },
      fuzztest::VectorOf(AnyPlausibleIdRefValue())
          .WithMinSize(1)
          .WithMaxSize(3));
}

}  // namespace blink
