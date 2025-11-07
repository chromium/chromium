// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fuzztest_utils/svg_domains.h"

#include <array>
#include <string>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/common_domains.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {

namespace {
const std::vector<const QualifiedName*>& GetAllSvgTags() {
  static const base::NoDestructor<std::vector<const QualifiedName*>> all_tags(
      []() {
        const base::HeapArray<const QualifiedName*> tags = svg_names::GetTags();
        return std::vector(tags.begin(), tags.end());
      }());
  return *all_tags;
}

const std::vector<const QualifiedName*>& GetAllSvgAttributes() {
  static const base::NoDestructor<std::vector<const QualifiedName*>>
      all_attributes([]() {
        const base::HeapArray<const QualifiedName*> attrs =
            svg_names::GetAttrs();
        return std::vector(attrs.begin(), attrs.end());
      }());
  return *all_attributes;
}
}  // namespace

fuzztest::Domain<QualifiedName> AnySvgTag() {
  return fuzztest::Map(
      [](const QualifiedName* tag) { return *tag; },
      fuzztest::ElementOf<const QualifiedName*>(GetAllSvgTags()));
}

fuzztest::Domain<QualifiedName> AnySvgAttribute() {
  return fuzztest::Map(
      [](const QualifiedName* attr) { return *attr; },
      fuzztest::ElementOf<const QualifiedName*>(GetAllSvgAttributes()));
}

fuzztest::Domain<std::string> AnySvgViewBoxValue() {
  return fuzztest::Map(
      [](const std::array<std::string, 4>& vals) {
        // ViewBox: min-x, min-y, width, height
        // https://svgwg.org/svg2-draft/coords.html#ViewBoxAttribute
        return base::JoinString(vals, " ");
      },
      fuzztest::ArrayOf<4>(AnyIntegerString()));
}

fuzztest::Domain<std::string> AnySvgPathValue() {
  return fuzztest::Map(
      [](base::span<const std::string> commands) {
        return base::JoinString(commands, " ");
      },
      fuzztest::VectorOf(fuzztest::OneOf(
          // Move command with absolute coordinates: "M x y"
          // https://svgwg.org/svg2-draft/paths.html#PathDataMovetoCommands
          fuzztest::Map(
              [](const std::array<std::string, 2>& coords) {
                const auto& [x, y] = coords;
                return base::StrCat({"M ", x, " ", y});
              },
              fuzztest::ArrayOf<2>(AnyIntegerString())),

          // Line command with absolute coordinates: "L x y"
          // https://svgwg.org/svg2-draft/paths.html#PathDataLinetoCommands
          fuzztest::Map(
              [](const std::array<std::string, 2>& coords) {
                const auto& [x, y] = coords;
                return base::StrCat({"L ", x, " ", y});
              },
              fuzztest::ArrayOf<2>(AnyIntegerString())),

          // Horizontal line command with absolute coordinates: "H x"
          // https://svgwg.org/svg2-draft/paths.html#PathDataLinetoCommands
          fuzztest::Map(
              [](const std::string& coord) {
                return base::StrCat({"H ", coord});
              },
              AnyIntegerString()),

          // Vertical line command with absolute coordinates: "V y"
          // https://svgwg.org/svg2-draft/paths.html#PathDataLinetoCommands
          fuzztest::Map(
              [](const std::string& coord) {
                return base::StrCat({"V ", coord});
              },
              AnyIntegerString()),

          // Close path: Z
          // https://svgwg.org/svg2-draft/paths.html#PathDataClosePathCommand
          fuzztest::Just(std::string("Z")))));
}

fuzztest::Domain<std::string> AnySvgTransformValue() {
  // https://svgwg.org/svg2-draft/coords.html#InterfaceSVGTransform
  return fuzztest::Map(
      [](base::span<const std::string> transforms) {
        return base::JoinString(transforms, " ");
      },
      fuzztest::VectorOf(
          fuzztest::OneOf(
              // translate(x y):
              fuzztest::Map(
                  [](const std::array<double, 2>& coords) {
                    return base::StrCat({"translate(",
                                         base::NumberToString(coords[0]), " ",
                                         base::NumberToString(coords[1]), ")"});
                  },
                  fuzztest::ArrayOf<2>(fuzztest::Arbitrary<double>())),

              // rotate(angle):
              fuzztest::Map(
                  [](double angle) {
                    return base::StrCat(
                        {"rotate(", base::NumberToString(angle), ")"});
                  },
                  fuzztest::Arbitrary<double>()),

              // scale(x y):
              fuzztest::Map(
                  [](const std::array<double, 2>& factors) {
                    return base::StrCat(
                        {"scale(", base::NumberToString(factors[0]), " ",
                         base::NumberToString(factors[1]), ")"});
                  },
                  fuzztest::ArrayOf<2>(fuzztest::Arbitrary<double>())),

              // skewX(angle):
              fuzztest::Map(
                  [](double angle) {
                    return base::StrCat(
                        {"skewX(", base::NumberToString(angle), ")"});
                  },
                  fuzztest::Arbitrary<double>()),

              // skewY(angle):
              fuzztest::Map(
                  [](double angle) {
                    return base::StrCat(
                        {"skewY(", base::NumberToString(angle), ")"});
                  },
                  fuzztest::Arbitrary<double>()))));
}

fuzztest::Domain<std::string> AnyPlausibleValueForSvgAttribute(
    const QualifiedName& attribute) {
  // SVG numeric attributes (positioning, dimensions, radii)
  if (attribute == svg_names::kXAttr || attribute == svg_names::kYAttr ||
      attribute == svg_names::kCxAttr || attribute == svg_names::kCyAttr ||
      attribute == svg_names::kRxAttr || attribute == svg_names::kRyAttr ||
      attribute == svg_names::kX1Attr || attribute == svg_names::kY1Attr ||
      attribute == svg_names::kX2Attr || attribute == svg_names::kY2Attr ||
      attribute == svg_names::kWidthAttr ||
      attribute == svg_names::kHeightAttr || attribute == svg_names::kRAttr) {
    return AnyIntegerString();
  }

  // SVG viewBox
  if (attribute == svg_names::kViewBoxAttr) {
    return AnySvgViewBoxValue();
  }

  // SVG path data
  if (attribute == svg_names::kDAttr) {
    return AnySvgPathValue();
  }

  // SVG fill and stroke
  if (attribute == svg_names::kFillAttr ||
      attribute == svg_names::kStrokeAttr) {
    return AnyColorValue();
  }

  // SVG transform
  if (attribute == svg_names::kTransformAttr) {
    return AnySvgTransformValue();
  }

  // Fallback to arbitrary string for unhandled attributes
  return fuzztest::Arbitrary<std::string>();
}

fuzztest::Domain<std::pair<QualifiedName, std::string>>
AnySvgAttributeNameValuePair() {
  return fuzztest::FlatMap(
      [](const QualifiedName& attribute) {
        return fuzztest::Map(
            [attribute](std::string value) {
              return std::pair(attribute, std::move(value));
            },
            AnyValueForSvgAttribute(attribute));
      },
      AnySvgAttribute());
}

fuzztest::Domain<std::string> AnyValueForSvgAttribute(
    const QualifiedName& attribute) {
  return fuzztest::OneOf(AnyPlausibleValueForSvgAttribute(attribute),
                         fuzztest::Arbitrary<std::string>());
}

}  // namespace blink
