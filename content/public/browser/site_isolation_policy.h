// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SITE_ISOLATION_POLICY_H_
#define CONTENT_PUBLIC_BROWSER_SITE_ISOLATION_POLICY_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string_piece_forward.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace network {
namespace mojom {
class URLLoaderFactoryParams;
}
}  // namespace network

namespace content {

// A centralized place for making policy decisions about out-of-process iframes,
// site isolation, --site-per-process, and related features.
//
// This is currently static because all these modes are controlled by command-
// line flags or field trials.
//
// These methods can be called from any thread.
class CONTENT_EXPORT SiteIsolationPolicy {
 public:
  // Returns true if every site should be placed in a dedicated process.
  static bool UseDedicatedProcessesForAllSites();

  // Populates CORB-related (Cross-Origin Read Blocking related) parts of the
  // URLLoaderFactoryParams depending on the current Site Isolation policy.
  static void PopulateURLLoaderFactoryParamsPtrForCORB(
      network::mojom::URLLoaderFactoryParams* params);

  // Returns true if isolated origins feature is enabled.
  static bool AreIsolatedOriginsEnabled();

  // Returns true if error page isolation is enabled.
  static bool IsErrorPageIsolationEnabled(bool in_main_frame);

  // Returns true if the PDF compositor should be enabled to allow out-of-
  // process iframes (OOPIF's) to print properly.
  static bool ShouldPdfCompositorBeEnabledForOopifs();

  // Returns the origins to isolate.  See also AreIsolatedOriginsEnabled.
  // This list applies globally to the whole browser in all profiles.
  static std::vector<url::Origin> GetIsolatedOrigins();

  // Records metrics about which site isolation command-line flags are present,
  // and sets up a timer to keep recording them every 24 hours.  This should be
  // called once on browser startup.
  static void StartRecordingSiteIsolationFlagUsage();

 private:
  SiteIsolationPolicy();  // Not instantiable.

  // Parses |arg| into a list of origins.
  static std::vector<url::Origin> ParseIsolatedOrigins(base::StringPiece arg);
  FRIEND_TEST_ALL_PREFIXES(SiteIsolationPolicyTest, ParseIsolatedOrigins);

  // Gets isolated origins from cmdline and/or from field trial param.
  static std::vector<url::Origin> GetIsolatedOriginsFromEnvironment();

  // Records metrics about which site isolation command-line flags are present.
  static void RecordSiteIsolationFlagUsage();

  DISALLOW_COPY_AND_ASSIGN(SiteIsolationPolicy);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SITE_ISOLATION_POLICY_H_
