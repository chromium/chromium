// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_SHARING_MODEL_IOS_SHARE_URL_INTERCEPTION_CONTEXT_H_
#define IOS_CHROME_BROWSER_DATA_SHARING_MODEL_IOS_SHARE_URL_INTERCEPTION_CONTEXT_H_

#import "base/memory/weak_ptr.h"
#import "components/data_sharing/public/share_url_interception_context.h"

class Browser;

namespace data_sharing {

// iOS implementation of ShareURLInterceptionContext used to help with opening
// in the right window.
struct IOSShareURLInterceptionContext : public ShareURLInterceptionContext {
  explicit IOSShareURLInterceptionContext(Browser* browser);
  ~IOSShareURLInterceptionContext() override;

  IOSShareURLInterceptionContext(const IOSShareURLInterceptionContext&) =
      delete;
  IOSShareURLInterceptionContext& operator=(
      const IOSShareURLInterceptionContext&) = delete;

  base::WeakPtr<Browser> weak_browser;
};

}  // namespace data_sharing

#endif  // IOS_CHROME_BROWSER_DATA_SHARING_MODEL_IOS_SHARE_URL_INTERCEPTION_CONTEXT_H_
