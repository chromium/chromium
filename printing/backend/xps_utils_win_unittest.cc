// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/xps_utils_win.h"

#include "base/test/values_test_util.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"
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
  PrinterSemanticCapsAndDefaults printer_info;

  // Expect that parsing XPS Printer Capabilities is successful.
  // After parsing, `printer_info` will have 2 capabilities: "PageOutputQuality"
  // and "PageOutputColor".
  EXPECT_EQ(ParseValueForXpsPrinterCapabilities(
                base::test::ParseJson(kCorrectCapabilities), &printer_info),
            mojom::ResultCode::kSuccess);
  ASSERT_EQ(printer_info.page_output_quality->qualities, kPageOutputQualities);
}

TEST(XpsUtilTest, ParseIncorrectPageOutputQualityForXpsPrinterCapabilities) {
  PrinterSemanticCapsAndDefaults printer_info;
  EXPECT_EQ(ParseValueForXpsPrinterCapabilities(
                base::test::ParseJson(kIncorrectCapabilities), &printer_info),
            mojom::ResultCode::kFailed);
}

}  // namespace printing
