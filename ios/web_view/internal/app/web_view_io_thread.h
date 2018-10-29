// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_APP_WEB_VIEW_IO_THREAD_H_
#define IOS_WEB_VIEW_INTERNAL_APP_WEB_VIEW_IO_THREAD_H_

#include <memory>

#include "ios/components/io_thread/ios_io_thread.h"

class PrefService;

namespace net {
class NetLog;
}  // namespace net

namespace ios_web_view {

// Contains state associated with, initialized and cleaned up on, and
// primarily used on, the IO thread.
class WebViewIOThread : public io_thread::IOSIOThread {
 public:
  WebViewIOThread(PrefService* local_state, net::NetLog* net_log);
  ~WebViewIOThread() override;

 protected:
  // io_thread::IOSIOThread overrides
  std::unique_ptr<net::NetworkDelegate> CreateSystemNetworkDelegate() override;
  std::string GetChannelString() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewIOThread);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_APP_WEB_VIEW_IO_THREAD_H_
