// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SITE_ISOLATION_MODE_H_
#define CONTENT_PUBLIC_BROWSER_SITE_ISOLATION_MODE_H_

namespace content {

// Refers to particular kinds of site isolation which may be active. This is
// used to specify different memory thresholds for different kinds of site
// isolation.
enum class SiteIsolationMode {
  // This specifies a mode where every site requires a dedicated process,
  // a.k.a. --site-per-process.
  kStrictSiteIsolation,
  // This specifies modes where only some sites require a dedicated process.
  // This is primarily used on Android and includes isolation of login sites
  // and COOP sites.
  kPartialSiteIsolation
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SITE_ISOLATION_MODE_H_
