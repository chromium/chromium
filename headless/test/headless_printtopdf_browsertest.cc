// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/headless/command_handler/headless_command_switches.h"
#include "components/headless/test/pdf_utils.h"
#include "content/public/test/browser_test.h"
#include "headless/public/switches.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/headless_browser_test_utils.h"
#include "headless/test/headless_devtooled_browsertest.h"
#include "pdf/pdf.h"
#include "printing/buildflags/buildflags.h"
#include "printing/pdf_render_settings.h"
#include "printing/units.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace headless {

class HeadlessPDFPagesBrowserTest : public HeadlessDevTooledBrowserTest {
 public:
  const double kPaperWidth = 10;
  const double kPaperHeight = 15;
  const double kDocHeight = 50;
  // Number of color channels in a BGRA bitmap.
  const int kColorChannels = 4;
  const int kDpi = 300;

  void RunDevTooledTest() override {
    std::string script =
        "document.body.style.background = '#123456';"
        "document.body.style.height = '" +
        base::NumberToString(kDocHeight) + "in'";

    devtools_client_.SendCommand(
        "Runtime.evaluate", Param("expression", script),
        base::BindOnce(&HeadlessPDFPagesBrowserTest::OnPageSetupCompleted,
                       base::Unretained(this)));
  }

  void OnPageSetupCompleted(base::Value::Dict) {
    base::Value::Dict params;
    params.Set("printBackground", true);
    params.Set("paperHeight", kPaperHeight);
    params.Set("paperWidth", kPaperWidth);
    params.Set("marginTop", 0);
    params.Set("marginBottom", 0);
    params.Set("marginLeft", 0);
    params.Set("marginRight", 0);

    devtools_client_.SendCommand(
        "Page.printToPDF", std::move(params),
        base::BindOnce(&HeadlessPDFPagesBrowserTest::OnPDFCreated,
                       base::Unretained(this)));
  }

  void OnPDFCreated(base::Value::Dict result) {
    std::string pdf_data_base64 = DictString(result, "result.data");
    ASSERT_FALSE(pdf_data_base64.empty());

    std::string pdf_data;
    ASSERT_TRUE(base::Base64Decode(pdf_data_base64, &pdf_data));
    EXPECT_GT(pdf_data.size(), 0U);

    auto pdf_span = base::as_byte_span(pdf_data);
    int num_pages;
    EXPECT_TRUE(chrome_pdf::GetPDFDocInfo(pdf_span, &num_pages, nullptr));
    EXPECT_EQ(std::ceil(kDocHeight / kPaperHeight), num_pages);

    constexpr chrome_pdf::RenderOptions options = {
        .stretch_to_bounds = false,
        .keep_aspect_ratio = true,
        .autorotate = true,
        .use_color = true,
        .render_device_type = chrome_pdf::RenderDeviceType::kPrinter,
    };
    for (int i = 0; i < num_pages; i++) {
      std::optional<gfx::SizeF> size_in_points =
          chrome_pdf::GetPDFPageSizeByIndex(pdf_span, i);
      ASSERT_TRUE(size_in_points.has_value());
      EXPECT_EQ(static_cast<int>(size_in_points.value().width()),
                static_cast<int>(kPaperWidth * printing::kPointsPerInch));
      EXPECT_EQ(static_cast<int>(size_in_points.value().height()),
                static_cast<int>(kPaperHeight * printing::kPointsPerInch));

      gfx::Rect rect(kPaperWidth * kDpi, kPaperHeight * kDpi);
      printing::PdfRenderSettings settings(
          rect, gfx::Point(), gfx::Size(kDpi, kDpi), options.autorotate,
          options.use_color, printing::PdfRenderSettings::Mode::NORMAL);
      std::vector<uint8_t> page_bitmap_data(kColorChannels *
                                            settings.area.size().GetArea());
      EXPECT_TRUE(chrome_pdf::RenderPDFPageToBitmap(
          pdf_span, i, page_bitmap_data.data(), settings.area.size(),
          settings.dpi, options));
      EXPECT_EQ(0x56, page_bitmap_data[0]);  // B
      EXPECT_EQ(0x34, page_bitmap_data[1]);  // G
      EXPECT_EQ(0x12, page_bitmap_data[2]);  // R
    }

    FinishAsynchronousTest();
  }
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessPDFPagesBrowserTest);

class HeadlessPDFStreamBrowserTest : public HeadlessDevTooledBrowserTest {
 public:
  const double kPaperWidth = 10;
  const double kPaperHeight = 15;
  const double kDocHeight = 50;

  void RunDevTooledTest() override {
    std::string script = "document.body.style.height = '" +
                         base::NumberToString(kDocHeight) + "in'";

    devtools_client_.SendCommand(
        "Runtime.evaluate", Param("expression", script),
        base::BindOnce(&HeadlessPDFStreamBrowserTest::OnPageSetupCompleted,
                       base::Unretained(this)));
  }

  void OnPageSetupCompleted(base::Value::Dict) {
    base::Value::Dict params;
    params.Set("transferMode", "ReturnAsStream");
    params.Set("printBackground", true);
    params.Set("paperHeight", kPaperHeight);
    params.Set("paperWidth", kPaperWidth);
    params.Set("marginTop", 0);
    params.Set("marginBottom", 0);
    params.Set("marginLeft", 0);
    params.Set("marginRight", 0);

    devtools_client_.SendCommand(
        "Page.printToPDF", std::move(params),
        base::BindOnce(&HeadlessPDFStreamBrowserTest::OnPDFCreated,
                       base::Unretained(this)));
  }

  void OnPDFCreated(base::Value::Dict result) {
    EXPECT_THAT(result, DictHasValue("result.data", std::string()));

    stream_ = DictString(result, "result.stream");

    devtools_client_.SendCommand(
        "IO.read", Param("handle", std::string(stream_)),
        base::BindOnce(&HeadlessPDFStreamBrowserTest::OnReadChunk,
                       base::Unretained(this)));
  }

  void OnReadChunk(base::Value::Dict result) {
    EXPECT_THAT(result, DictHasValue("result.base64Encoded", true));

    const std::string base64_pdf_data_chunk = DictString(result, "result.data");
    base64_pdf_data_.append(base64_pdf_data_chunk);

    if (DictBool(result, "result.eof")) {
      OnPDFLoaded();
    } else {
      devtools_client_.SendCommand(
          "IO.read", Param("handle", std::string(stream_)),
          base::BindOnce(&HeadlessPDFStreamBrowserTest::OnReadChunk,
                         base::Unretained(this)));
    }
  }

  void OnPDFLoaded() {
    EXPECT_GT(base64_pdf_data_.size(), 0U);

    std::string pdf_data;
    ASSERT_TRUE(base::Base64Decode(base64_pdf_data_, &pdf_data));
    EXPECT_GT(pdf_data.size(), 0U);

    auto pdf_span = base::as_byte_span(pdf_data);

    int num_pages;
    EXPECT_TRUE(chrome_pdf::GetPDFDocInfo(pdf_span, &num_pages, nullptr));
    EXPECT_EQ(std::ceil(kDocHeight / kPaperHeight), num_pages);

    std::optional<bool> tagged = chrome_pdf::IsPDFDocTagged(pdf_span);
    ASSERT_TRUE(tagged.has_value());
    EXPECT_FALSE(tagged.value());

    FinishAsynchronousTest();
  }

 private:
  std::string stream_;
  std::string base64_pdf_data_;
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessPDFStreamBrowserTest);

class HeadlessPDFBrowserTestBase : public HeadlessDevTooledBrowserTest {
 public:
  void RunDevTooledTest() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    devtools_client_.AddEventHandler(
        "Page.loadEventFired",
        base::BindRepeating(&HeadlessPDFBrowserTestBase::OnLoadEventFired,
                            base::Unretained(this)));
    SendCommandSync(devtools_client_, "Page.enable");

    devtools_client_.SendCommand(
        "Page.navigate",
        Param("url", embedded_test_server()->GetURL(GetUrl()).spec()));
  }

  void OnLoadEventFired(const base::Value::Dict&) {
    devtools_client_.SendCommand(
        "Page.printToPDF", GetPrintToPDFParams(),
        base::BindOnce(&HeadlessPDFBrowserTestBase::OnPDFCreated,
                       base::Unretained(this)));
  }

  void OnPDFCreated(base::Value::Dict result) {
    std::optional<int> error_code = result.FindIntByDottedPath("error.code");
    const std::string* error_message =
        result.FindStringByDottedPath("error.message");
    ASSERT_EQ(error_code.has_value(), !!error_message);
    if (error_code || error_message) {
      OnPDFFailure(*error_code, *error_message);
    } else {
      std::string pdf_data_base64 = DictString(result, "result.data");
      ASSERT_FALSE(pdf_data_base64.empty());

      std::string pdf_data;
      ASSERT_TRUE(base::Base64Decode(pdf_data_base64, &pdf_data));
      ASSERT_GT(pdf_data.size(), 0U);

      auto pdf_span = base::as_byte_span(pdf_data);
      int num_pages;
      ASSERT_TRUE(chrome_pdf::GetPDFDocInfo(pdf_span, &num_pages, nullptr));
      OnPDFReady(pdf_span, num_pages);
    }

    FinishAsynchronousTest();
  }

  virtual const char* GetUrl() = 0;

  virtual base::Value::Dict GetPrintToPDFParams() {
    base::Value::Dict params;
    params.Set("printBackground", true);
    params.Set("paperHeight", 41);
    params.Set("paperWidth", 41);
    params.Set("marginTop", 0);
    params.Set("marginBottom", 0);
    params.Set("marginLeft", 0);
    params.Set("marginRight", 0);

    return params;
  }

  virtual void OnPDFReady(base::span<const uint8_t> pdf_span,
                          int num_pages) = 0;

  virtual void OnPDFFailure(int code, const std::string& message) {
    ADD_FAILURE() << "code=" << code << " message: " << message;
  }
};

class HeadlessPDFPageSizeRoundingBrowserTest
    : public HeadlessPDFBrowserTestBase {
 public:
  const char* GetUrl() override { return "/red_square.html"; }

  void OnPDFReady(base::span<const uint8_t> pdf_span, int num_pages) override {
    EXPECT_THAT(num_pages, testing::Eq(1));
  }
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessPDFPageSizeRoundingBrowserTest);

class HeadlessPDFPageOrientationBrowserTest
    : public HeadlessPDFBrowserTestBase {
 public:
  const char* GetUrl() override { return "/pages_with_orientation.html"; }

  base::Value::Dict GetPrintToPDFParams() override {
    base::Value::Dict params;
    params.Set("paperHeight", 11);
    params.Set("paperWidth", 8.5);

    return params;
  }

  void OnPDFReady(base::span<const uint8_t> pdf_span, int num_pages) override {
    ASSERT_THAT(num_pages, testing::Eq(4));

    PDFPageBitmap page_bitmap;

    // Page1 page is normal.
    ASSERT_TRUE(page_bitmap.Render(pdf_span, /*page_index=*/0));
    EXPECT_GT(page_bitmap.height(), page_bitmap.width());

    // Page2 page is clockwise.
    ASSERT_TRUE(page_bitmap.Render(pdf_span, /*page_index=*/1));
    EXPECT_LT(page_bitmap.height(), page_bitmap.width());

    // Page3 page is upright.
    ASSERT_TRUE(page_bitmap.Render(pdf_span, /*page_index=*/2));
    EXPECT_GT(page_bitmap.height(), page_bitmap.width());

    // Page4 page is counter-clockwise
    ASSERT_TRUE(page_bitmap.Render(pdf_span, /*page_index=*/3));
    EXPECT_LT(page_bitmap.height(), page_bitmap.width());
  }
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessPDFPageOrientationBrowserTest);

class HeadlessPDFPageRangesBrowserTest
    : public HeadlessPDFBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<const char*, int, const char*>> {
 public:
  const char* GetUrl() override { return "/lorem_ipsum.html"; }

  base::Value::Dict GetPrintToPDFParams() override {
    base::Value::Dict params;
    params.Set("pageRanges", page_ranges());
    params.Set("paperHeight", 8.5);
    params.Set("paperWidth", 11);
    params.Set("marginTop", 0.5);
    params.Set("marginBottom", 0.5);
    params.Set("marginLeft", 0.5);
    params.Set("marginRight", 0.5);

    return params;
  }

  void OnPDFReady(base::span<const uint8_t> pdf_span, int num_pages) override {
    EXPECT_THAT(num_pages, testing::Eq(expected_page_count()));
  }

  void OnPDFFailure(int code, const std::string& message) override {
    EXPECT_THAT(-1, testing::Eq(expected_page_count()));
    EXPECT_THAT(
        code, testing::Eq(static_cast<int>(crdtp::DispatchCode::SERVER_ERROR)));
    EXPECT_THAT(message, testing::Eq(expected_error_message()));
  }

  std::string page_ranges() { return std::get<0>(GetParam()); }
  int expected_page_count() { return std::get<1>(GetParam()); }
  std::string expected_error_message() { return std::get<2>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HeadlessPDFPageRangesBrowserTest,
    testing::Values(
        std::make_tuple("1-9", 4, ""),
        std::make_tuple("1-3", 3, ""),
        std::make_tuple("2-4", 3, ""),
        std::make_tuple("4-9", 1, ""),
        std::make_tuple("5-9", -1, "Page range exceeds page count"),
        std::make_tuple("9-5", -1, "Page range is invalid (start > end)"),
        std::make_tuple("abc", -1, "Page range syntax error")));

HEADLESS_DEVTOOLED_TEST_P(HeadlessPDFPageRangesBrowserTest);

class HeadlessPDFOOPIFBrowserTest : public HeadlessPDFBrowserTestBase {
 public:
  const char* GetUrl() override { return "/oopif.html"; }

  base::Value::Dict GetPrintToPDFParams() override {
    base::Value::Dict params;
    params.Set("printBackground", true);
    params.Set("paperHeight", 10);
    params.Set("paperWidth", 15);
    params.Set("marginTop", 0);
    params.Set("marginBottom", 0);
    params.Set("marginLeft", 0);
    params.Set("marginRight", 0);

    return params;
  }

  void OnPDFReady(base::span<const uint8_t> pdf_span, int num_pages) override {
    EXPECT_THAT(num_pages, testing::Eq(1));

    PDFPageBitmap page_image;
    ASSERT_TRUE(page_image.Render(pdf_span, 0));

    // Expect red iframe pixel at 1 inch into the page.
    EXPECT_EQ(page_image.GetPixelRGB(1 * PDFPageBitmap::kDpi,
                                     1 * PDFPageBitmap::kDpi),
              0xFF0000u);
  }
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessPDFOOPIFBrowserTest);

class HeadlessPDFTinyPageBrowserTest
    : public HeadlessPDFBrowserTestBase,
      public testing::WithParamInterface<gfx::SizeF> {
 public:
  const char* GetUrl() override { return "/hello.html"; }

  base::Value::Dict GetPrintToPDFParams() override {
    // This tests that we can print into tiny pages as some WPT
    // tests expect that.
    base::Value::Dict params;
    params.Set("paperHeight", paper_height());
    params.Set("paperWidth", paper_width());
    params.Set("marginTop", 0);
    params.Set("marginBottom", 0);
    params.Set("marginLeft", 0);
    params.Set("marginRight", 0);

    return params;
  }

  void OnPDFReady(base::span<const uint8_t> pdf_span, int num_pages) override {
    EXPECT_GT(num_pages, 0);
  }

  void OnPDFFailure(int code, const std::string& message) override {
    ADD_FAILURE() << "code=" << code << " message: " << message
                  << " paper size: " << paper_size().ToString();
  }

  gfx::SizeF paper_size() const { return GetParam(); }
  float paper_height() const { return paper_size().height(); }
  float paper_width() const { return paper_size().width(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         HeadlessPDFTinyPageBrowserTest,
                         testing::Values(gfx::SizeF(0.1, 0.1),
                                         gfx::SizeF(0.01, 0.01),
                                         gfx::SizeF(0.001, 0.001)));

HEADLESS_DEVTOOLED_TEST_P(HeadlessPDFTinyPageBrowserTest);

class HeadlessPDFOversizeMarginsBrowserTest
    : public HeadlessPDFBrowserTestBase {
 public:
  const char* GetUrl() override { return "/hello.html"; }

  base::Value::Dict GetPrintToPDFParams() override {
    // Set paper size to be smaller than the margins and expect content size
    // error.
    base::Value::Dict params;
    params.Set("paperHeight", 0.1);
    params.Set("paperWidth", 0.1);
    params.Set("marginTop", 0.2);
    params.Set("marginBottom", 0.2);
    params.Set("marginLeft", 0.2);
    params.Set("marginRight", 0.2);

    return params;
  }

  void OnPDFReady(base::span<const uint8_t> pdf_span, int num_pages) override {
    EXPECT_TRUE(false);
  }

  void OnPDFFailure(int code, const std::string& message) override {
    EXPECT_THAT(
        code,
        testing::Eq(static_cast<int>(crdtp::DispatchCode::INVALID_PARAMS)));
    EXPECT_THAT(message,
                testing::Eq("invalid print parameters: content area is empty"));
  }
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessPDFOversizeMarginsBrowserTest);

class HeadlessPDFDisableLazyLoading : public HeadlessPDFBrowserTestBase {
 public:
  const char* GetUrl() override { return "/page_with_lazy_image.html"; }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessPDFBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableLazyLoading);
  }

  base::Value::Dict GetPrintToPDFParams() override {
    base::Value::Dict params;
    params.Set("printBackground", true);

    return params;
  }

  void OnPDFReady(base::span<const uint8_t> pdf_span, int num_pages) override {
    EXPECT_THAT(num_pages, testing::Eq(5));
    PDFPageBitmap page_image;
    ASSERT_TRUE(page_image.Render(pdf_span, 4));
    EXPECT_TRUE(page_image.CheckColoredRect(SkColorSetRGB(0x00, 0x64, 0x00),
                                            SkColorSetRGB(0xff, 0xff, 0xff)));
  }
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessPDFDisableLazyLoading);

const char kExpectedStructTreeJSON[] = R"({
   "lang": "en",
   "type": "Document",
   "~children": [ {
      "type": "H1",
      "~children": [ {
         "type": "NonStruct"
      } ]
   }, {
      "type": "P",
      "~children": [ {
         "type": "NonStruct"
      } ]
   }, {
      "type": "L",
      "~children": [ {
         "type": "LI",
         "~children": [ {
            "type": "NonStruct"
         } ]
      }, {
         "type": "LI",
         "~children": [ {
            "type": "NonStruct"
         } ]
      } ]
   }, {
      "type": "Div",
      "~children": [ {
         "type": "Link",
         "~children": [ {
            "type": "NonStruct"
         } ]
      } ]
   }, {
      "type": "Table",
      "~children": [ {
         "type": "TR",
         "~children": [ {
            "type": "TH",
            "~children": [ {
               "type": "NonStruct"
            } ]
         }, {
            "type": "TH",
            "~children": [ {
               "type": "NonStruct"
            } ]
         } ]
      }, {
         "type": "TR",
         "~children": [ {
            "type": "TD",
            "~children": [ {
               "type": "NonStruct"
            } ]
         }, {
            "type": "TD",
            "~children": [ {
               "type": "NonStruct"
            } ]
         } ]
      } ]
   }, {
      "type": "H2",
      "~children": [ {
         "type": "NonStruct"
      } ]
   }, {
      "type": "Div",
      "~children": [ {
         "alt": "Car at the beach",
         "type": "Figure"
      } ]
   }, {
      "lang": "fr",
      "type": "P",
      "~children": [ {
         "type": "NonStruct"
      } ]
   } ]
}
)";

const char kExpectedFigureOnlyStructTreeJSON[] = R"({
   "lang": "en",
   "type": "Document",
   "~children": [ {
      "type": "Figure",
      "~children": [ {
         "alt": "Sample SVG image",
         "type": "Figure"
      }, {
         "type": "NonStruct",
         "~children": [ {
            "type": "NonStruct"
         } ]
      } ]
   } ]
}
)";

const char kExpectedFigureRoleOnlyStructTreeJSON[] = R"({
   "lang": "en",
   "type": "Document",
   "~children": [ {
      "alt": "Text that describes the figure.",
      "type": "Figure",
      "~children": [ {
         "alt": "Sample SVG image",
         "type": "Figure"
      }, {
         "type": "P",
         "~children": [ {
            "type": "NonStruct"
         } ]
      } ]
   } ]
}
)";

const char kExpectedImageOnlyStructTreeJSON[] = R"({
   "lang": "en",
   "type": "Document",
   "~children": [ {
      "type": "Div",
      "~children": [ {
         "alt": "Sample SVG image",
         "type": "Figure"
      } ]
   } ]
}
)";

const char kExpectedImageRoleOnlyStructTreeJSON[] = R"({
   "lang": "en",
   "type": "Document",
   "~children": [ {
      "alt": "That cat is so cute",
      "type": "Figure",
      "~children": [ {
         "type": "P",
         "~children": [ {
            "type": "NonStruct"
         } ]
      } ]
   } ]
}
)";

struct TaggedPDFTestData {
  const char* url;
  const char* expected_json;
};

constexpr TaggedPDFTestData kTaggedPDFTestData[] = {
    {"/structured_doc.html", kExpectedStructTreeJSON},
    {"/structured_doc_only_figure.html", kExpectedFigureOnlyStructTreeJSON},
    {"/structured_doc_only_figure_role.html",
     kExpectedFigureRoleOnlyStructTreeJSON},
    {"/structured_doc_only_image.html", kExpectedImageOnlyStructTreeJSON},
    {"/structured_doc_only_image_role.html",
     kExpectedImageRoleOnlyStructTreeJSON},
};

class HeadlessTaggedPDFBrowserTest
    : public HeadlessPDFBrowserTestBase,
      public ::testing::WithParamInterface<TaggedPDFTestData> {
 public:
  const char* GetUrl() override { return GetParam().url; }

  void OnPDFReady(base::span<const uint8_t> pdf_span, int num_pages) override {
    EXPECT_THAT(num_pages, testing::Eq(1));

    std::optional<bool> tagged = chrome_pdf::IsPDFDocTagged(pdf_span);
    ASSERT_THAT(tagged, testing::Optional(true));

    constexpr int kFirstPage = 0;
    base::Value struct_tree =
        chrome_pdf::GetPDFStructTreeForPage(pdf_span, kFirstPage);
    std::string json;
    base::JSONWriter::WriteWithOptions(
        struct_tree, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
    // Map Windows line endings to Unix by removing '\r'.
    base::RemoveChars(json, "\r", &json);

    EXPECT_EQ(GetParam().expected_json, json);
  }
};

HEADLESS_DEVTOOLED_TEST_P(HeadlessTaggedPDFBrowserTest);

INSTANTIATE_TEST_SUITE_P(All,
                         HeadlessTaggedPDFBrowserTest,
                         ::testing::ValuesIn(kTaggedPDFTestData));

class HeadlessTaggedPDFDisabledBrowserTest
    : public HeadlessPDFBrowserTestBase,
      public ::testing::WithParamInterface<TaggedPDFTestData> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessPDFBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisablePDFTagging);
  }

  const char* GetUrl() override { return GetParam().url; }

  void OnPDFReady(base::span<const uint8_t> pdf_span, int num_pages) override {
    EXPECT_THAT(num_pages, testing::Eq(1));

    std::optional<bool> tagged = chrome_pdf::IsPDFDocTagged(pdf_span);
    EXPECT_THAT(tagged, testing::Optional(false));
  }
};

HEADLESS_DEVTOOLED_TEST_P(HeadlessTaggedPDFDisabledBrowserTest);

INSTANTIATE_TEST_SUITE_P(All,
                         HeadlessTaggedPDFDisabledBrowserTest,
                         ::testing::ValuesIn(kTaggedPDFTestData));

class HeadlessGenerateTaggedPDFBrowserTest
    : public HeadlessPDFBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  const char* GetUrl() override { return "/structured_doc.html"; }

  base::Value::Dict GetPrintToPDFParams() override {
    base::Value::Dict params;
    params.Set("generateTaggedPDF", generate_tagged_pdf());
    return params;
  }

  bool generate_tagged_pdf() { return GetParam(); }

  void OnPDFReady(base::span<const uint8_t> pdf_span, int num_pages) override {
    EXPECT_THAT(num_pages, testing::Eq(1));

    std::optional<bool> is_pdf_tagged = chrome_pdf::IsPDFDocTagged(pdf_span);
    EXPECT_THAT(is_pdf_tagged, testing::Optional(generate_tagged_pdf()));
  }
};

HEADLESS_DEVTOOLED_TEST_P(HeadlessGenerateTaggedPDFBrowserTest);

INSTANTIATE_TEST_SUITE_P(All,
                         HeadlessGenerateTaggedPDFBrowserTest,
                         ::testing::Bool());

class HeadlessGenerateDocumentOutlinePDFBrowserTest
    : public HeadlessPDFBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  const char* GetUrl() override { return "/structured_doc.html"; }

  base::Value::Dict GetPrintToPDFParams() override {
    base::Value::Dict params;
    params.Set("generateDocumentOutline", generate_document_outline());
    return params;
  }

  bool generate_document_outline() { return GetParam(); }

  void OnPDFReady(base::span<const uint8_t> pdf_span, int num_pages) override {
    EXPECT_THAT(num_pages, testing::Eq(1));

    std::optional<bool> has_outline = chrome_pdf::PDFDocHasOutline(pdf_span);
    EXPECT_THAT(has_outline, testing::Optional(generate_document_outline()));
  }
};

HEADLESS_DEVTOOLED_TEST_P(HeadlessGenerateDocumentOutlinePDFBrowserTest);

INSTANTIATE_TEST_SUITE_P(All,
                         HeadlessGenerateDocumentOutlinePDFBrowserTest,
                         ::testing::Bool());

}  // namespace headless
