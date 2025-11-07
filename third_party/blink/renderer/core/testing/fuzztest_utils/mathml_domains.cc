// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fuzztest_utils/mathml_domains.h"

#include <string>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/no_destructor.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/common_domains.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {

namespace {
const std::vector<const QualifiedName*>& GetAllMathMlTags() {
  static const base::NoDestructor<std::vector<const QualifiedName*>> all_tags(
      []() {
        const base::HeapArray<const QualifiedName*> tags =
            mathml_names::GetTags();
        return std::vector(tags.begin(), tags.end());
      }());
  return *all_tags;
}

const std::vector<const QualifiedName*>& GetAllMathMlAttributes() {
  static const base::NoDestructor<std::vector<const QualifiedName*>>
      all_attributes([]() {
        const base::HeapArray<const QualifiedName*> attrs =
            mathml_names::GetAttrs();
        return std::vector(attrs.begin(), attrs.end());
      }());
  return *all_attributes;
}
}  // namespace

fuzztest::Domain<QualifiedName> AnyMathMlTag() {
  return fuzztest::Map(
      [](const QualifiedName* tag) { return *tag; },
      fuzztest::ElementOf<const QualifiedName*>(GetAllMathMlTags()));
}

fuzztest::Domain<QualifiedName> AnyMathMlAttribute() {
  return fuzztest::Map(
      [](const QualifiedName* attr) { return *attr; },
      fuzztest::ElementOf<const QualifiedName*>(GetAllMathMlAttributes()));
}

fuzztest::Domain<std::string> AnyPlausibleValueForMathMlAttribute(
    const QualifiedName& attribute) {
  if (attribute == mathml_names::kAccentAttr ||
      attribute == mathml_names::kAccentunderAttr ||
      attribute == mathml_names::kDisplaystyleAttr ||
      attribute == mathml_names::kLargeopAttr ||
      attribute == mathml_names::kMovablelimitsAttr ||
      attribute == mathml_names::kStretchyAttr ||
      attribute == mathml_names::kSymmetricAttr) {
    return AnyTrueFalseString();
  }
  if (attribute == mathml_names::kDisplayAttr) {
    std::vector<std::string> values = {"block", "inline"};
    return fuzztest::ElementOf(std::move(values));
  }
  if (attribute == mathml_names::kMathvariantAttr) {
    std::vector<std::string> values = {"normal",
                                       "bold",
                                       "italic",
                                       "bold-italic",
                                       "double-struck",
                                       "bold-fraktur",
                                       "script",
                                       "bold-script",
                                       "fraktur",
                                       "sans-serif",
                                       "bold-sans-serif",
                                       "sans-serif-italic",
                                       "sans-serif-bold-italic",
                                       "monospace",
                                       "initial",
                                       "tailed",
                                       "looped",
                                       "stretched"};
    return fuzztest::ElementOf(std::move(values));
  }
  if (attribute == mathml_names::kFormAttr) {
    std::vector<std::string> values = {"prefix", "infix", "postfix"};
    return fuzztest::ElementOf(std::move(values));
  }
  if (attribute == mathml_names::kMathcolorAttr ||
      attribute == mathml_names::kMathbackgroundAttr) {
    return AnyColorValue();
  }
  if (attribute == mathml_names::kColumnspanAttr ||
      attribute == mathml_names::kRowspanAttr ||
      attribute == mathml_names::kScriptlevelAttr) {
    return AnyIntegerString();
  }
  if (attribute == mathml_names::kHeightAttr ||
      attribute == mathml_names::kWidthAttr ||
      attribute == mathml_names::kDepthAttr ||
      attribute == mathml_names::kVoffsetAttr ||
      attribute == mathml_names::kLspaceAttr ||
      attribute == mathml_names::kRspaceAttr ||
      attribute == mathml_names::kLinethicknessAttr ||
      attribute == mathml_names::kMathsizeAttr ||
      attribute == mathml_names::kMinsizeAttr ||
      attribute == mathml_names::kMaxsizeAttr) {
    std::vector<std::string> size_values = {"12px",  "1.5em", "10pt", "2cm",
                                            "0.8ex", "50%",   "100%", "1.2",
                                            "0.5",   "2.0",   "auto", "fit"};
    std::vector<std::string> keyword_values = {"small", "normal", "big",
                                               "very small", "very big"};
    return fuzztest::OneOf(fuzztest::ElementOf(std::move(size_values)),
                           fuzztest::ElementOf(std::move(keyword_values)));
  }
  if (attribute == mathml_names::kEncodingAttr) {
    std::vector<std::string> values = {
        "application/mathml-presentation+xml", "application/mathml-content+xml",
        "application/openmath+xml", "text/latex"};
    return fuzztest::ElementOf(std::move(values));
  }

  return fuzztest::Arbitrary<std::string>();
}

fuzztest::Domain<std::string> AnyValueForMathMlAttribute(
    const QualifiedName& attribute) {
  return fuzztest::OneOf(AnyPlausibleValueForMathMlAttribute(attribute),
                         fuzztest::Arbitrary<std::string>());
}

fuzztest::Domain<std::pair<QualifiedName, std::string>>
AnyMathMlAttributeNameValuePair() {
  return fuzztest::FlatMap(
      [](const QualifiedName& attribute) {
        return fuzztest::Map(
            [attribute](std::string value) {
              return std::pair(attribute, std::move(value));
            },
            AnyValueForMathMlAttribute(attribute));
      },
      AnyMathMlAttribute());
}

}  // namespace blink
