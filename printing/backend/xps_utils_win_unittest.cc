// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/xps_utils_win.h"

#include <utility>

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_test_constants.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {
// The correct format of XPS "PageOutputQuality" and "PageOutputColor"
// capabilities.
constexpr char kCorrectCapabilities[] = R"({
  "type": "element",
  "tag": "psf:PrintCapabilities",
  "children": [
    {
      "type": "element",
      "tag": "psf:Feature",
      "attributes": {
        "name": "psk:PageOutputQuality"
      },
      "children": [
        {
          "type": "element",
          "tag": "psf:Feature",
          "attributes": {
            "name": "psk:PageOutputQuality"
          }
        },
        {
          "type": "element",
          "tag": "psf:Property",
          "attributes": {
            "name": "psf:SelectionType"
          },
          "children": [
            {
              "type": "element",
              "tag": "psf:Value",
              "attributes": {
                "xsi:type": "xsd:QName"
              },
              "children": [
                {
                  "type": "text",
                  "text": "psk:PickOne"
                }
              ]
            }
          ]
        },
        {
          "type": "element",
          "tag": "psf:Property",
          "attributes": {
            "name": "psf:DisplayName"
          },
          "children": [
            {
              "type": "element",
              "tag": "psf:Value",
              "attributes": {
                "xsi:type": "xsd:string"
              },
              "children": [
                {
                  "type": "text",
                  "text": "Quality"
                }
              ]
            }
          ]
        },
        {
          "type": "element",
          "tag": "psf:Option",
          "attributes": {
            "name": "ns0000:Draft",
            "constrain": "psk:None"
          },
          "children": [
            {
              "type": "element",
              "tag": "psf:Property",
              "attributes": {
                "name": "psf:DisplayName"
              },
              "children": [
                {
                  "type": "element",
                  "tag": "psf:Value",
                  "attributes": {
                    "xsi:type": "xsd:string"
                  },
                  "children": [
                    {
                      "type": "text",
                      "text": "Draft"
                    }
                  ]
                }
              ]
            }
          ]
        },
        {
          "type": "element",
          "tag": "psf:Option",
          "attributes": {
            "name": "ns0000:Advanced",
            "constrain": "psk:None"
          },
          "children": [
            {
              "type": "element",
              "tag": "psf:Property",
              "attributes": {
                "name": "psf:DisplayName"
              },
              "children": [
                {
                  "type": "element",
                  "tag": "psf:Value",
                  "attributes": {
                    "xsi:type": "xsd:string"
                  },
                  "children": [
                    {
                      "type": "text",
                      "text": "Advanced"
                    }
                  ]
                }
              ]
            }
          ]
        },
        {
          "type": "element",
          "tag": "psf:Option",
          "attributes": {
            "name": "psk:Normal"
          }
        }
      ]
    },
    {
      "type": "element",
      "tag": "psf:Feature",
      "attributes": {
        "name": "psk:PageOutputColor"
      }
    }
  ]
}
)";

// The incorrect format of XPS `PageOutputQuality` capability.
// The property inside option ns0000:Draft does not have any value.
constexpr char kIncorrectCapabilities[] = R"({
  "type": "element",
  "tag": "psf:PrintCapabilities",
  "children": [
    {
      "type": "element",
      "tag": "psf:Feature",
      "attributes": {
        "name": "psk:PageOutputQuality"
      },
      "children": [
        {
          "type": "element",
          "tag": "psf:Feature",
          "attributes": {
            "name": "psk:PageOutputQuality"
          }
        },
        {
          "type": "element",
          "tag": "psf:Property",
          "attributes": {
            "name": "psf:SelectionType"
          },
          "children": [
            {
              "type": "element",
              "tag": "psf:Value",
              "attributes": {
                "xsi:type": "xsd:QName"
              },
              "children": [
                {
                  "type": "text",
                  "text": "psk:PickOne"
                }
              ]
            }
          ]
        },
        {
          "type": "element",
          "tag": "psf:Property",
          "attributes": {
            "name": "psf:DisplayName"
          },
          "children": [
            {
              "type": "element",
              "tag": "psf:Value",
              "attributes": {
                "xsi:type": "xsd:string"
              },
              "children": [
                {
                  "type": "text",
                  "text": "Quality"
                }
              ]
            }
          ]
        },
        {
          "type": "element",
          "tag": "psf:Option",
          "attributes": {
            "name": "ns0000:Draft",
            "constrain": "psk:None"
          },
          "children": [
            {
              "type": "element",
              "tag": "psf:Property",
              "attributes": {
                "name": "psf:DisplayName"
              }
            }
          ]
        }
      ]
    }
  ]
}
)";

const PageOutputQualityAttributes kPageOutputQualities = {
    PageOutputQualityAttribute("Draft", "ns0000:Draft"),
    PageOutputQualityAttribute("Advanced", "ns0000:Advanced"),
    PageOutputQualityAttribute("", "psk:Normal")};

}  // namespace

TEST(XpsUtilTest, ParseCorrectPageOutputQualityForXpsPrinterCapabilities) {
  // Assert that parsing XPS printer capabilities is successful.
  ASSERT_OK_AND_ASSIGN(const XpsCapabilities result,
                       ParseValueForXpsPrinterCapabilities(
                           base::test::ParseJson(kCorrectCapabilities)));
  ASSERT_TRUE(result.page_output_quality);
  EXPECT_EQ(result.page_output_quality->qualities, kPageOutputQualities);
}

TEST(XpsUtilTest, ParseIncorrectPageOutputQualityForXpsPrinterCapabilities) {
  // The property inside option ns0000:Draft does not have any value,
  // so parsing XPS printer capabilities should fail.
  EXPECT_THAT(ParseValueForXpsPrinterCapabilities(
                  base::test::ParseJson(kIncorrectCapabilities)),
              base::test::ErrorIs(mojom::ResultCode::kFailed));
}

TEST(XpsUtilTest, MergeXpsCapabilitiesPageOutputQuality) {
  PrinterSemanticCapsAndDefaults printer_capabilities =
      GenerateSamplePrinterSemanticCapsAndDefaults({});

  XpsCapabilities xps_capabilities;
  xps_capabilities.page_output_quality = kPageOutputQuality;

  MergeXpsCapabilities(std::move(xps_capabilities), printer_capabilities);

  // Expect that XPS capability PageOutputQuality was successfully merged into a
  // PrinterSemanticCapsAndDefaults object.
  ASSERT_TRUE(printer_capabilities.page_output_quality.has_value());
  EXPECT_EQ(printer_capabilities.page_output_quality.value(),
            kPageOutputQuality);

  // Expect that non-XPS capabilities remain unmodified.
  printer_capabilities.page_output_quality = std::nullopt;
  EXPECT_EQ(printer_capabilities,
            GenerateSamplePrinterSemanticCapsAndDefaults({}));
}

}  // namespace printing
