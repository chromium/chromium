// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COOP_COEP_CROSS_ORIGIN_ISOLATED_INFO_H_
#define CONTENT_BROWSER_COOP_COEP_CROSS_ORIGIN_ISOLATED_INFO_H_

#include "base/optional.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// Groups information about the cross-origin isolation of a page or group of
// pages. This is used for process allocation and to selectively enable
// powerful powerful features, such as SharedArrayBuffer.
//
// This is computed using the Cross-Origin-Opener-Policy and
// Cross-Origin-Embedder-Policy headers.
class CONTENT_EXPORT CoopCoepCrossOriginIsolatedInfo {
 public:
  static CoopCoepCrossOriginIsolatedInfo CreateNonIsolated();
  static CoopCoepCrossOriginIsolatedInfo CreateIsolated(
      const url::Origin& origin);

  CoopCoepCrossOriginIsolatedInfo(const CoopCoepCrossOriginIsolatedInfo& other);
  ~CoopCoepCrossOriginIsolatedInfo();

  // Returns the value of the window.crossOriginIsolated boolean.
  bool is_isolated() const { return origin_.has_value(); }

  // Returns the top level origin shared across pages with this cross-origin
  // isolation status. This only returns a value if is_isolated is true.
  const url::Origin& origin() const;

  bool operator==(const CoopCoepCrossOriginIsolatedInfo& b) const;
  bool operator!=(const CoopCoepCrossOriginIsolatedInfo& b) const;

  // Note: This only exists to be compatible with std::tie usage of SiteInfo.
  bool operator<(const CoopCoepCrossOriginIsolatedInfo& b) const;

 private:
  explicit CoopCoepCrossOriginIsolatedInfo(
      const base::Optional<url::Origin>& origin);

  // |origin_| serve two purposes. If null, it indicates that the page(s) it
  // refers to are not isolated, and that the crossOriginIsolated boolean is
  // false. If it has a value, all these page(s) share the same top level
  // origin. This ensure we can put them in the same process.
  base::Optional<url::Origin> origin_;
};

CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const CoopCoepCrossOriginIsolatedInfo& info);
}  // namespace content

#endif  // CONTENT_BROWSER_COOP_COEP_CROSS_ORIGIN_ISOLATED_INFO_H_
