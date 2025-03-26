// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "build/build_config.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "printing/mojom/print.mojom.h"
#include "printing/mojom/printing_context.mojom.h"
#include "printing/page_range.h"
#include "printing/page_setup.h"
#include "printing/print_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

// Values for PageMargins, just want non-zero and all different.
const PageMargins kPageMarginNonzero(/*header=*/1,
                                     /*footer=*/2,
                                     /*left=*/3,
                                     /*right=*/4,
                                     /*top=*/5,
                                     /*bottom=*/6);

constexpr int kPageSetupTextHeight = 1;
constexpr gfx::Size kPageSetupPhysicalSize =
    gfx::Size(/*width=*/100, /*height=*/200);
constexpr gfx::Rect kPageSetupPrintableArea =
    gfx::Rect(/*x=*/5, /*y=*/10, /*width=*/80, /*height=*/170);
const PageMargins kPageSetupRequestedMargins(
    /*header=*/10,
    /*footer=*/20,
    /*left=*/5,
    /*right=*/15,
    /*top=*/10 + kPageSetupTextHeight,
    /*bottom=*/20 + kPageSetupTextHeight);

const PageSetup kPageSetupAsymmetricalMargins(kPageSetupPhysicalSize,
                                              kPageSetupPrintableArea,
                                              kPageSetupRequestedMargins,
                                              /*forced_margins=*/false,
                                              kPageSetupTextHeight);
const PageSetup kPageSetupForcedMargins(kPageSetupPhysicalSize,
                                        kPageSetupPrintableArea,
                                        kPageSetupRequestedMargins,
                                        /*forced_margins=*/true,
                                        kPageSetupTextHeight);

constexpr gfx::Size kRequestedMediaSize =
    gfx::Size(/*width=*/25, /*height=*/75);
const char kRequestedMediaVendorId[] = "iso-foo";

PrintSettings::RequestedMedia GenerateSampleRequestedMedia() {
  PrintSettings::RequestedMedia media;
  media.size_microns = kRequestedMediaSize;
  media.vendor_id = kRequestedMediaVendorId;
  return media;
}

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
base::Value::Dict GenerateSampleSystemPrintDialogData(
#if BUILDFLAG(IS_MAC)
    bool include_optional_data
#endif
) {
  base::Value::Dict data;

#if BUILDFLAG(IS_MAC)
  data.Set(kMacSystemPrintDialogDataDestinationType, 4);
  data.Set(kMacSystemPrintDialogDataPageFormat,
           base::Value::BlobStorage({0xA0, 0xA1, 0xA2}));
  data.Set(kMacSystemPrintDialogDataPrintSettings,
           base::Value::BlobStorage({0x00, 0x01}));
  if (include_optional_data) {
    data.Set(kMacSystemPrintDialogDataDestinationFormat, "application/pdf");
    data.Set(kMacSystemPrintDialogDataDestinationLocation, "/foo/bar.pdf");
  }

#elif BUILDFLAG(IS_LINUX)
  data.Set(kLinuxSystemPrintDialogDataPrinter, "printer-name");
  data.Set(kLinuxSystemPrintDialogDataPrintSettings, "print-settings-foo");
  data.Set(kLinuxSystemPrintDialogDataPageSetup, "page-setup-bar");

#else
#error "System print dialog support not implemented for this platform."
#endif

  return data;
}
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)

// Support two possible sample `PrintSettings`, to ensure that certain fields
// get definitively tested for coverage (e.g., booleans).  Note that not all
// fields need distinct values between the two.  A key difference between them
// is custom vs. default margins, as this has significant implications
// internally on printable area.
const PageRanges kPrintSettingsPageRanges{{/*from=*/1, /*to=*/2},
                                          {/*from=*/5, /*to=*/8},
                                          {/*from=*/10, /*to=*/10}};
constexpr char16_t kPrintSettingsTitle[] = u"Title";
constexpr char16_t kPrintSettingsUrl[] = u"//url";
constexpr int kPrintSettingsCopies = 99;
constexpr char16_t kPrintSettingsDeviceName[] = u"device";
const PrintSettings::RequestedMedia kPrintSettingsRequestedMedia{
    /*size_microns=*/gfx::Size(/*width=*/215900, /*height=*/279400),
    /*vendor_id=*/"vendor"};
const PageMargins kPrintSettingsCustomMarginsInPoints(/*header=*/10,
                                                      /*footer=*/15,
                                                      /*left=*/20,
                                                      /*right=*/25,
                                                      /*top=*/30,
                                                      /*bottom=*/35);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
PrintSettings::AdvancedSettings GenerateSampleAdvancedSettings() {
  PrintSettings::AdvancedSettings advanced_settings;
  advanced_settings.emplace("advanced-setting-A", base::Value("setting-A"));
  advanced_settings.emplace("advanced-setting-B", base::Value(42));
  return advanced_settings;
}

const PrintSettings::AdvancedSettings kPrintSettingsAdvancedSettings =
    GenerateSampleAdvancedSettings();
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kPrintSettingsUsername[] = "username";
constexpr char kPrintSettingsPinValue[] = "pin-value";
#endif  // BUILDFLAG(IS_CHROMEOS)

constexpr bool kPrintSettingsSetSelection1 = false;
constexpr bool kPrintSettingsSetSelection2 = true;

constexpr mojom::MarginType kPrintSettingsMarginType1 =
    mojom::MarginType::kDefaultMargins;
constexpr mojom::MarginType kPrintSettingsMarginType2 =
    mojom::MarginType::kCustomMargins;

constexpr bool kPrintSettingsDisplayHeaderFooter1 = true;
constexpr bool kPrintSettingsDisplayHeaderFooter2 = false;

constexpr bool kPrintSettingsShouldPrintBackgrounds1 = false;
constexpr bool kPrintSettingsShouldPrintBackgrounds2 = true;

constexpr bool kPrintSettingsCollate1 = false;
constexpr bool kPrintSettingsCollate2 = true;

constexpr mojom::ColorModel kPrintSettingsColorModel1 =
    mojom::ColorModel::kGrayscale;
constexpr mojom::ColorModel kPrintSettingsColorModel2 = mojom::ColorModel::kRGB;

constexpr mojom::DuplexMode kPrintSettingsDuplexMode1 =
    mojom::DuplexMode::kSimplex;
constexpr mojom::DuplexMode kPrintSettingsDuplexMode2 =
    mojom::DuplexMode::kLongEdge;

constexpr gfx::Size kPrintSettingsDpi1(600, 600);
constexpr gfx::Size kPrintSettingsDpi2(1200, 600);

const char kPrintSettingsMediaTypeEmpty[] = "";
const char kPrintSettingsMediaTypePlain[] = "stationery";

constexpr bool kPrintSettingsBorderless1 = false;
constexpr bool kPrintSettingsBorderless2 = true;

constexpr double kPrintSettingsScaleFactor1 = 1.0;
constexpr double kPrintSettingsScaleFactor2 = 1.25;

constexpr bool kPrintSettingsRasterizePdf1 = true;
constexpr bool kPrintSettingsRasterizePdf2 = false;

constexpr bool kPrintSettingsLandscape1 = false;
constexpr bool kPrintSettingsLandscape2 = true;

#if BUILDFLAG(IS_WIN)
constexpr mojom::PrinterLanguageType kPrintSettingsPrinterLanguageType1 =
    mojom::PrinterLanguageType::kTextOnly;
constexpr mojom::PrinterLanguageType kPrintSettingsPrinterLanguageType2 =
    mojom::PrinterLanguageType::kXps;
#endif  // BUILDFLAG(IS_WIN)

constexpr bool kPrintSettingsModifiable1 = true;
constexpr bool kPrintSettingsModifiable2 = false;

constexpr int kPrintSettingsPagesPerSheet1 = 1;
constexpr int kPrintSettingsPagesPerSheet2 = 2;

#if BUILDFLAG(IS_CHROMEOS)
constexpr bool kPrintSettingsSendUserInfo1 = true;
constexpr bool kPrintSettingsSendUserInfo2 = false;
#endif

struct GenerationParams {
  bool set_printable_area = true;
};

PrintSettings GenerateSamplePrintSettingsCommon() {
  PrintSettings settings;

  settings.set_ranges(kPrintSettingsPageRanges);
  settings.set_title(kPrintSettingsTitle);
  settings.set_url(kPrintSettingsUrl);
  settings.set_copies(kPrintSettingsCopies);
  settings.set_device_name(kPrintSettingsDeviceName);
  settings.set_requested_media(kPrintSettingsRequestedMedia);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  PrintSettings::AdvancedSettings& advanced_settings =
      settings.advanced_settings();
  for (const auto& item : kPrintSettingsAdvancedSettings)
    advanced_settings.emplace(item.first, item.second.Clone());
#endif

#if BUILDFLAG(IS_CHROMEOS)
  settings.set_username(kPrintSettingsUsername);
  settings.set_pin_value(kPrintSettingsPinValue);
#endif

  return settings;
}

PrintSettings GenerateSamplePrintSettingsDefaultMargins() {
  PrintSettings settings = GenerateSamplePrintSettingsCommon();

  settings.set_selection_only(kPrintSettingsSetSelection1);
  settings.set_margin_type(kPrintSettingsMarginType1);
  settings.set_display_header_footer(kPrintSettingsDisplayHeaderFooter1);
  settings.set_should_print_backgrounds(kPrintSettingsShouldPrintBackgrounds1);
  settings.set_collate(kPrintSettingsCollate1);
  settings.set_color(kPrintSettingsColorModel1);
  settings.set_duplex_mode(kPrintSettingsDuplexMode1);
  settings.set_dpi(
      kPrintSettingsDpi1.width());  // Same resolution for both axes.
  settings.set_media_type(kPrintSettingsMediaTypeEmpty);
  settings.set_borderless(kPrintSettingsBorderless1);
  settings.set_scale_factor(kPrintSettingsScaleFactor1);
  settings.set_rasterize_pdf(kPrintSettingsRasterizePdf1);
  settings.SetOrientation(kPrintSettingsLandscape1);

#if BUILDFLAG(IS_WIN)
  settings.set_printer_language_type(kPrintSettingsPrinterLanguageType1);
#endif

  settings.set_is_modifiable(kPrintSettingsModifiable1);
  settings.set_pages_per_sheet(kPrintSettingsPagesPerSheet1);

#if BUILDFLAG(IS_CHROMEOS)
  settings.set_send_user_info(kPrintSettingsSendUserInfo1);
#endif

  return settings;
}

PrintSettings GenerateSamplePrintSettingsCustomMarginsWithParams(
    GenerationParams params) {
  PrintSettings settings = GenerateSamplePrintSettingsCommon();

  settings.set_selection_only(kPrintSettingsSetSelection2);
  settings.set_margin_type(kPrintSettingsMarginType2);
  settings.set_display_header_footer(kPrintSettingsDisplayHeaderFooter2);
  settings.set_should_print_backgrounds(kPrintSettingsShouldPrintBackgrounds2);
  settings.set_collate(kPrintSettingsCollate2);
  settings.set_color(kPrintSettingsColorModel2);
  settings.set_duplex_mode(kPrintSettingsDuplexMode2);
  settings.set_dpi_xy(kPrintSettingsDpi2.width(), kPrintSettingsDpi2.height());
  settings.set_media_type(kPrintSettingsMediaTypePlain);
  settings.set_borderless(kPrintSettingsBorderless2);
  settings.set_scale_factor(kPrintSettingsScaleFactor2);
  settings.set_rasterize_pdf(kPrintSettingsRasterizePdf2);
  settings.SetOrientation(kPrintSettingsLandscape2);

  settings.SetCustomMargins(kPrintSettingsCustomMarginsInPoints);

#if BUILDFLAG(IS_WIN)
  settings.set_printer_language_type(kPrintSettingsPrinterLanguageType2);
#endif

  settings.set_is_modifiable(kPrintSettingsModifiable2);
  settings.set_pages_per_sheet(kPrintSettingsPagesPerSheet2);

#if BUILDFLAG(IS_CHROMEOS)
  settings.set_send_user_info(kPrintSettingsSendUserInfo2);
#endif

  if (params.set_printable_area) {
    settings.SetPrinterPrintableArea(kPageSetupPhysicalSize,
                                     kPageSetupPrintableArea,
                                     /*landscape_needs_flip=*/true);
  }

  return settings;
}

PrintSettings GenerateSamplePrintSettingsCustomMargins() {
  const GenerationParams kParams;
  return GenerateSamplePrintSettingsCustomMarginsWithParams(kParams);
}

bool RequestedMediasEqual(const PrintSettings::RequestedMedia& lhs,
                          const PrintSettings::RequestedMedia& rhs) {
  return lhs.size_microns == rhs.size_microns && lhs.vendor_id == rhs.vendor_id;
}

bool PageMarginsEqual(const PageMargins& lhs, const PageMargins& rhs) {
  return lhs.header == rhs.header && lhs.footer == rhs.footer &&
         lhs.left == rhs.left && lhs.right == rhs.right && lhs.top == rhs.top &&
         lhs.bottom == rhs.bottom;
}

}  // namespace

TEST(PrintingContextMojomTraitsTest, TestSerializeAndDeserializePageMargins) {
  const PageMargins kInput = kPageMarginNonzero;
  PageMargins output;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageMargins>(kInput, output));

  EXPECT_EQ(kPageMarginNonzero.header, output.header);
  EXPECT_EQ(kPageMarginNonzero.footer, output.footer);
  EXPECT_EQ(kPageMarginNonzero.left, output.left);
  EXPECT_EQ(kPageMarginNonzero.right, output.right);
  EXPECT_EQ(kPageMarginNonzero.top, output.top);
  EXPECT_EQ(kPageMarginNonzero.bottom, output.bottom);
}

// Test that no margins is valid.
TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePageMarginsNoMargins) {
  // Default constructor is all members zero.
  PageMargins input;
  // Ensure `output` doesn't start out with all zeroes.
  PageMargins output = kPageMarginNonzero;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageMargins>(input, output));

  EXPECT_EQ(0, output.header);
  EXPECT_EQ(0, output.footer);
  EXPECT_EQ(0, output.left);
  EXPECT_EQ(0, output.right);
  EXPECT_EQ(0, output.top);
  EXPECT_EQ(0, output.bottom);
}

// Test that negative margin values are allowed.
TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePageMarginsNegativeValues) {
  PageMargins input = kPageMarginNonzero;
  PageMargins output;

  input.header = -1;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageMargins>(input, output));
  EXPECT_EQ(-1, output.header);

  input = kPageMarginNonzero;
  input.footer = -1;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageMargins>(input, output));
  EXPECT_EQ(-1, output.footer);

  input = kPageMarginNonzero;
  input.left = -1;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageMargins>(input, output));
  EXPECT_EQ(-1, output.left);

  input = kPageMarginNonzero;
  input.right = -1;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageMargins>(input, output));
  EXPECT_EQ(-1, output.right);

  input = kPageMarginNonzero;
  input.top = -1;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageMargins>(input, output));
  EXPECT_EQ(-1, output.top);

  input = kPageMarginNonzero;
  input.bottom = -1;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageMargins>(input, output));
  EXPECT_EQ(-1, output.bottom);
}

TEST(PrintingContextMojomTraitsTest, TestSerializeAndDeserializePageSetup) {
  const PageSetup kInput = kPageSetupAsymmetricalMargins;
  PageSetup output;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageSetup>(kInput, output));

  EXPECT_EQ(kPageSetupAsymmetricalMargins.physical_size(),
            output.physical_size());
  EXPECT_EQ(kPageSetupAsymmetricalMargins.printable_area(),
            output.printable_area());
  EXPECT_EQ(kPageSetupAsymmetricalMargins.overlay_area(),
            output.overlay_area());
  EXPECT_EQ(kPageSetupAsymmetricalMargins.content_area(),
            output.content_area());
  EXPECT_EQ(kPageSetupAsymmetricalMargins.effective_margins(),
            output.effective_margins());
  EXPECT_EQ(kPageSetupAsymmetricalMargins.requested_margins(),
            output.requested_margins());
  EXPECT_EQ(kPageSetupAsymmetricalMargins.forced_margins(),
            output.forced_margins());
  EXPECT_EQ(kPageSetupAsymmetricalMargins.text_height(), output.text_height());
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePageSetupForcedMargins) {
  const PageSetup kInput = kPageSetupForcedMargins;
  PageSetup output;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageSetup>(kInput, output));

  EXPECT_EQ(kPageSetupForcedMargins.physical_size(), output.physical_size());
  EXPECT_EQ(kPageSetupForcedMargins.printable_area(), output.printable_area());
  EXPECT_EQ(kPageSetupForcedMargins.overlay_area(), output.overlay_area());
  EXPECT_EQ(kPageSetupForcedMargins.content_area(), output.content_area());
  EXPECT_EQ(kPageSetupForcedMargins.effective_margins(),
            output.effective_margins());
  EXPECT_EQ(kPageSetupForcedMargins.requested_margins(),
            output.requested_margins());
  EXPECT_EQ(kPageSetupForcedMargins.forced_margins(), output.forced_margins());
  EXPECT_EQ(kPageSetupForcedMargins.text_height(), output.text_height());
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePageRangeMultiPage) {
  PageRange input;
  PageRange output;

  input.from = 0u;
  input.to = 5u;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageRange>(input, output));

  EXPECT_EQ(0u, output.from);
  EXPECT_EQ(5u, output.to);
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePageRangeSinglePage) {
  PageRange input;
  PageRange output;

  input.from = 1u;
  input.to = 1u;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageRange>(input, output));

  EXPECT_EQ(1u, output.from);
  EXPECT_EQ(1u, output.to);
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePageRangeReverseRange) {
  PageRange input;
  PageRange output;

  // Verify that reverse ranges are not allowed (e.g., not a mechanism to print
  // the range backwards).
  input.from = 5u;
  input.to = 1u;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::PageRange>(input, output));
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializeRequestedMedia) {
  const PrintSettings::RequestedMedia kInput = GenerateSampleRequestedMedia();
  PrintSettings::RequestedMedia output;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::RequestedMedia>(
      kInput, output));

  EXPECT_EQ(kRequestedMediaSize, output.size_microns);
  EXPECT_EQ(kRequestedMediaVendorId, output.vendor_id);
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializeRequestedMediaEmpty) {
  PrintSettings::RequestedMedia input;
  PrintSettings::RequestedMedia output;

  // The default is empty.
  EXPECT_TRUE(input.IsDefault());

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::RequestedMedia>(
      input, output));

  EXPECT_TRUE(output.IsDefault());
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePrintSettingsDefaultMargins) {
  const PrintSettings kInput = GenerateSamplePrintSettingsDefaultMargins();
  PrintSettings output;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(
      kInput, output));

  EXPECT_EQ(output.ranges(), kPrintSettingsPageRanges);
  EXPECT_EQ(output.selection_only(), kPrintSettingsSetSelection1);
  EXPECT_EQ(output.margin_type(), kPrintSettingsMarginType1);
  EXPECT_EQ(output.title(), kPrintSettingsTitle);
  EXPECT_EQ(output.url(), kPrintSettingsUrl);
  EXPECT_EQ(output.display_header_footer(), kPrintSettingsDisplayHeaderFooter1);
  EXPECT_EQ(output.should_print_backgrounds(),
            kPrintSettingsShouldPrintBackgrounds1);
  EXPECT_EQ(output.collate(), kPrintSettingsCollate1);
  EXPECT_EQ(output.color(), kPrintSettingsColorModel1);
  EXPECT_EQ(output.copies(), kPrintSettingsCopies);
  EXPECT_EQ(output.duplex_mode(), kPrintSettingsDuplexMode1);
  EXPECT_EQ(output.device_name(), kPrintSettingsDeviceName);
  EXPECT_TRUE(RequestedMediasEqual(output.requested_media(),
                                   kPrintSettingsRequestedMedia));
  // `page_setup_device_units` is set programmatically by PrintSettings based
  // upon all other parameters, so rely upon the value from the constant input.
  EXPECT_EQ(output.page_setup_device_units(), kInput.page_setup_device_units());
  EXPECT_EQ(output.dpi_size(), kPrintSettingsDpi1);
  EXPECT_EQ(output.media_type(), kPrintSettingsMediaTypeEmpty);
  EXPECT_EQ(output.borderless(), kPrintSettingsBorderless1);
  EXPECT_EQ(output.scale_factor(), kPrintSettingsScaleFactor1);
  EXPECT_EQ(output.rasterize_pdf(), kPrintSettingsRasterizePdf1);
  EXPECT_EQ(output.landscape(), kPrintSettingsLandscape1);

#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(output.printer_language_type(), kPrintSettingsPrinterLanguageType1);
#endif

  EXPECT_EQ(output.is_modifiable(), kPrintSettingsModifiable1);

  // Since `kPrintSettingsMarginType1` is not `kCustomMargins` then expect the
  // custom margins to be default values.
  EXPECT_TRUE(PageMarginsEqual(output.requested_custom_margins_in_points(),
                               PageMargins()));

  EXPECT_EQ(output.pages_per_sheet(), kPrintSettingsPagesPerSheet1);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(output.advanced_settings(), kPrintSettingsAdvancedSettings);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(output.send_user_info(), kPrintSettingsSendUserInfo1);
  EXPECT_EQ(output.username(), kPrintSettingsUsername);
  EXPECT_EQ(output.pin_value(), kPrintSettingsPinValue);
#endif
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePrintSettingsCustomMargins) {
  const PrintSettings kInput = GenerateSamplePrintSettingsCustomMargins();
  PrintSettings output;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(
      kInput, output));

  EXPECT_EQ(output.ranges(), kPrintSettingsPageRanges);
  EXPECT_EQ(output.selection_only(), kPrintSettingsSetSelection2);
  EXPECT_EQ(output.margin_type(), kPrintSettingsMarginType2);
  EXPECT_EQ(output.title(), kPrintSettingsTitle);
  EXPECT_EQ(output.url(), kPrintSettingsUrl);
  EXPECT_EQ(output.display_header_footer(), kPrintSettingsDisplayHeaderFooter2);
  EXPECT_EQ(output.should_print_backgrounds(),
            kPrintSettingsShouldPrintBackgrounds2);
  EXPECT_EQ(output.collate(), kPrintSettingsCollate2);
  EXPECT_EQ(output.color(), kPrintSettingsColorModel2);
  EXPECT_EQ(output.copies(), kPrintSettingsCopies);
  EXPECT_EQ(output.duplex_mode(), kPrintSettingsDuplexMode2);
  EXPECT_EQ(output.device_name(), kPrintSettingsDeviceName);
  EXPECT_TRUE(RequestedMediasEqual(output.requested_media(),
                                   kPrintSettingsRequestedMedia));
  // `page_setup_device_units` is set programmatically by PrintSettings based
  // upon all other parameters, so rely upon the value from the constant input.
  EXPECT_EQ(output.page_setup_device_units(), kInput.page_setup_device_units());
  EXPECT_EQ(output.dpi_size(), kPrintSettingsDpi2);
  EXPECT_EQ(output.media_type(), kPrintSettingsMediaTypePlain);
  EXPECT_EQ(output.borderless(), kPrintSettingsBorderless2);
  EXPECT_EQ(output.scale_factor(), kPrintSettingsScaleFactor2);
  EXPECT_EQ(output.rasterize_pdf(), kPrintSettingsRasterizePdf2);
  EXPECT_EQ(output.landscape(), kPrintSettingsLandscape2);

#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(output.printer_language_type(), kPrintSettingsPrinterLanguageType2);
#endif

  EXPECT_EQ(output.is_modifiable(), kPrintSettingsModifiable2);
  EXPECT_TRUE(PageMarginsEqual(output.requested_custom_margins_in_points(),
                               kPrintSettingsCustomMarginsInPoints));
  EXPECT_EQ(output.pages_per_sheet(), kPrintSettingsPagesPerSheet2);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(output.advanced_settings(), kPrintSettingsAdvancedSettings);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(output.send_user_info(), kPrintSettingsSendUserInfo2);
  EXPECT_EQ(output.username(), kPrintSettingsUsername);
  EXPECT_EQ(output.pin_value(), kPrintSettingsPinValue);
#endif
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePrintSettingsEmptyPageRanges) {
  PrintSettings input = GenerateSamplePrintSettingsDefaultMargins();
  PrintSettings output;

  // Empty page ranges are allowed in settings.
  const PageRanges kEmptyPageRanges;
  input.set_ranges(kEmptyPageRanges);

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(input, output));

  EXPECT_EQ(output.ranges(), kEmptyPageRanges);
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePrintSettingsEmptyPrintableArea) {
  const GenerationParams kParams{.set_printable_area = false};
  const PrintSettings kInput =
      GenerateSamplePrintSettingsCustomMarginsWithParams(kParams);
  PrintSettings output;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(
      kInput, output));

  EXPECT_EQ(output.page_setup_device_units(), kInput.page_setup_device_units());
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePrintSettingsEmptyAdvancedSettings) {
  PrintSettings input = GenerateSamplePrintSettingsDefaultMargins();
  PrintSettings output;

  // Empty advanced settings is allowed.
  input.advanced_settings().clear();

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(input, output));

  EXPECT_TRUE(output.advanced_settings().empty());
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePrintSettingsSystemPrintDialogData) {
  PrintSettings input = GenerateSamplePrintSettingsDefaultMargins();

  // Generate some system print dialog data, including all optional data.
  input.set_system_print_dialog_data(GenerateSampleSystemPrintDialogData(
#if BUILDFLAG(IS_MAC)
      /*include_optional_data=*/true
#endif
      ));

  {
    PrintSettings output;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(
        input, output));

    EXPECT_EQ(output.system_print_dialog_data(),
              input.system_print_dialog_data());
  }

#if BUILDFLAG(IS_MAC)
  // Generate some system print dialog data, excluding optional data
  input.set_system_print_dialog_data(
      GenerateSampleSystemPrintDialogData(/*include_optional_data=*/false));

  {
    PrintSettings output;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(
        input, output));

    EXPECT_EQ(output.system_print_dialog_data(),
              input.system_print_dialog_data());
  }
#endif  // BUILDFLAG(IS_MAC)
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePrintSettingsSystemPrintDialogDataInvalid) {
  PrintSettings input = GenerateSamplePrintSettingsDefaultMargins();

  // Generate some system print dialog data which is invalid.
  base::Value::Dict data;
  data.Set("foo", "bar");
  input.set_system_print_dialog_data(std::move(data));

  PrintSettings output;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(input, output));
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePrintSettingsSystemPrintDialogDataExtraKey) {
  PrintSettings input = GenerateSamplePrintSettingsDefaultMargins();

  // Generate some system print dialog data.
  base::Value::Dict data = GenerateSampleSystemPrintDialogData(
#if BUILDFLAG(IS_MAC)
      /*include_optional_data=*/true
#endif
  );

  // Erroneously include an extra key/value pair.
  data.Set("foo", "bar");
  input.set_system_print_dialog_data(std::move(data));

  PrintSettings output;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(input, output));
}

#if BUILDFLAG(IS_MAC)
TEST(
    PrintingContextMojomTraitsTest,
    TestSerializeAndDeserializePrintSettingsSystemPrintDialogDataDestTypeOutOfRange) {
  PrintSettings input = GenerateSamplePrintSettingsDefaultMargins();

  // Generate some system print dialog data.
  base::Value::Dict data =
      GenerateSampleSystemPrintDialogData(/*include_optional_data=*/false);

  // Override with out-of-range destination type.
  data.Set(kMacSystemPrintDialogDataDestinationType, 0x10000);
  input.set_system_print_dialog_data(std::move(data));

  PrintSettings output;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(input, output));
}

TEST(
    PrintingContextMojomTraitsTest,
    TestSerializeAndDeserializePrintSettingsSystemPrintDialogDataDestTypeInvalidDataType) {
  PrintSettings input = GenerateSamplePrintSettingsDefaultMargins();

  // Generate some system print dialog data.
  base::Value::Dict data =
      GenerateSampleSystemPrintDialogData(/*include_optional_data=*/false);

  // Override with invalid data type for destination type.
  data.Set(kMacSystemPrintDialogDataDestinationType, "supposed to be an int");
  input.set_system_print_dialog_data(std::move(data));

  PrintSettings output;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(input, output));
}

TEST(
    PrintingContextMojomTraitsTest,
    TestSerializeAndDeserializePrintSettingsSystemPrintDialogPageFormatInvalidDataType) {
  PrintSettings input = GenerateSamplePrintSettingsDefaultMargins();

  // Generate some system print dialog data.
  base::Value::Dict data =
      GenerateSampleSystemPrintDialogData(/*include_optional_data=*/false);

  // Override with invalid data type for page format.
  data.Set(kMacSystemPrintDialogDataPageFormat, "supposed to be a BlobStorage");
  input.set_system_print_dialog_data(std::move(data));

  PrintSettings output;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(input, output));
}

TEST(
    PrintingContextMojomTraitsTest,
    TestSerializeAndDeserializePrintSettingsSystemPrintDialogPrintSettingsInvalidDataType) {
  PrintSettings input = GenerateSamplePrintSettingsDefaultMargins();

  // Generate some system print dialog data.
  base::Value::Dict data =
      GenerateSampleSystemPrintDialogData(/*include_optional_data=*/false);

  // Override with invalid data type for print settings.
  data.Set(kMacSystemPrintDialogDataPrintSettings,
           "supposed to be a BlobStorage");
  input.set_system_print_dialog_data(std::move(data));

  PrintSettings output;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(input, output));
}

TEST(
    PrintingContextMojomTraitsTest,
    TestSerializeAndDeserializePrintSettingsSystemPrintDialogDestinationFormatInvalidDataType) {
  PrintSettings input = GenerateSamplePrintSettingsDefaultMargins();

  // Generate some system print dialog data.
  base::Value::Dict data =
      GenerateSampleSystemPrintDialogData(/*include_optional_data=*/false);

  // Override with invalid data type for destination format.
  data.Set(kMacSystemPrintDialogDataPageFormat, 0xBAD);
  input.set_system_print_dialog_data(std::move(data));

  PrintSettings output;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(input, output));
}

TEST(
    PrintingContextMojomTraitsTest,
    TestSerializeAndDeserializePrintSettingsSystemPrintDialogDestinationLocationInvalidDataType) {
  PrintSettings input = GenerateSamplePrintSettingsDefaultMargins();

  // Generate some system print dialog data.
  base::Value::Dict data =
      GenerateSampleSystemPrintDialogData(/*include_optional_data=*/false);

  // Override with invalid data type for destination location.
  data.Set(kMacSystemPrintDialogDataDestinationLocation, 0xBAD);
  input.set_system_print_dialog_data(std::move(data));

  PrintSettings output;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(input, output));
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX)
TEST(
    PrintingContextMojomTraitsTest,
    TestSerializeAndDeserializePrintSettingsSystemPrintDialogPrinterInvalidDataType) {
  PrintSettings input = GenerateSamplePrintSettingsDefaultMargins();

  // Generate some system print dialog data.
  base::Value::Dict data = GenerateSampleSystemPrintDialogData();

  // Override with invalid data type for printer.
  data.Set(kLinuxSystemPrintDialogDataPrinter, 0xBAD);
  input.set_system_print_dialog_data(std::move(data));

  PrintSettings output;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(input, output));
}

TEST(
    PrintingContextMojomTraitsTest,
    TestSerializeAndDeserializePrintSettingsSystemPrintDialogPrintSettingsDataType) {
  PrintSettings input = GenerateSamplePrintSettingsDefaultMargins();

  // Generate some system print dialog data.
  base::Value::Dict data = GenerateSampleSystemPrintDialogData();

  // Override with invalid data type for printer.
  data.Set(kLinuxSystemPrintDialogDataPrintSettings, 0xBAD);
  input.set_system_print_dialog_data(std::move(data));

  PrintSettings output;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(input, output));
}

TEST(
    PrintingContextMojomTraitsTest,
    TestSerializeAndDeserializePrintSettingsSystemPrintDialogPageSetupDataType) {
  PrintSettings input = GenerateSamplePrintSettingsDefaultMargins();

  // Generate some system print dialog data.
  base::Value::Dict data = GenerateSampleSystemPrintDialogData();

  // Override with invalid data type for printer.
  data.Set(kLinuxSystemPrintDialogDataPageSetup, 0xBAD);
  input.set_system_print_dialog_data(std::move(data));

  PrintSettings output;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::PrintSettings>(input, output));
}
#endif  // BUILDFLAG(IS_LINUX)

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)

}  // namespace printing
