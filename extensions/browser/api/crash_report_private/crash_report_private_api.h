// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CRASH_REPORT_PRIVATE_CRASH_REPORT_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_CRASH_REPORT_PRIVATE_CRASH_REPORT_PRIVATE_API_H_

#include <string>

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/common/api/crash_report_private.h"

namespace base {
class Clock;
}

namespace extensions {
namespace api {

class CrashReportPrivateReportErrorFunction : public ExtensionFunction {
 public:
  CrashReportPrivateReportErrorFunction();
  DECLARE_EXTENSION_FUNCTION("crashReportPrivate.reportError",
                             CRASHREPORTPRIVATE_REPORTERROR)

 protected:
  ~CrashReportPrivateReportErrorFunction() override;
  ResponseAction Run() override;

 private:
  void OnReportComplete();

  DISALLOW_COPY_AND_ASSIGN(CrashReportPrivateReportErrorFunction);
};

void SetClockForTesting(base::Clock* clock);

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CRASH_REPORT_PRIVATE_CRASH_REPORT_PRIVATE_API_H_
