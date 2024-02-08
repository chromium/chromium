// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_MOCK_CUPS_PRINTER_H_
#define PRINTING_BACKEND_MOCK_CUPS_PRINTER_H_

#include <string_view>

#include "printing/backend/cups_printer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace printing {

// A mock `CupsPrinter` used for testing.
class MockCupsPrinter : public CupsPrinter {
 public:
  MockCupsPrinter();
  ~MockCupsPrinter() override;

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
                            ipp_t* attributes));
  MOCK_METHOD5(StartDocument,
               bool(int job_id,
                    const std::string& docname,
                    bool last_doc,
                    const std::string& username,
                    ipp_t* attributes));
  MOCK_METHOD1(StreamData, bool(const std::vector<char>& buffer));
  MOCK_METHOD0(FinishDocument, bool());
  MOCK_METHOD2(CloseJob, ipp_status_t(int job_id, const std::string& username));
  MOCK_METHOD1(CancelJob, bool(int job_id));

  MOCK_CONST_METHOD1(GetSupportedOptionValues,
                     ipp_attribute_t*(const char* option_name));
  MOCK_CONST_METHOD1(GetSupportedOptionValueStrings,
                     std::vector<std::string_view>(const char* option_name));
  MOCK_CONST_METHOD0(GetMediaColDatabase, ipp_attribute_t*());
  MOCK_CONST_METHOD1(GetDefaultOptionValue,
                     ipp_attribute_t*(const char* option_name));
  MOCK_CONST_METHOD2(CheckOptionSupported,
                     bool(const char* name, const char* value));
  MOCK_CONST_METHOD2(GetLocalizedOptionValueName,
                     const char*(const char* option_name, const char* value));
};

}  // namespace printing

#endif  // PRINTING_BACKEND_MOCK_CUPS_PRINTER_H_
