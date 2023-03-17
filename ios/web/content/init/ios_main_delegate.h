// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_INIT_IOS_MAIN_DELEGATE_H_
#define IOS_WEB_CONTENT_INIT_IOS_MAIN_DELEGATE_H_

#import "build/blink_buildflags.h"
#import "content/public/app/content_main_delegate.h"
#import "content/public/browser/browser_main_runner.h"
#import "content/public/browser/content_browser_client.h"
#import "content/public/common/content_client.h"
#import "content/public/renderer/content_renderer_client.h"

#if !BUILDFLAG(USE_BLINK)
#error File can only be included when USE_BLINK is true
#endif

namespace content {
class BrowserMainRunner;
}

namespace web {

class IOSMainDelegate : public content::ContentMainDelegate {
 public:
  IOSMainDelegate();
  IOSMainDelegate(const IOSMainDelegate&) = delete;
  IOSMainDelegate& operator=(const IOSMainDelegate&) = delete;
  ~IOSMainDelegate() override;

  // ContentMainDelegate implementation:
  content::ContentClient* CreateContentClient() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;

  absl::variant<int, content::MainFunctionParams> RunProcess(
      const std::string& process_type,
      content::MainFunctionParams main_function_params) override;

 private:
  std::unique_ptr<content::BrowserMainRunner> browser_runner_;
  std::unique_ptr<content::ContentClient> content_client_;
  std::unique_ptr<content::ContentBrowserClient> browser_client_;
  std::unique_ptr<content::ContentRendererClient> renderer_client_;
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_INIT_IOS_MAIN_DELEGATE_H_
