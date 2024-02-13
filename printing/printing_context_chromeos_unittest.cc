// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_chromeos.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "printing/backend/cups_ipp_constants.h"
#include "printing/backend/mock_cups_printer.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;

constexpr char kPrinterName[] = "printer";
constexpr char16_t kPrinterName16[] = u"printer";

constexpr char kUsername[] = "test user";

constexpr char kDocumentName[] = "document name";
constexpr char16_t kDocumentName16[] = u"document name";

constexpr gfx::Size kDefaultPaperSize = {215900, 279400};
constexpr char kDefaultPaperName[] = "some_vendor_id";

class MockCupsConnection : public CupsConnection {
 public:
  MOCK_METHOD1(GetDests, bool(std::vector<std::unique_ptr<CupsPrinter>>&));
  MOCK_METHOD2(GetJobs,
               bool(const std::vector<std::string>& printer_ids,
                    std::vector<QueueStatus>* jobs));
  MOCK_METHOD2(GetPrinterStatus,
               bool(const std::string& printer_id,
                    PrinterStatus* printer_status));
  MOCK_CONST_METHOD0(server_name, std::string());
  MOCK_CONST_METHOD0(last_error, int());
  MOCK_CONST_METHOD0(last_error_message, std::string());

  MOCK_METHOD1(GetPrinter,
               std::unique_ptr<CupsPrinter>(const std::string& printer_name));
};

class TestPrintSettings : public PrintSettings {
 public:
  TestPrintSettings() { set_duplex_mode(mojom::DuplexMode::kSimplex); }
};

class PrintingContextTest : public testing::Test,
                            public PrintingContext::Delegate {
 public:
  void SetDefaultSettings(bool send_user_info, const std::string& uri) {
    auto unique_connection = std::make_unique<MockCupsConnection>();
    auto* connection = unique_connection.get();
    auto unique_printer = std::make_unique<NiceMock<MockCupsPrinter>>();
    printer_ = unique_printer.get();
    EXPECT_CALL(*printer_, GetUri()).WillRepeatedly(Return(uri));
    EXPECT_CALL(*connection, GetPrinter(kPrinterName))
        .WillOnce(Return(ByMove(std::move(unique_printer))));
    printing_context_ = PrintingContextChromeos::CreateForTesting(
        this, PrintingContext::ProcessBehavior::kOopDisabled,
        std::move(unique_connection));
    auto settings = std::make_unique<PrintSettings>();
    settings->set_device_name(kPrinterName16);
    settings->set_send_user_info(send_user_info);
    settings->set_duplex_mode(mojom::DuplexMode::kLongEdge);
    settings->set_username(kUsername);
    printing_context_->UpdatePrintSettingsFromPOD(std::move(settings));
    settings_.set_requested_media({kDefaultPaperSize, kDefaultPaperName});
  }

  ipp_attribute_t* GetAttribute(ipp_t* attributes,
                                const char* attr_name) const {
    DCHECK(attr_name);
    ipp_attribute_t* ret = nullptr;
    for (ipp_attribute_t* attr = ippFirstAttribute(attributes); attr;
         attr = ippNextAttribute(attributes)) {
      const char* name = ippGetName(attr);
      if (name && !strcmp(attr_name, name)) {
        EXPECT_EQ(nullptr, ret)
            << "Multiple attributes with name " << attr_name << " found.";
        ret = attr;
      }
    }
    EXPECT_TRUE(ret);
    return ret;
  }

  void TestStringOptionValue(const char* attr_name,
                             const char* expected_value) const {
    auto attributes = SettingsToIPPOptions(settings_, printable_area_);
    auto* attr = GetAttribute(attributes.get(), attr_name);
    EXPECT_STREQ(expected_value, ippGetString(attr, 0, nullptr));
  }

  void TestIntegerOptionValue(const char* attr_name, int expected_value) const {
    auto attributes = SettingsToIPPOptions(settings_, printable_area_);
    auto* attr = GetAttribute(attributes.get(), attr_name);
    EXPECT_EQ(expected_value, ippGetInteger(attr, 0));
  }

  void TestOctetStringOptionValue(const char* attr_name,
                                  base::span<const char> expected_value) const {
    auto attributes = SettingsToIPPOptions(settings_, printable_area_);
    auto* attr = GetAttribute(attributes.get(), attr_name);
    int length;
    void* value = ippGetOctetString(attr, 0, &length);
    ASSERT_EQ(expected_value.size(), static_cast<size_t>(length));
    ASSERT_TRUE(value);
    EXPECT_EQ(0, memcmp(expected_value.data(), value, expected_value.size()));
  }

  void TestResolutionOptionValue(const char* attr_name,
                                 int expected_x_res,
                                 int expected_y_res) const {
    auto attributes = SettingsToIPPOptions(settings_, printable_area_);
    auto* attr = GetAttribute(attributes.get(), attr_name);
    ipp_res_t unit;
    int y_res;
    int x_res = ippGetResolution(attr, 0, &y_res, &unit);
    EXPECT_EQ(unit, IPP_RES_PER_INCH);
    EXPECT_EQ(expected_x_res, x_res);
    EXPECT_EQ(expected_y_res, y_res);
  }

  void TestMediaColValue(const gfx::Size& expected_size,
                         int expected_bottom_margin,
                         int expected_left_margin,
                         int expected_right_margin,
                         int expected_top_margin) {
    auto attributes = SettingsToIPPOptions(settings_, printable_area_);
    ipp_t* media_col =
        ippGetCollection(GetAttribute(attributes.get(), kIppMediaCol), 0);
    ipp_t* media_size =
        ippGetCollection(GetAttribute(media_col, kIppMediaSize), 0);

    int width = ippGetInteger(GetAttribute(media_size, kIppXDimension), 0);
    int height = ippGetInteger(GetAttribute(media_size, kIppYDimension), 0);
    EXPECT_EQ(expected_size.width(), width);
    EXPECT_EQ(expected_size.height(), height);

    int bottom =
        ippGetInteger(GetAttribute(media_col, kIppMediaBottomMargin), 0);
    int left = ippGetInteger(GetAttribute(media_col, kIppMediaLeftMargin), 0);
    int right = ippGetInteger(GetAttribute(media_col, kIppMediaRightMargin), 0);
    int top = ippGetInteger(GetAttribute(media_col, kIppMediaTopMargin), 0);

    EXPECT_EQ(expected_bottom_margin, bottom);
    EXPECT_EQ(expected_left_margin, left);
    EXPECT_EQ(expected_right_margin, right);
    EXPECT_EQ(expected_top_margin, top);
  }

  bool HasAttribute(const char* attr_name) const {
    auto attributes = SettingsToIPPOptions(settings_, printable_area_);
    return !!ippFindAttribute(attributes.get(), attr_name, IPP_TAG_ZERO);
  }

  int GetAttrValueCount(const char* attr_name) const {
    auto attributes = SettingsToIPPOptions(settings_, printable_area_);
    auto* attr = GetAttribute(attributes.get(), attr_name);
    return ippGetCount(attr);
  }

  TestPrintSettings settings_;
  gfx::Rect printable_area_;

  // PrintingContext::Delegate methods.
  gfx::NativeView GetParentView() override { return gfx::NativeView(); }
  std::string GetAppLocale() override { return std::string(); }

  std::unique_ptr<PrintingContextChromeos> printing_context_;
  raw_ptr<MockCupsPrinter> printer_;
};

TEST_F(PrintingContextTest, SettingsToIPPOptions_Color) {
  settings_.set_color(mojom::ColorModel::kGray);
  TestStringOptionValue(kIppColor, "monochrome");
  settings_.set_color(mojom::ColorModel::kColor);
  TestStringOptionValue(kIppColor, "color");
}

TEST_F(PrintingContextTest, SettingsToIPPOptions_Duplex) {
  settings_.set_duplex_mode(mojom::DuplexMode::kSimplex);
  TestStringOptionValue(kIppDuplex, "one-sided");
  settings_.set_duplex_mode(mojom::DuplexMode::kLongEdge);
  TestStringOptionValue(kIppDuplex, "two-sided-long-edge");
  settings_.set_duplex_mode(mojom::DuplexMode::kShortEdge);
  TestStringOptionValue(kIppDuplex, "two-sided-short-edge");
}

TEST_F(PrintingContextTest, SettingsToIPPOptions_MediaCol) {
  settings_.set_requested_media(
      {gfx::Size(297000, 420000), "iso_a3_297x420mm"});
  printable_area_ =
      gfx::Rect(2000, 1000, 297000 - (2000 + 3000), 420000 - (1000 + 4000));
  TestMediaColValue(gfx::Size(29700, 42000), 100, 200, 300, 400);
}

TEST_F(PrintingContextTest, SettingsToIPPOptionsMediaColLandscape) {
  settings_.set_requested_media(
      {gfx::Size(148000, 200000), "om_200030x148170um_200x148mm"});
  // Use margins (LBRT) of 500, 700, 200, and 1000.
  printable_area_ =
      gfx::Rect(500, 700, 148000 - (500 + 200), 200000 - (700 + 1000));
  // The requested media and printable area is in portrait mode (height larger
  // than width).  Since the vendor ID has a width larger than the height, the
  // expected media should get swapped.  When swapped, the margins (LBRT) should
  // be 1000, 500, 700, and 200.
  TestMediaColValue(gfx::Size(20000, 14800), 50, 100, 70, 20);
}

TEST_F(PrintingContextTest, SettingsToIPPOptions_Copies) {
  settings_.set_copies(3);
  TestIntegerOptionValue(kIppCopies, 3);
}

TEST_F(PrintingContextTest, SettingsToIPPOptions_Collate) {
  TestStringOptionValue(kIppCollate, "separate-documents-uncollated-copies");
  settings_.set_collate(true);
  TestStringOptionValue(kIppCollate, "separate-documents-collated-copies");
}

TEST_F(PrintingContextTest, SettingsToIPPOptions_Pin) {
  EXPECT_FALSE(HasAttribute(kIppPin));
  settings_.set_pin_value("1234");
  TestOctetStringOptionValue(kIppPin, base::make_span("1234", 4u));
}

TEST_F(PrintingContextTest, SettingsToIPPOptions_Resolution) {
  EXPECT_FALSE(HasAttribute(kIppResolution));
  settings_.set_dpi_xy(0, 300);
  EXPECT_FALSE(HasAttribute(kIppResolution));
  settings_.set_dpi_xy(300, 0);
  EXPECT_FALSE(HasAttribute(kIppResolution));
  settings_.set_dpi(600);
  TestResolutionOptionValue(kIppResolution, 600, 600);
  settings_.set_dpi_xy(600, 1200);
  TestResolutionOptionValue(kIppResolution, 600, 1200);
}

TEST_F(PrintingContextTest, SettingsToIPPOptions_SendUserInfo_Secure) {
  ipp_status_t status = ipp_status_t::IPP_STATUS_OK;
  std::u16string document_name = kDocumentName16;
  SetDefaultSettings(/*send_user_info=*/true, "ipps://test-uri");
  std::string create_job_document_name;
  std::string create_job_username;
  std::string start_document_document_name;
  std::string start_document_username;
  EXPECT_CALL(*printer_, CreateJob)
      .WillOnce(DoAll(SetArgPointee<0>(/*job_id=*/1),
                      SaveArg<1>(&create_job_document_name),
                      SaveArg<2>(&create_job_username), Return(status)));
  EXPECT_CALL(*printer_, StartDocument)
      .WillOnce(DoAll(SaveArg<1>(&start_document_document_name),
                      SaveArg<3>(&start_document_username), Return(true)));

  printing_context_->NewDocument(document_name);

  EXPECT_EQ(create_job_document_name, kDocumentName);
  EXPECT_EQ(start_document_document_name, kDocumentName);
  EXPECT_EQ(create_job_username, kUsername);
  EXPECT_EQ(start_document_username, kUsername);
}

TEST_F(PrintingContextTest, SettingsToIPPOptions_SendUserInfo_Insecure) {
  ipp_status_t status = ipp_status_t::IPP_STATUS_OK;
  std::u16string document_name = kDocumentName16;
  std::string default_username = "chronos";
  std::string default_document_name = "-";
  SetDefaultSettings(/*send_user_info=*/true, "ipp://test-uri");
  std::string create_job_document_name;
  std::string create_job_username;
  std::string start_document_document_name;
  std::string start_document_username;
  EXPECT_CALL(*printer_, CreateJob)
      .WillOnce(DoAll(SetArgPointee<0>(/*job_id=*/1),
                      SaveArg<1>(&create_job_document_name),
                      SaveArg<2>(&create_job_username), Return(status)));
  EXPECT_CALL(*printer_, StartDocument)
      .WillOnce(DoAll(SaveArg<1>(&start_document_document_name),
                      SaveArg<3>(&start_document_username), Return(true)));

  printing_context_->NewDocument(document_name);

  EXPECT_EQ(create_job_document_name, default_document_name);
  EXPECT_EQ(start_document_document_name, default_document_name);
  EXPECT_EQ(create_job_username, default_username);
  EXPECT_EQ(start_document_username, default_username);
}

TEST_F(PrintingContextTest, SettingsToIPPOptions_DoNotSendUserInfo) {
  ipp_status_t status = ipp_status_t::IPP_STATUS_OK;
  std::u16string document_name = kDocumentName16;
  SetDefaultSettings(/*send_user_info=*/false, "ipps://test-uri");
  std::string create_job_document_name;
  std::string create_job_username;
  std::string start_document_document_name;
  std::string start_document_username;
  EXPECT_CALL(*printer_, CreateJob)
      .WillOnce(DoAll(SetArgPointee<0>(/*job_id=*/1),
                      SaveArg<1>(&create_job_document_name),
                      SaveArg<2>(&create_job_username), Return(status)));
  EXPECT_CALL(*printer_, StartDocument)
      .WillOnce(DoAll(SaveArg<1>(&start_document_document_name),
                      SaveArg<3>(&start_document_username), Return(true)));

  printing_context_->NewDocument(document_name);

  EXPECT_EQ(create_job_document_name, "");
  EXPECT_EQ(start_document_document_name, "");
  EXPECT_EQ(create_job_username, "");
  EXPECT_EQ(start_document_username, "");
}

TEST_F(PrintingContextTest, SettingsToIPPOptionsClientInfo) {
  mojom::IppClientInfo client_info(
      mojom::IppClientInfo::ClientType::kOperatingSystem, "a-", "B_", "1.",
      "a.1-B_");
  settings_.set_client_infos({client_info});

  auto attributes = SettingsToIPPOptions(settings_, printable_area_);
  auto* attr = ippFindAttribute(attributes.get(), kIppClientInfo,
                                IPP_TAG_BEGIN_COLLECTION);
  auto* client_info_collection = ippGetCollection(attr, 0);

  attr = ippFindAttribute(client_info_collection, kIppClientName, IPP_TAG_NAME);
  EXPECT_STREQ("a-", ippGetString(attr, 0, nullptr));

  attr = ippFindAttribute(client_info_collection, kIppClientType, IPP_TAG_ENUM);
  EXPECT_EQ(4, ippGetInteger(attr, 0));

  attr =
      ippFindAttribute(client_info_collection, kIppClientPatches, IPP_TAG_TEXT);
  EXPECT_STREQ("B_", ippGetString(attr, 0, nullptr));

  attr = ippFindAttribute(client_info_collection, kIppClientStringVersion,
                          IPP_TAG_TEXT);
  EXPECT_STREQ("1.", ippGetString(attr, 0, nullptr));

  attr = ippFindAttribute(client_info_collection, kIppClientVersion,
                          IPP_TAG_STRING);
  int length;
  void* version = ippGetOctetString(attr, 0, &length);
  ASSERT_TRUE(version);
  EXPECT_EQ(6, length);
  EXPECT_EQ(0, memcmp("a.1-B_", version, 6));
}

TEST_F(PrintingContextTest, SettingsToIPPOptionsClientInfoSomeValid) {
  mojom::IppClientInfo valid_client_info(
      mojom::IppClientInfo::ClientType::kOperatingSystem, "aB.1-_", "aB.1-_",
      "aB.1-_", "aB.1-_");
  mojom::IppClientInfo invalid_client_info(
      mojom::IppClientInfo::ClientType::kOperatingSystem, "{}", "aB.1-_",
      "aB.1-_", "aB.1-_");
  settings_.set_client_infos(
      {valid_client_info, invalid_client_info, valid_client_info});

  // Check that the invalid item is skipped in the client-info collection.
  EXPECT_EQ(GetAttrValueCount(kIppClientInfo), 2);
}

TEST_F(PrintingContextTest, SettingsToIPPOptionsClientInfoEmpty) {
  settings_.set_client_infos({});
  EXPECT_FALSE(HasAttribute(kIppClientInfo));

  mojom::IppClientInfo invalid_client_info(
      mojom::IppClientInfo::ClientType::kOther, "$", " ", "{}", std::nullopt);

  settings_.set_client_infos({invalid_client_info});
  EXPECT_FALSE(HasAttribute(kIppClientInfo));
}

}  // namespace

}  // namespace printing
