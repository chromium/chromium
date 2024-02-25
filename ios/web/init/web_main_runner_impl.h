// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_INIT_WEB_MAIN_RUNNER_IMPL_H_
#define IOS_WEB_INIT_WEB_MAIN_RUNNER_IMPL_H_

#import "ios/web/public/init/web_main_runner.h"

#import "base/memory/raw_ptr.h"
#import "ios/web/public/web_client.h"

namespace web {

class WebClient;
class WebMainLoop;

class WebMainRunnerImpl : public WebMainRunner {
 public:
  WebMainRunnerImpl();
  WebMainRunnerImpl(const WebMainRunnerImpl&) = delete;
  WebMainRunnerImpl& operator=(const WebMainRunnerImpl&) = delete;
  ~WebMainRunnerImpl() override;

  // WebMainRunner implementation:
  int Initialize(WebMainParams params) override;
  void ShutDown() override;

 protected:
  // True if we have started to initialize the runner.
  bool is_initialized_;

  // True if the runner has been shut down.
  bool is_shutdown_;

  // True if basic startup was completed.
  bool completed_basic_startup_;

  // The delegate will outlive this object.
  raw_ptr<WebMainDelegate> delegate_;

  // Used if the embedder doesn't set one.
  WebClient empty_web_client_;

  std::unique_ptr<WebMainLoop> main_loop_;
};

}  // namespace web

#endif  // IOS_WEB_INIT_WEB_MAIN_RUNNER_IMPL_H_
