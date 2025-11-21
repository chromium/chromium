// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fuzztest_utils/html_domains.h"

#include <string>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/no_destructor.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/common_domains.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {

namespace {
const std::vector<html_names::HTMLTag>& GetAllHtmlTags() {
  static const base::NoDestructor<std::vector<html_names::HTMLTag>> all_tags(
      []() {
        std::vector<html_names::HTMLTag> tags;
        tags.reserve(html_names::kTagsCount);
        for (size_t i = 0; i < html_names::kTagsCount; ++i) {
          html_names::HTMLTag tag = static_cast<html_names::HTMLTag>(i);
          if (tag != html_names::HTMLTag::kUnknown) {
            tags.push_back(tag);
          }
        }
        return tags;
      }());
  return *all_tags;
}

const std::vector<const QualifiedName*>& GetAllHtmlAttributes() {
  static const base::NoDestructor<std::vector<const QualifiedName*>>
      all_attributes([]() {
        // Since `GetAttrs()` always makes a new `HeapArray` anyway.
        const base::HeapArray<const QualifiedName*> attrs =
            html_names::GetAttrs();
        // Eventually, we will have C++23 and the range constructor.
        return std::vector(attrs.begin(), attrs.end());
      }());
  return *all_attributes;
}
}  // namespace

fuzztest::Domain<QualifiedName> AnyHtmlTag() {
  return fuzztest::Map(
      [](html_names::HTMLTag tag) -> QualifiedName {
        return html_names::TagToQualifiedName(tag);
      },
      fuzztest::ElementOf<html_names::HTMLTag>(GetAllHtmlTags()));
}

fuzztest::Domain<QualifiedName> AnyHtmlAttribute() {
  return fuzztest::Map(
      [](const QualifiedName* attr) { return *attr; },
      fuzztest::ElementOf<const QualifiedName*>(GetAllHtmlAttributes()));
}

fuzztest::Domain<std::string> AnyPlausibleValueForHtmlAttribute(
    const QualifiedName& attribute) {
  if (attribute == html_names::kAccesskeyAttr) {
    return fuzztest::PrintableAsciiString().WithMaxSize(1);
  }
  if (attribute == html_names::kAutocompleteAttr) {
    std::vector<std::string> values = {"on",    "off",      "name",
                                       "email", "username", "current-password"};
    return fuzztest::ElementOf(std::move(values));
  }
  if (attribute == html_names::kAutofocusAttr ||
      attribute == html_names::kAutoplayAttr ||
      attribute == html_names::kCheckedAttr ||
      attribute == html_names::kControlsAttr ||
      attribute == html_names::kDisabledAttr ||
      attribute == html_names::kHiddenAttr ||
      attribute == html_names::kLoopAttr ||
      attribute == html_names::kMultipleAttr ||
      attribute == html_names::kMutedAttr ||
      attribute == html_names::kOpenAttr ||
      attribute == html_names::kReadonlyAttr ||
      attribute == html_names::kRequiredAttr ||
      attribute == html_names::kReversedAttr ||
      attribute == html_names::kSelectedAttr) {
    return fuzztest::Just(std::string(""));
  }
  if (attribute == html_names::kColspanAttr ||
      attribute == html_names::kRowspanAttr ||
      attribute == html_names::kSpanAttr) {
    return AnyPositiveIntegerString();
  }
  if (attribute == html_names::kContenteditableAttr) {
    std::vector<std::string> values = {"true", "false", ""};
    return fuzztest::ElementOf(std::move(values));
  }
  if (attribute == html_names::kDirAttr) {
    std::vector<std::string> values = {"ltr", "rtl", "auto"};
    return fuzztest::ElementOf(std::move(values));
  }
  if (attribute == html_names::kDraggableAttr ||
      attribute == html_names::kSpellcheckAttr) {
    return AnyTrueFalseString();
  }
  if (attribute == html_names::kForAttr) {
    return AnyPlausibleIdRefValue();
  }
  if (attribute == html_names::kHeightAttr ||
      attribute == html_names::kMaxAttr ||
      attribute == html_names::kMaxlengthAttr ||
      attribute == html_names::kMinAttr ||
      attribute == html_names::kMinlengthAttr ||
      attribute == html_names::kSizeAttr ||
      attribute == html_names::kStartAttr ||
      attribute == html_names::kStepAttr ||
      attribute == html_names::kTabindexAttr ||
      attribute == html_names::kWidthAttr) {
    return AnyIntegerString();
  }
  if (attribute == html_names::kLangAttr) {
    std::vector<std::string> values = {"en", "es", "fr",    "de",   "ja",
                                       "zh", "ar", "en-US", "en-GB"};
    return fuzztest::ElementOf(std::move(values));
  }
  if (attribute == html_names::kPatternAttr) {
    std::vector<std::string> values = {"[a-zA-Z]+", "\\d+", "[0-9]{3}", ".*"};
    return fuzztest::ElementOf(std::move(values));
  }
  if (attribute == html_names::kPreloadAttr) {
    std::vector<std::string> values = {"none", "metadata", "auto"};
    return fuzztest::ElementOf(std::move(values));
  }
  if (attribute == html_names::kRelAttr) {
    std::vector<std::string> values = {
        "alternate", "author", "bookmark", "external", "help",
        "license",   "next",   "nofollow", "noopener", "noreferrer",
        "prev",      "search", "tag"};
    return fuzztest::ElementOf(std::move(values));
  }
  if (attribute == html_names::kScopeAttr) {
    std::vector<std::string> values = {"row", "col", "rowgroup", "colgroup"};
    return fuzztest::ElementOf(std::move(values));
  }
  if (attribute == html_names::kTargetAttr) {
    std::vector<std::string> values = {"_blank", "_self", "_parent", "_top"};
    return fuzztest::ElementOf(std::move(values));
  }
  if (attribute == html_names::kTitleAttr) {
    return fuzztest::PrintableAsciiString();
  }
  if (attribute == html_names::kTranslateAttr) {
    std::vector<std::string> values = {"yes", "no"};
    return fuzztest::ElementOf(std::move(values));
  }
  if (attribute == html_names::kWrapAttr) {
    std::vector<std::string> values = {"soft", "hard"};
    return fuzztest::ElementOf(std::move(values));
  }

  if (attribute == html_names::kColorAttr ||
      attribute == html_names::kBgcolorAttr) {
    return AnyColorValue();
  }

  return fuzztest::Arbitrary<std::string>();
}

fuzztest::Domain<std::pair<QualifiedName, std::string>>
AnyHtmlAttributeNameValuePair() {
  return fuzztest::FlatMap(
      [](const QualifiedName& attribute) {
        return fuzztest::Map(
            [attribute](std::string value) {
              return std::pair(attribute, std::move(value));
            },
            AnyValueForHtmlAttribute(attribute));
      },
      AnyHtmlAttribute());
}

fuzztest::Domain<std::string> AnyValueForHtmlAttribute(
    const QualifiedName& attribute) {
  return fuzztest::OneOf(AnyPlausibleValueForHtmlAttribute(attribute),
                         fuzztest::Arbitrary<std::string>());
}

fuzztest::Domain<const QualifiedName*> AnyHtmlTableAttribute() {
  static const base::NoDestructor<std::vector<const QualifiedName*>>
      kAttributes(std::vector<const QualifiedName*>{
          &html_names::kScopeAttr, &html_names::kHeadersAttr,
          &html_names::kColspanAttr, &html_names::kRowspanAttr});
  return fuzztest::ElementOf<const QualifiedName*>(*kAttributes);
}

fuzztest::Domain<std::pair<QualifiedName, std::string>>
AnyHtmlTableAttributeNameValuePair() {
  return fuzztest::FlatMap(
      [](const QualifiedName* attribute) {
        return fuzztest::Map(
            [attribute](std::string value) {
              return std::make_pair(*attribute, std::move(value));
            },
            AnyValueForHtmlAttribute(*attribute));
      },
      AnyHtmlTableAttribute());
}

}  // namespace blink
