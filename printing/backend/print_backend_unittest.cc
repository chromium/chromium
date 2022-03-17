// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

// PrintBackendTest makes use of a real print backend instance, and thus will
// interact with printer drivers installed on a system.  This can be useful on
// machines which a developer has control over the driver installations, but is
// less useful on bots which are managed by the infra team.
// These tests are intended to be run manually by developers using the
// --run-manual flag.
class PrintBackendTest : public testing::Test {
 public:
  void SetUp() override {
    print_backend_ = PrintBackend::CreateInstance(/*locale=*/"");
  }

  PrintBackend* GetPrintBackend() { return print_backend_.get(); }

 private:
  scoped_refptr<PrintBackend> print_backend_;
};

// Check behavior of `EnumeratePrinters()`.  At least one of the tests
// {EnumeratePrintersSomeInstalled, EnumeratePrintersNoneInstalled} should
// fail, since a single machine can't have both some and no printers installed.
// A developer running these manually can verify that the appropriate test is
// passing for the given state of installed printer drivers on the system being
// checked.
TEST_F(PrintBackendTest, MANUAL_EnumeratePrintersSomeInstalled) {
  PrinterList printer_list;

  EXPECT_EQ(GetPrintBackend()->EnumeratePrinters(&printer_list),
            mojom::ResultCode::kSuccess);
  EXPECT_FALSE(printer_list.empty());

  DLOG(WARNING) << "Number of printers found: " << printer_list.size();
  for (const auto& printer : printer_list) {
    DLOG(WARNING) << "Found printer: `" << printer.printer_name << "`";
  }
}

TEST_F(PrintBackendTest, MANUAL_EnumeratePrintersNoneInstalled) {
  PrinterList printer_list;

  EXPECT_EQ(GetPrintBackend()->EnumeratePrinters(&printer_list),
            mojom::ResultCode::kSuccess);
  EXPECT_TRUE(printer_list.empty());
}

#if BUILDFLAG(IS_WIN)
// This test is for the XPS API that read the XML capabilities of a
// specific printer.
TEST_F(PrintBackendTest, MANUAL_GetXmlPrinterCapabilitiesForXpsDriver) {
  PrinterList printer_list;
  EXPECT_EQ(GetPrintBackend()->EnumeratePrinters(&printer_list),
            mojom::ResultCode::kSuccess);
  for (const auto& printer : printer_list) {
    std::string capabilities;
    EXPECT_EQ(GetPrintBackend()->GetXmlPrinterCapabilitiesForXpsDriver(
                  printer.printer_name, capabilities),
              mojom::ResultCode::kSuccess);
  }
}

TEST_F(PrintBackendTest, ParseValueForXpsPrinterCapabilities) {
  // Convert a JSON string into a base::Value.
  // Since parsing XML data to base::Value can not be done here,
  // use JSONReader to create a base::Value. The JSON string
  // in this test is based on the XML data returned by
  // `GetXmlPrinterCapabilitiesForXpsDriver` API and processed by data_decoder
  // service.
  absl::optional<base::Value> capabilities = base::JSONReader::Read(R"({
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
)");
  PrinterSemanticCapsAndDefaults printer_info;
  ASSERT_TRUE(capabilities);
  EXPECT_EQ(GetPrintBackend()->ParseValueForXpsPrinterCapabilities(
                *capabilities, &printer_info),
            mojom::ResultCode::kSuccess);
}
#endif

}  // namespace printing
