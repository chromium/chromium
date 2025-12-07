// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_INIT_IOS_CONTENT_MAIN_RUNNER_H_
#define IOS_WEB_CONTENT_INIT_IOS_CONTENT_MAIN_RUNNER_H_

#import <string>
#import <vector>

#import "ios/web/public/init/web_main_runner.h"

namespace content {
class ContentMainRunner;
class ContentMainDelegate;
}  // namespace content

namespace web {

// This class is responsible for content initialization and shutdown.
class IOSContentMainRunner : public WebMainRunner {
 public:
  IOSContentMainRunner();
  IOSContentMainRunner(const IOSContentMainRunner&) = delete;
  IOSContentMainRunner& operator=(const IOSContentMainRunner&) = delete;
  ~IOSContentMainRunner() override;

  // WebMainRunner implementation:
  void Initialize(WebMainParams params) override;
  int Startup() override;
  void ShutDown() override;

 private:
  std::unique_ptr<content::ContentMainDelegate> content_main_delegate_;
  std::vector<std::string> argv_;
  std::unique_ptr<content::ContentMainRunner> content_main_runner_;
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_INIT_IOS_CONTENT_MAIN_RUNNER_H_
