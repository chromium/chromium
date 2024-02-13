// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_CONTENT_THREAD_IMPL_H_
#define IOS_WEB_CONTENT_CONTENT_THREAD_IMPL_H_

#include "ios/web/public/thread/web_thread.h"

namespace web {

// ContentThreadImpl is an alternate backend for WebThread that uses a
// BrowserThreadImpl as its base.
class ContentThreadImpl : public WebThread {
 public:
  // WebThread static implementation:
  static scoped_refptr<base::SingleThreadTaskRunner> GetUIThreadTaskRunner(
      const WebTaskTraits& traits);
  static scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner(
      const WebTaskTraits& traits);
  static bool IsThreadInitialized(ID identifier);
  static bool CurrentlyOn(ID identifier);
  static std::string GetCurrentlyOnErrorMessage(ID expected);
  static bool GetCurrentThreadIdentifier(ID* identifier);
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_CONTENT_THREAD_IMPL_H_
