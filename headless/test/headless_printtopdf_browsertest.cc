// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/test/browser_test.h"
#include "headless/app/headless_shell_switches.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/devtools/domains/io.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/test/headless_browser_test.h"
#include "pdf/pdf.h"
#include "printing/buildflags/buildflags.h"
#include "printing/pdf_render_settings.h"
#include "printing/units.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace headless {

namespace {

// Utility class to render the specified PDF page into a bitmap and
// inspect the resulting pixels.
class PDFPageBitmap {
 public:
  static constexpr int kColorChannels = 4;
  static constexpr int kDpi = 300;

  PDFPageBitmap() = default;
  ~PDFPageBitmap() = default;

  void Render(base::span<const uint8_t> pdf_span, int page_index) {
    absl::optional<gfx::SizeF> page_size_in_points =
        chrome_pdf::GetPDFPageSizeByIndex(pdf_span, page_index);
    ASSERT_TRUE(page_size_in_points.has_value());

    gfx::SizeF page_size_in_pixels =
        gfx::ScaleSize(page_size_in_points.value(),
                       static_cast<float>(kDpi) / printing::kPointsPerInch);

    gfx::Rect page_rect(gfx::ToCeiledSize(page_size_in_pixels));

    constexpr chrome_pdf::RenderOptions options = {
        .stretch_to_bounds = false,
        .keep_aspect_ratio = true,
        .autorotate = true,
        .use_color = true,
        .render_device_type = chrome_pdf::RenderDeviceType::kPrinter,
    };

    bitmap_size_ = page_rect.size();
    bitmap_data_.resize(kColorChannels * bitmap_size_.GetArea());
    ASSERT_TRUE(chrome_pdf::RenderPDFPageToBitmap(
        pdf_span, page_index, bitmap_data_.data(), bitmap_size_,
        gfx::Size(kDpi, kDpi), options));
  }

  uint32_t GetPixelRGB(int x, int y) {
    int pixel_index =
        bitmap_size_.width() * y * kColorChannels + x * kColorChannels;
    return bitmap_data_[pixel_index + 0]             // B
           | (bitmap_data_[pixel_index + 1] << 8)    // G
           | (bitmap_data_[pixel_index + 2] << 16);  // R
  }

 protected:
  std::vector<uint8_t> bitmap_data_;
  gfx::Size bitmap_size_;
};

}  // namespace

class HeadlessPDFPagesBrowserTest : public HeadlessAsyncDevTooledBrowserTest {
 public:
  const double kPaperWidth = 10;
  const double kPaperHeight = 15;
  const double kDocHeight = 50;
  // Number of color channels in a BGRA bitmap.
  const int kColorChannels = 4;
  const int kDpi = 300;

  void RunDevTooledTest() override {
    std::string height_expression = "document.body.style.height = '" +
                                    base::NumberToString(kDocHeight) + "in'";
    std::unique_ptr<runtime::EvaluateParams> params =
        runtime::EvaluateParams::Builder()
            .SetExpression("document.body.style.background = '#123456';" +
                           height_expression)
            .Build();
    devtools_client_->GetRuntime()->Evaluate(
        std::move(params),
        base::BindOnce(&HeadlessPDFPagesBrowserTest::OnPageSetupCompleted,
                       base::Unretained(this)));
  }

  void OnPageSetupCompleted(std::unique_ptr<runtime::EvaluateResult> result) {
    devtools_client_->GetPage()->GetExperimental()->PrintToPDF(
        page::PrintToPDFParams::Builder()
            .SetPrintBackground(true)
            .SetPaperHeight(kPaperHeight)
            .SetPaperWidth(kPaperWidth)
            .SetMarginTop(0)
            .SetMarginBottom(0)
            .SetMarginLeft(0)
            .SetMarginRight(0)
            .Build(),
        base::BindOnce(&HeadlessPDFPagesBrowserTest::OnPDFCreated,
                       base::Unretained(this)));
  }

  void OnPDFCreated(std::unique_ptr<page::PrintToPDFResult> result) {
    protocol::Binary pdf_data = result->GetData();
    EXPECT_GT(pdf_data.size(), 0U);
    auto pdf_span = base::make_span(pdf_data.data(), pdf_data.size());
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
      absl::optional<gfx::SizeF> size_in_points =
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

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessPDFPagesBrowserTest);

class HeadlessPDFStreamBrowserTest : public HeadlessAsyncDevTooledBrowserTest {
 public:
  const double kPaperWidth = 10;
  const double kPaperHeight = 15;
  const double kDocHeight = 50;

  void RunDevTooledTest() override {
    std::string height_expression = "document.body.style.height = '" +
                                    base::NumberToString(kDocHeight) + "in'";
    std::unique_ptr<runtime::EvaluateParams> params =
        runtime::EvaluateParams::Builder()
            .SetExpression(height_expression)
            .Build();
    devtools_client_->GetRuntime()->Evaluate(
        std::move(params),
        base::BindOnce(&HeadlessPDFStreamBrowserTest::OnPageSetupCompleted,
                       base::Unretained(this)));
  }

  void OnPageSetupCompleted(std::unique_ptr<runtime::EvaluateResult> result) {
    devtools_client_->GetPage()->GetExperimental()->PrintToPDF(
        page::PrintToPDFParams::Builder()
            .SetTransferMode(page::PrintToPDFTransferMode::RETURN_AS_STREAM)
            .SetPaperHeight(kPaperHeight)
            .SetPaperWidth(kPaperWidth)
            .SetMarginTop(0)
            .SetMarginBottom(0)
            .SetMarginLeft(0)
            .SetMarginRight(0)
            .Build(),
        base::BindOnce(&HeadlessPDFStreamBrowserTest::OnPDFCreated,
                       base::Unretained(this)));
  }

  void OnPDFCreated(std::unique_ptr<page::PrintToPDFResult> result) {
    EXPECT_EQ(result->GetData().size(), 0U);
    stream_ = result->GetStream();
    devtools_client_->GetIO()->Read(
        stream_, base::BindOnce(&HeadlessPDFStreamBrowserTest::OnReadChunk,
                                base::Unretained(this)));
  }

  void OnReadChunk(std::unique_ptr<io::ReadResult> result) {
    base64_data_ = base64_data_ + result->GetData();
    if (result->GetEof()) {
      OnPDFLoaded();
    } else {
      devtools_client_->GetIO()->Read(
          stream_, base::BindOnce(&HeadlessPDFStreamBrowserTest::OnReadChunk,
                                  base::Unretained(this)));
    }
  }

  void OnPDFLoaded() {
    EXPECT_GT(base64_data_.size(), 0U);
    bool success;
    protocol::Binary pdf_data =
        protocol::Binary::fromBase64(base64_data_, &success);
    EXPECT_TRUE(success);
    EXPECT_GT(pdf_data.size(), 0U);
    auto pdf_span = base::make_span(pdf_data.data(), pdf_data.size());

    int num_pages;
    EXPECT_TRUE(chrome_pdf::GetPDFDocInfo(pdf_span, &num_pages, nullptr));
    EXPECT_EQ(std::ceil(kDocHeight / kPaperHeight), num_pages);

    absl::optional<bool> tagged = chrome_pdf::IsPDFDocTagged(pdf_span);
    ASSERT_TRUE(tagged.has_value());
    EXPECT_FALSE(tagged.value());

    FinishAsynchronousTest();
  }

 private:
  std::string stream_;
  std::string base64_data_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessPDFStreamBrowserTest);

class HeadlessPDFBrowserTestBase : public HeadlessAsyncDevTooledBrowserTest,
                                   public page::Observer {
 public:
  void RunDevTooledTest() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    devtools_client_->GetPage()->AddObserver(this);

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    devtools_client_->GetPage()->Enable(run_loop.QuitClosure());
    run_loop.Run();

    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL(GetUrl()).spec());
  }

  void OnLoadEventFired(const page::LoadEventFiredParams&) override {
    devtools_client_->GetPage()->GetExperimental()->PrintToPDF(
        GetPrintToPDFParams(),
        base::BindOnce(&HeadlessPDFBrowserTestBase::OnPDFCreated,
                       base::Unretained(this)));
  }

  void OnPDFCreated(std::unique_ptr<page::PrintToPDFResult> result) {
    if (result) {
      protocol::Binary pdf_data = result->GetData();
      ASSERT_GT(pdf_data.size(), 0U);
      auto pdf_span = base::make_span(pdf_data.data(), pdf_data.size());
      int num_pages;
      ASSERT_TRUE(chrome_pdf::GetPDFDocInfo(pdf_span, &num_pages, nullptr));
      OnPDFReady(pdf_span, num_pages);
    } else {
      OnPDFFailure();
    }

    FinishAsynchronousTest();
  }

  virtual const char* GetUrl() = 0;
  virtual std::unique_ptr<page::PrintToPDFParams> GetPrintToPDFParams() {
    return page::PrintToPDFParams::Builder()
        .SetPrintBackground(true)
        .SetPaperHeight(41)
        .SetPaperWidth(41)
        .SetMarginTop(0)
        .SetMarginBottom(0)
        .SetMarginLeft(0)
        .SetMarginRight(0)
        .Build();
  }
  virtual void OnPDFReady(base::span<const uint8_t> pdf_span,
                          int num_pages) = 0;
  virtual void OnPDFFailure() { EXPECT_TRUE(false); }
};

class HeadlessPDFPageSizeRoundingBrowserTest
    : public HeadlessPDFBrowserTestBase {
 public:
  const char* GetUrl() override { return "/red_square.html"; }

  void OnPDFReady(base::span<const uint8_t> pdf_span, int num_pages) override {
    EXPECT_THAT(num_pages, testing::Eq(1));
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessPDFPageSizeRoundingBrowserTest);

class HeadlessPDFPageRangesBrowserTest
    : public HeadlessPDFBrowserTestBase,
      public testing::WithParamInterface<std::tuple<const char*, int>> {
 public:
  const char* GetUrl() override { return "/lorem_ipsum.html"; }

  std::unique_ptr<page::PrintToPDFParams> GetPrintToPDFParams() override {
    return page::PrintToPDFParams::Builder()
        .SetPaperHeight(8.5)
        .SetPaperWidth(11)
        .SetMarginTop(0.5)
        .SetMarginBottom(0.5)
        .SetMarginLeft(0.5)
        .SetMarginRight(0.5)
        .SetPageRanges(page_ranges())
        .Build();
  }

  void OnPDFReady(base::span<const uint8_t> pdf_span, int num_pages) override {
    EXPECT_THAT(num_pages, testing::Eq(expected_page_count()));
  }

  void OnPDFFailure() override {
    EXPECT_THAT(-1, testing::Eq(expected_page_count()));
  }

  std::string page_ranges() { return std::get<0>(GetParam()); }
  int expected_page_count() { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         HeadlessPDFPageRangesBrowserTest,
                         testing::Values(std::make_tuple("1-9", 4),
                                         std::make_tuple("1-3", 3),
                                         std::make_tuple("2-4", 3),
                                         std::make_tuple("4-9", 1),
                                         std::make_tuple("5-9", -1),
                                         std::make_tuple("9-5", -1),
                                         std::make_tuple("abc", -1)));

HEADLESS_ASYNC_DEVTOOLED_TEST_P(HeadlessPDFPageRangesBrowserTest);

class HeadlessPDFOOPIFBrowserTest : public HeadlessPDFBrowserTestBase {
 public:
  const char* GetUrl() override { return "/oopif.html"; }

  std::unique_ptr<page::PrintToPDFParams> GetPrintToPDFParams() override {
    return page::PrintToPDFParams::Builder()
        .SetPrintBackground(true)
        .SetPaperHeight(10)
        .SetPaperWidth(15)
        .SetMarginTop(0)
        .SetMarginBottom(0)
        .SetMarginLeft(0)
        .SetMarginRight(0)
        .Build();
  }

  void OnPDFReady(base::span<const uint8_t> pdf_span, int num_pages) override {
    EXPECT_THAT(num_pages, testing::Eq(1));

    PDFPageBitmap page_image;
    page_image.Render(pdf_span, 0);

    // Expect red iframe pixel at 1 inch into the page.
    EXPECT_EQ(page_image.GetPixelRGB(1 * PDFPageBitmap::kDpi,
                                     1 * PDFPageBitmap::kDpi),
              0xFF0000u);
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessPDFOOPIFBrowserTest);

#if BUILDFLAG(ENABLE_TAGGED_PDF)

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

    absl::optional<bool> tagged = chrome_pdf::IsPDFDocTagged(pdf_span);
    EXPECT_THAT(tagged, testing::Optional(true));

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

HEADLESS_ASYNC_DEVTOOLED_TEST_P(HeadlessTaggedPDFBrowserTest);

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

    absl::optional<bool> tagged = chrome_pdf::IsPDFDocTagged(pdf_span);
    EXPECT_THAT(tagged, testing::Optional(false));
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_P(HeadlessTaggedPDFDisabledBrowserTest);

INSTANTIATE_TEST_SUITE_P(All,
                         HeadlessTaggedPDFDisabledBrowserTest,
                         ::testing::ValuesIn(kTaggedPDFTestData));

#endif  // BUILDFLAG(ENABLE_TAGGED_PDF)

}  // namespace headless
