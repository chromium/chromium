// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/shell_browser_state.h"

#import "base/base_paths.h"
#import "base/files/file_path.h"
#import "base/path_service.h"
#import "base/threading/thread_restrictions.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/shell/shell_url_request_context_getter.h"

namespace web {

ShellBrowserState::ShellBrowserState() : BrowserState() {
  CHECK(base::PathService::Get(base::DIR_APP_DATA, &path_));

  request_context_getter_ = new ShellURLRequestContextGetter(
      GetStatePath(), this, web::GetIOThreadTaskRunner({}));
}

ShellBrowserState::~ShellBrowserState() {
}

bool ShellBrowserState::IsOffTheRecord() const {
  return false;
}

base::FilePath ShellBrowserState::GetStatePath() const {
  return path_;
}

net::URLRequestContextGetter* ShellBrowserState::GetRequestContext() {
  return request_context_getter_.get();
}

}  // namespace web
