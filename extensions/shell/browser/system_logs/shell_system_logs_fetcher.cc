// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/system_logs/shell_system_logs_fetcher.h"

#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "extensions/shell/browser/system_logs/log_sources/basic_log_source.h"

namespace system_logs {

SystemLogsFetcher* BuildShellSystemLogsFetcher(
    content::BrowserContext* browser_context) {
  // Deletes itself after Fetch() is completes.
  SystemLogsFetcher* fetcher =
      new SystemLogsFetcher(/* scrub_data= */ true,
                            /* first_party_extension_ids= */ nullptr);
  fetcher->AddSource(std::make_unique<BasicLogSource>(browser_context));
  return fetcher;
}

}  // namespace system_logs
