// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_HANDLE_TRACKER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_HANDLE_TRACKER_H_

#include "base/memory/scoped_refptr.h"
#include "content/browser/web_package/web_bundle_reader.h"
#include "url/gurl.h"

namespace content {

class WebBundleHandle;
class WebBundleReader;

// This class is used to track navigations within the Web Bundle file.
class WebBundleHandleTracker {
 public:
  WebBundleHandleTracker(scoped_refptr<WebBundleReader> reader,
                         const GURL& target_inner_url);

  WebBundleHandleTracker(const WebBundleHandleTracker&) = delete;
  WebBundleHandleTracker& operator=(const WebBundleHandleTracker&) = delete;

  ~WebBundleHandleTracker();

  // Returns a WebBundleHandle to handle the navigation request to |url|
  // if the Web Bundle file contains the matching response. Otherwise returns
  // null.
  std::unique_ptr<WebBundleHandle> MaybeCreateWebBundleHandle(
      const GURL& url,
      int frame_tree_node_id);

 private:
  scoped_refptr<WebBundleReader> reader_;
  const GURL target_inner_url_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_HANDLE_TRACKER_H_
