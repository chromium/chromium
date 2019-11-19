// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SITE_ISOLATION_POLICY_H_
#define CONTENT_PUBLIC_BROWSER_SITE_ISOLATION_POLICY_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string_piece_forward.h"
#include "content/common/content_export.h"
#include "url/origin.h"

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

  // Returns true if isolated origins feature is enabled.
  static bool AreIsolatedOriginsEnabled();

  // Returns true if strict origin isolation is enabled. Controls whether site
  // isolation uses origins instead of scheme and eTLD+1.
  static bool IsStrictOriginIsolationEnabled();

  // Returns true if error page isolation is enabled.
  static bool IsErrorPageIsolationEnabled(bool in_main_frame);

  // Returns true if the PDF compositor should be enabled to allow out-of-
  // process iframes (OOPIF's) to print properly.
  static bool ShouldPdfCompositorBeEnabledForOopifs();

  // Returns true if isolated origins may be added at runtime in response
  // to hints such as users typing in a password or (in the future) an origin
  // opting itself into isolation via a header.
  static bool AreDynamicIsolatedOriginsEnabled();

  // Applies isolated origins from all available sources, including the
  // command-line switch, field trials, enterprise policy, and the embedder.
  // See also AreIsolatedOriginsEnabled. These origins apply globally to the
  // whole browser in all profiles.  This should be called once on browser
  // startup.
  static void ApplyGlobalIsolatedOrigins();

  // Records metrics about which site isolation command-line flags are present,
  // and sets up a timer to keep recording them every 24 hours.  This should be
  // called once on browser startup.
  static void StartRecordingSiteIsolationFlagUsage();

 private:
  SiteIsolationPolicy();  // Not instantiable.

  // Gets isolated origins from cmdline and/or from field trial param.
  static std::string GetIsolatedOriginsFromCommandLine();
  static std::string GetIsolatedOriginsFromFieldTrial();

  // Records metrics about which site isolation command-line flags are present.
  static void RecordSiteIsolationFlagUsage();

  DISALLOW_COPY_AND_ASSIGN(SiteIsolationPolicy);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SITE_ISOLATION_POLICY_H_
