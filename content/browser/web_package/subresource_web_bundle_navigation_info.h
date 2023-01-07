// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SUBRESOURCE_WEB_BUNDLE_NAVIGATION_INFO_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SUBRESOURCE_WEB_BUNDLE_NAVIGATION_INFO_H_

#include <memory>

#include "base/unguessable_token.h"
#include "url/gurl.h"

namespace content {

// A class that holds necessary information for subframe navigation to a
// resource from the parent frame's subresource WebBundle.
class SubresourceWebBundleNavigationInfo {
 public:
  SubresourceWebBundleNavigationInfo(const GURL bundle_url,
                                     base::UnguessableToken token,
                                     int32_t render_process_id);
  ~SubresourceWebBundleNavigationInfo();
  const GURL& bundle_url() const { return bundle_url_; }
  const base::UnguessableToken& token() const { return token_; }
  int32_t render_process_id() const { return render_process_id_; }

  std::unique_ptr<SubresourceWebBundleNavigationInfo> Clone() const;

 private:
  const GURL bundle_url_;
  base::UnguessableToken token_;
  int32_t render_process_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SUBRESOURCE_WEB_BUNDLE_NAVIGATION_INFO_H_
