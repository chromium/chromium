// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fuzztest_utils/aria_domains.h"

#include "base/no_destructor.h"
#include "third_party/blink/renderer/core/accessibility/ax_utilities_generated.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/common_domains.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {

namespace {

// Helper template to convert Vector<String> from the generated functions in
// ax_utilities_generated.h into a fuzztest::Domain<std::string>.
template <auto Getter>
fuzztest::Domain<std::string> MakeAriaValueDomain() {
  static const base::NoDestructor<std::vector<std::string>> values([]() {
    const auto& blink_values = Getter();
    std::vector<std::string> out;
    out.reserve(blink_values.size());
    for (const auto& value : blink_values) {
      out.push_back(value.Utf8());
    }
    return out;
  }());
  return fuzztest::ElementOf(*values);
}

std::vector<const QualifiedName*>& GetAllAriaAttributes() {
  static base::NoDestructor<std::vector<const QualifiedName*>> all_attributes(
      []() {
        const auto& attrs = GetAriaAttributes();
        return std::vector(attrs.begin(), attrs.end());
      }());
  return *all_attributes;
}

}  // namespace

fuzztest::Domain<std::string> AnyNonAbstractAriaRole() {
  static const base::NoDestructor<std::vector<std::string>> values([]() {
    const auto& blink_vector = GetAriaRoleNames();
    std::vector<std::string> out;
    out.reserve(blink_vector.size());
    for (const auto& role : blink_vector) {
      out.push_back(role.Utf8());
    }
    return out;
  }());
  return fuzztest::ElementOf(*values);
}

fuzztest::Domain<std::string> AnyAriaAutocompleteValue() {
  return MakeAriaValueDomain<GetAriaAutocompleteValues>();
}

fuzztest::Domain<std::string> AnyAriaCheckedValue() {
  return MakeAriaValueDomain<GetAriaCheckedValues>();
}

fuzztest::Domain<std::string> AnyAriaCurrentValue() {
  return MakeAriaValueDomain<GetAriaCurrentValues>();
}

fuzztest::Domain<std::string> AnyAriaHasPopupValue() {
  return MakeAriaValueDomain<GetAriaHaspopupValues>();
}

fuzztest::Domain<std::string> AnyAriaInvalidValue() {
  return MakeAriaValueDomain<GetAriaInvalidValues>();
}

fuzztest::Domain<std::string> AnyAriaLiveValue() {
  return MakeAriaValueDomain<GetAriaLiveValues>();
}

fuzztest::Domain<std::string> AnyAriaOrientationValue() {
  return MakeAriaValueDomain<GetAriaOrientationValues>();
}

fuzztest::Domain<std::string> AnyAriaPressedValue() {
  return MakeAriaValueDomain<GetAriaPressedValues>();
}

fuzztest::Domain<std::string> AnyAriaRelevantValue() {
  return MakeAriaValueDomain<GetAriaRelevantValues>();
}

fuzztest::Domain<std::string> AnyAriaSortValue() {
  return MakeAriaValueDomain<GetAriaSortValues>();
}

fuzztest::Domain<QualifiedName> AnyAriaAttribute() {
  return fuzztest::Map(
      [](const QualifiedName* attr) { return *attr; },
      fuzztest::ElementOf<const QualifiedName*>(GetAllAriaAttributes()));
}

fuzztest::Domain<QualifiedName> AnyAriaTableAttribute() {
  static const base::NoDestructor<std::vector<QualifiedName>> attributes([]() {
    return std::vector(
        {html_names::kAriaColcountAttr, html_names::kAriaColindexAttr,
         html_names::kAriaColindextextAttr, html_names::kAriaColspanAttr,
         html_names::kAriaRowcountAttr, html_names::kAriaRowindexAttr,
         html_names::kAriaRowindextextAttr, html_names::kAriaRowspanAttr});
  }());
  return fuzztest::ElementOf(*attributes);
}

fuzztest::Domain<std::string> AnyPlausibleValueForAriaAttribute(
    const QualifiedName& attribute) {
  if (attribute == html_names::kRoleAttr) {
    return AnyNonAbstractAriaRole();
  }

  if (IsAriaBooleanAttribute(attribute)) {
    return AnyTrueFalseString();
  }

  if (IsAriaIntegerAttribute(attribute) || IsAriaDecimalAttribute(attribute)) {
    return AnyIntegerString();
  }

  if (IsAriaStringAttribute(attribute)) {
    return fuzztest::Arbitrary<std::string>();
  }

  if (IsAriaIdrefAttribute(attribute)) {
    return AnyPlausibleIdRefValue();
  }

  if (IsAriaIdrefListAttribute(attribute)) {
    return AnyPlausibleIdRefListValue();
  }

  if (IsAriaTokenAttribute(attribute) || IsAriaTokenListAttribute(attribute)) {
    if (attribute == html_names::kAriaAutocompleteAttr) {
      return AnyAriaAutocompleteValue();
    }
    if (attribute == html_names::kAriaCheckedAttr) {
      return AnyAriaCheckedValue();
    }
    if (attribute == html_names::kAriaCurrentAttr) {
      return AnyAriaCurrentValue();
    }
    if (attribute == html_names::kAriaHaspopupAttr) {
      return AnyAriaHasPopupValue();
    }
    if (attribute == html_names::kAriaInvalidAttr) {
      return AnyAriaInvalidValue();
    }
    if (attribute == html_names::kAriaLiveAttr) {
      return AnyAriaLiveValue();
    }
    if (attribute == html_names::kAriaOrientationAttr) {
      return AnyAriaOrientationValue();
    }
    if (attribute == html_names::kAriaPressedAttr) {
      return AnyAriaPressedValue();
    }
    if (attribute == html_names::kAriaRelevantAttr) {
      return AnyAriaRelevantValue();
    }
    if (attribute == html_names::kAriaSortAttr) {
      return AnyAriaSortValue();
    }
  }

  return fuzztest::Arbitrary<std::string>();
}

fuzztest::Domain<std::string> AnyValueForAriaAttribute(
    const QualifiedName& attribute) {
  return fuzztest::OneOf(AnyPlausibleValueForAriaAttribute(attribute),
                         fuzztest::Arbitrary<std::string>());
}

fuzztest::Domain<std::pair<QualifiedName, std::string>>
AnyAriaAttributeNameValuePair() {
  return fuzztest::FlatMap(
      [](const QualifiedName& attribute) {
        return fuzztest::Map(
            [attribute](std::string value) {
              return std::pair(attribute, std::move(value));
            },
            AnyValueForAriaAttribute(attribute));
      },
      AnyAriaAttribute());
}

fuzztest::Domain<std::pair<QualifiedName, std::string>>
AnyAriaTableAttributeNameValuePair() {
  return fuzztest::FlatMap(
      [](const QualifiedName& attribute) {
        return fuzztest::Map(
            [attribute](std::string value) {
              return std::pair(attribute, std::move(value));
            },
            AnyValueForAriaAttribute(attribute));
      },
      AnyAriaTableAttribute());
}

fuzztest::Domain<std::pair<QualifiedName, std::string>>
AnyAriaTableRoleNameValuePair() {
  static auto kTableRoles = std::to_array<std::string_view>(
      {"table", "grid", "treegrid", "row", "rowgroup", "cell", "gridcell",
       "columnheader", "rowheader", "none"});
  return fuzztest::Map(
      [](std::string_view role) {
        return std::pair(html_names::kRoleAttr, std::string(role));
      },
      fuzztest::ElementOf(kTableRoles));
}

}  // namespace blink
