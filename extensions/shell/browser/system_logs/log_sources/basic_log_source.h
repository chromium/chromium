// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SYSTEM_LOGS_LOG_SOURCES_BASIC_LOG_SOURCE_H_
#define EXTENSIONS_SHELL_BROWSER_SYSTEM_LOGS_LOG_SOURCES_BASIC_LOG_SOURCE_H_

#include "base/macros.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace system_logs {

// Fetches internal logs.
class BasicLogSource : public SystemLogsSource {
 public:
  explicit BasicLogSource(content::BrowserContext* browser_context);
  ~BasicLogSource() override;

  // SystemLogsSource:
  void Fetch(SysLogsSourceCallback request) override;

 private:
  void PopulateVersionStrings(SystemLogsResponse* response);
  void PopulateExtensionInfoLogs(SystemLogsResponse* response);

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(BasicLogSource);
};

}  // namespace system_logs

#endif  // EXTENSIONS_SHELL_BROWSER_SYSTEM_LOGS_LOG_SOURCES_BASIC_LOG_SOURCE_H_
