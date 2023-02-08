// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_TEST_TEST_CONTENT_BROWSER_CLIENT_H_

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"

namespace content {

// Base for unit tests that need a ContentBrowserClient. Browser tests should
// not use this class, instead use ContentBrowserTestContentBrowserClient.
class TestContentBrowserClient : public ContentBrowserClient {
 public:
  TestContentBrowserClient();

  TestContentBrowserClient(const TestContentBrowserClient&) = delete;
  TestContentBrowserClient& operator=(const TestContentBrowserClient&) = delete;

  ~TestContentBrowserClient() override;

  static TestContentBrowserClient* GetInstance();

  void set_application_locale(const std::string& locale) {
    application_locale_ = locale;
  }

  // ContentBrowserClient:
  base::FilePath GetDefaultDownloadDirectory() override;
  GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      content::BrowserContext* context) override;
  std::string GetUserAgent() override;
  std::string GetApplicationLocale() override;
#if BUILDFLAG(IS_ANDROID)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  // Temporary directory for GetDefaultDownloadDirectory.
  base::ScopedTempDir download_dir_;
  std::string application_locale_;
  static TestContentBrowserClient* instance_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_CONTENT_BROWSER_CLIENT_H_
