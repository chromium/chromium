// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SYSTEM_LOGS_SHELL_SYSTEM_LOGS_FETCHER_H_
#define EXTENSIONS_SHELL_BROWSER_SYSTEM_LOGS_SHELL_SYSTEM_LOGS_FETCHER_H_

namespace content {
class BrowserContext;
}  // namespace content

namespace system_logs {

class SystemLogsFetcher;

// Creates a SystemLogsFetcher to aggregate the scrubbed logs for sending with
// feedback reports. The fetcher deletes itself once it finishes fetching data.
SystemLogsFetcher* BuildShellSystemLogsFetcher(
    content::BrowserContext* browser_context);

}  // namespace system_logs

#endif  // EXTENSIONS_SHELL_BROWSER_SYSTEM_LOGS_SHELL_SYSTEM_LOGS_FETCHER_H_
