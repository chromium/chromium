// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {

class DevToolsHttpHandler;
class DevToolsPipeHandler;

// This class is a singleton that manage global DevTools state for the whole
// browser.
// TODO(dgozman): remove this class entirely.
class CONTENT_EXPORT DevToolsManager {
 public:
  // Returns single instance of this class. The instance is destroyed on the
  // browser main loop exit so this method MUST NOT be called after that point.
  static DevToolsManager* GetInstance();

  DevToolsManager();

  DevToolsManager(const DevToolsManager&) = delete;
  DevToolsManager& operator=(const DevToolsManager&) = delete;

  virtual ~DevToolsManager();

  DevToolsManagerDelegate* delegate() const { return delegate_.get(); }

  void SetHttpHandler(std::unique_ptr<DevToolsHttpHandler> http_handler);

  void SetPipeHandler(std::unique_ptr<DevToolsPipeHandler> pipe_handler);

 private:
  friend struct base::DefaultSingletonTraits<DevToolsManager>;

  std::unique_ptr<DevToolsManagerDelegate> delegate_;
  std::unique_ptr<DevToolsHttpHandler> http_handler_;
  std::unique_ptr<DevToolsPipeHandler> pipe_handler_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_H_
