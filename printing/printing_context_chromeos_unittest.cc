// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_chromeos.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "printing/backend/cups_ipp_constants.h"
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

class MockCupsPrinter : public CupsPrinter {
 public:
  MOCK_CONST_METHOD0(is_default, bool());
  MOCK_CONST_METHOD0(GetName, std::string());
  MOCK_CONST_METHOD0(GetMakeAndModel, std::string());
  MOCK_CONST_METHOD0(GetInfo, std::string());
  MOCK_CONST_METHOD0(GetUri, std::string());
  MOCK_CONST_METHOD0(EnsureDestInfo, bool());
  MOCK_CONST_METHOD1(ToPrinterInfo, bool(PrinterBasicInfo* basic_info));
  MOCK_METHOD4(CreateJob,
               ipp_status_t(int* job_id,
                            const std::string& title,
                            const std::string& username,
                            const std::vector<cups_option_t>& options));
  MOCK_METHOD5(StartDocument,
               bool(int job_id,
                    const std::string& docname,
                    bool last_doc,
                    const std::string& username,
                    const std::vector<cups_option_t>& options));
  MOCK_METHOD1(StreamData, bool(const std::vector<char>& buffer));
  MOCK_METHOD0(FinishDocument, bool());
  MOCK_METHOD2(CloseJob, ipp_status_t(int job_id, const std::string& username));
  MOCK_METHOD1(CancelJob, bool(int job_id));
  MOCK_METHOD1(GetMediaMarginsByName,
               CupsMediaMargins(const std::string& media_id));

  MOCK_CONST_METHOD1(GetSupportedOptionValues,
                     ipp_attribute_t*(const char* option_name));
  MOCK_CONST_METHOD1(GetSupportedOptionValueStrings,
                     std::vector<base::StringPiece>(const char* option_name));
  MOCK_CONST_METHOD1(GetDefaultOptionValue,
                     ipp_attribute_t*(const char* option_name));
  MOCK_CONST_METHOD2(CheckOptionSupported,
                     bool(const char* name, const char* value));
};

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
        this, std::move(unique_connection));
    auto settings = std::make_unique<PrintSettings>();
    settings->set_device_name(kPrinterName16);
    settings->set_send_user_info(send_user_info);
    settings->set_duplex_mode(mojom::DuplexMode::kLongEdge);
    settings->set_username(kUsername);
    printing_context_->UpdatePrintSettingsFromPOD(std::move(settings));
  }

  void TestCupsOptionValue(const char* option_name,
                           const char* expected_option_value) const {
    DCHECK(option_name);
    auto cups_options = SettingsToCupsOptions(settings_);
    const char* ret = nullptr;
    for (const auto& option : cups_options) {
      EXPECT_TRUE(option->name);
      EXPECT_TRUE(option->value);
      if (option->name && !strcmp(option_name, option->name)) {
        EXPECT_EQ(nullptr, ret)
            << "Multiple options with name " << option_name << " found.";
        ret = option->value;
      }
    }
    EXPECT_STREQ(expected_option_value, ret);
  }

  TestPrintSettings settings_;

  // PrintingContext::Delegate methods.
  gfx::NativeView GetParentView() override { return nullptr; }
  std::string GetAppLocale() override { return std::string(); }

  std::unique_ptr<PrintingContextChromeos> printing_context_;
  raw_ptr<MockCupsPrinter> printer_;
};

TEST_F(PrintingContextTest, SettingsToCupsOptions_Color) {
  settings_.set_color(mojom::ColorModel::kGray);
  TestCupsOptionValue(kIppColor, "monochrome");
  settings_.set_color(mojom::ColorModel::kColor);
  TestCupsOptionValue(kIppColor, "color");
}

TEST_F(PrintingContextTest, SettingsToCupsOptions_Duplex) {
  settings_.set_duplex_mode(mojom::DuplexMode::kSimplex);
  TestCupsOptionValue(kIppDuplex, "one-sided");
  settings_.set_duplex_mode(mojom::DuplexMode::kLongEdge);
  TestCupsOptionValue(kIppDuplex, "two-sided-long-edge");
  settings_.set_duplex_mode(mojom::DuplexMode::kShortEdge);
  TestCupsOptionValue(kIppDuplex, "two-sided-short-edge");
}

TEST_F(PrintingContextTest, SettingsToCupsOptions_Media) {
  TestCupsOptionValue(kIppMedia, "");
  settings_.set_requested_media(
      {gfx::Size(297000, 420000), "iso_a3_297x420mm"});
  TestCupsOptionValue(kIppMedia, "iso_a3_297x420mm");
}

TEST_F(PrintingContextTest, SettingsToCupsOptions_Copies) {
  settings_.set_copies(3);
  TestCupsOptionValue(kIppCopies, "3");
}

TEST_F(PrintingContextTest, SettingsToCupsOptions_Collate) {
  TestCupsOptionValue(kIppCollate, "separate-documents-uncollated-copies");
  settings_.set_collate(true);
  TestCupsOptionValue(kIppCollate, "separate-documents-collated-copies");
}

TEST_F(PrintingContextTest, SettingsToCupsOptions_Pin) {
  TestCupsOptionValue(kIppPin, nullptr);
  settings_.set_pin_value("1234");
  TestCupsOptionValue(kIppPin, "1234");
}

TEST_F(PrintingContextTest, SettingsToCupsOptions_Resolution) {
  TestCupsOptionValue(kIppResolution, nullptr);
  settings_.set_dpi_xy(0, 300);
  TestCupsOptionValue(kIppResolution, nullptr);
  settings_.set_dpi_xy(300, 0);
  TestCupsOptionValue(kIppResolution, nullptr);
  settings_.set_dpi(600);
  TestCupsOptionValue(kIppResolution, "600dpi");
  settings_.set_dpi_xy(600, 1200);
  TestCupsOptionValue(kIppResolution, "600x1200dpi");
}

TEST_F(PrintingContextTest, SettingsToCupsOptions_SendUserInfo_Secure) {
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

TEST_F(PrintingContextTest, SettingsToCupsOptions_SendUserInfo_Insecure) {
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

TEST_F(PrintingContextTest, SettingsToCupsOptions_DoNotSendUserInfo) {
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

}  // namespace

}  // namespace printing
