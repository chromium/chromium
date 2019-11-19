// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_HEADLESS_CONTENT_MAIN_DELEGATE_H_
#define HEADLESS_LIB_HEADLESS_CONTENT_MAIN_DELEGATE_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "headless/lib/headless_content_client.h"
#include "headless/public/headless_export.h"

#if !defined(CHROME_MULTIPLE_DLL_CHILD)
#include "headless/lib/browser/headless_platform_event_source.h"
#endif

namespace base {
namespace debug {
struct CrashKeyString;
}  // namespace debug
class CommandLine;
}  // namespace base

namespace headless {

class HeadlessBrowserImpl;

// Exported for tests.
class HEADLESS_EXPORT HeadlessContentMainDelegate
    : public content::ContentMainDelegate {
 public:
  explicit HeadlessContentMainDelegate(
      std::unique_ptr<HeadlessBrowserImpl> browser);
  explicit HeadlessContentMainDelegate(HeadlessBrowser::Options options);
  ~HeadlessContentMainDelegate() override;

  // content::ContentMainDelegate implementation:
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
#if defined(OS_MACOSX)
  void PreCreateMainMessageLoop() override;
#endif
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;

  void PostEarlyInitialization(bool is_running_tests) override;

  HeadlessBrowserImpl* browser() const { return browser_.get(); }

#if defined(OS_LINUX)
  void ZygoteForked() override;
#endif

 private:
  friend class HeadlessBrowserTest;

  void Init();

  HeadlessBrowser::Options* options();

  static void InitializeResourceBundle();
  static HeadlessContentMainDelegate* GetInstance();

  void InitLogging(const base::CommandLine& command_line);
  void InitCrashReporter(const base::CommandLine& command_line);

  std::unique_ptr<content::ContentRendererClient> renderer_client_;
  std::unique_ptr<content::ContentBrowserClient> browser_client_;
  std::unique_ptr<content::ContentUtilityClient> utility_client_;
  HeadlessContentClient content_client_;
#if !defined(CHROME_MULTIPLE_DLL_CHILD)
  HeadlessPlatformEventSource platform_event_source_;
#endif

  std::unique_ptr<HeadlessBrowserImpl> browser_;
  std::unique_ptr<HeadlessBrowser::Options> options_;

  base::debug::CrashKeyString* headless_crash_key_;  // Note: never deallocated.

  DISALLOW_COPY_AND_ASSIGN(HeadlessContentMainDelegate);
};

}  // namespace headless

#endif  // HEADLESS_LIB_HEADLESS_CONTENT_MAIN_DELEGATE_H_
