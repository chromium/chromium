// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_HANDLE_TRACKER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_HANDLE_TRACKER_H_

#include "base/memory/scoped_refptr.h"
#include "content/browser/web_package/bundled_exchanges_reader.h"
#include "url/gurl.h"

namespace content {

class BundledExchangesHandle;
class BundledExchangesReader;

// This class is used to track navigations within the bundled exchanges file.
class BundledExchangesHandleTracker {
 public:
  BundledExchangesHandleTracker(scoped_refptr<BundledExchangesReader> reader,
                                const GURL& target_inner_url);
  ~BundledExchangesHandleTracker();

  // Returns a BundledExchangesHandle to handle the navigation request to |url|
  // if the bundled exchanges file contains the matching response. Otherwise
  // returns null.
  std::unique_ptr<BundledExchangesHandle> MaybeCreateBundledExchangesHandle(
      const GURL& url,
      int frame_tree_node_id);

 private:
  scoped_refptr<BundledExchangesReader> reader_;
  const GURL target_inner_url_;

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesHandleTracker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_HANDLE_TRACKER_H_
