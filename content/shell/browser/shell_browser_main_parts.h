// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
#define CONTENT_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/main_function_params.h"
#include "content/shell/browser/shell_browser_context.h"
#include "ui/base/buildflags.h"

namespace performance_manager {
class PerformanceManagerLifetime;
}  // namespace performance_manager

#if BUILDFLAG(USE_GTK)
namespace ui {
class GtkUiDelegate;
}
#endif

namespace content {
class ShellPlatformDelegate;

class ShellBrowserMainParts : public BrowserMainParts {
 public:
  explicit ShellBrowserMainParts(const MainFunctionParams& parameters);
  ~ShellBrowserMainParts() override;

  // BrowserMainParts overrides.
  int PreEarlyInitialization() override;
  int PreCreateThreads() override;
  void PostCreateThreads() override;
  void PreMainMessageLoopStart() override;
  void PostMainMessageLoopStart() override;
  void ToolkitInitialized() override;
  void PreMainMessageLoopRun() override;
  bool MainMessageLoopRun(int* result_code) override;
  void PreDefaultMainMessageLoopRun(base::OnceClosure quit_closure) override;
  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

  ShellBrowserContext* browser_context() { return browser_context_.get(); }
  ShellBrowserContext* off_the_record_browser_context() {
    return off_the_record_browser_context_.get();
  }

 protected:
  virtual void InitializeBrowserContexts();
  virtual void InitializeMessageLoopContext();
  // Gets the ShellPlatformDelegate to be used. May be a subclass of
  // ShellPlatformDelegate to change behaviour based on platform or for tests.
  virtual std::unique_ptr<ShellPlatformDelegate> CreateShellPlatformDelegate();

  void set_browser_context(ShellBrowserContext* context) {
    browser_context_.reset(context);
  }
  void set_off_the_record_browser_context(ShellBrowserContext* context) {
    off_the_record_browser_context_.reset(context);
  }

 private:

  std::unique_ptr<ShellBrowserContext> browser_context_;
  std::unique_ptr<ShellBrowserContext> off_the_record_browser_context_;

  // For running content_browsertests.
  const MainFunctionParams parameters_;
  bool run_message_loop_;

#if BUILDFLAG(USE_GTK)
  std::unique_ptr<ui::GtkUiDelegate> gtk_ui_delegate_;
#endif

  std::unique_ptr<performance_manager::PerformanceManagerLifetime>
      performance_manager_lifetime_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserMainParts);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
