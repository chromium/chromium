// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_pdf_browsertest.h"

#include <optional>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "components/headless/test/pdf_utils.h"
#include "content/public/test/browser_test.h"
#include "headless/test/headless_browser_test_utils.h"
#include "pdf/pdf.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace headless {

void HeadlessPDFBrowserTestBase::RunDevTooledTest() {
  ASSERT_TRUE(embedded_test_server()->Start());

  devtools_client_.AddEventHandler(
      "Page.loadEventFired",
      base::BindRepeating(&HeadlessPDFBrowserTestBase::OnLoadEventFired,
                          base::Unretained(this)));
  SendCommandSync(devtools_client_, "Page.enable");

  std::string test_path = "/" + GetTestPath();
  GURL url = embedded_test_server()->GetURL(test_path);
  devtools_client_.SendCommand("Page.navigate", Param("url", url.spec()));
}

void HeadlessPDFBrowserTestBase::OnLoadEventFired(const base::DictValue&) {
  devtools_client_.SendCommand(
      "Page.printToPDF", GetPrintToPDFParams(),
      base::BindOnce(&HeadlessPDFBrowserTestBase::OnPDFCreated,
                     base::Unretained(this)));
}

void HeadlessPDFBrowserTestBase::OnPDFCreated(base::DictValue result) {
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

base::DictValue HeadlessPDFBrowserTestBase::GetPrintToPDFParams() {
  base::DictValue params;
  params.Set("printBackground", true);
  params.Set("paperHeight", 41);
  params.Set("paperWidth", 41);
  params.Set("marginTop", 0);
  params.Set("marginBottom", 0);
  params.Set("marginLeft", 0);
  params.Set("marginRight", 0);

  return params;
}

void HeadlessPDFBrowserTestBase::OnPDFFailure(int code,
                                              const std::string& message) {
  ADD_FAILURE() << "code=" << code << " message: " << message;
}

}  // namespace headless
