// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_TEST_HEADLESS_PDF_BROWSERTEST_H_
#define HEADLESS_TEST_HEADLESS_PDF_BROWSERTEST_H_

#include <string>

#include "base/containers/span.h"
#include "base/values.h"
#include "headless/test/headless_devtooled_browsertest.h"

namespace headless {

class HeadlessPDFBrowserTestBase : public HeadlessDevTooledBrowserTest {
 public:
  void RunDevTooledTest() override;

 protected:
  virtual std::string GetTestPath() = 0;
  virtual base::DictValue GetPrintToPDFParams();
  virtual void OnPDFReady(base::span<const uint8_t> pdf_span,
                          int num_pages) = 0;
  virtual void OnPDFFailure(int code, const std::string& message);

 private:
  void OnLoadEventFired(const base::DictValue&);
  void OnPDFCreated(base::DictValue result);
};

}  // namespace headless

#endif  // HEADLESS_TEST_HEADLESS_PDF_BROWSERTEST_H_
