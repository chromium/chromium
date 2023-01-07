// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_NET_LOG_OBSERVER_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_NET_LOG_OBSERVER_H_

#include <memory>

namespace base {
class FilePath;
}  // namespace base

namespace net {
class FileNetLogObserver;
}  // namespace net

class WebEngineNetLogObserver {
 public:
  explicit WebEngineNetLogObserver(const base::FilePath& log_path);

  WebEngineNetLogObserver(const WebEngineNetLogObserver&) = delete;
  WebEngineNetLogObserver& operator=(const WebEngineNetLogObserver&) = delete;

  ~WebEngineNetLogObserver();

 private:
  std::unique_ptr<net::FileNetLogObserver> file_net_log_observer_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_NET_LOG_OBSERVER_H_
