// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SITE_ISOLATION_POLICY_H_
#define CONTENT_PUBLIC_BROWSER_SITE_ISOLATION_POLICY_H_

#include <string>

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;

// A centralized place for making policy decisions about out-of-process iframes,
// site isolation, --site-per-process, and related features.
//
// This is currently static because all these modes are controlled by command-
// line flags or field trials.
//
// Unless otherwise stated, these methods can be called from any thread.
class CONTENT_EXPORT SiteIsolationPolicy {
 public:
  SiteIsolationPolicy(const SiteIsolationPolicy&) = delete;
  SiteIsolationPolicy& operator=(const SiteIsolationPolicy&) = delete;

  // Returns true if every site should be placed in a dedicated process.
  static bool UseDedicatedProcessesForAllSites();

  // Returns true if sandboxed iframes should be isolated.
  static bool AreIsolatedSandboxedIframesEnabled();

  // Returns true if isolated origins feature is enabled.
  static bool AreIsolatedOriginsEnabled();

  // Returns true if strict origin isolation is enabled. Controls whether site
  // isolation uses origins instead of scheme and eTLD+1.
  static bool IsStrictOriginIsolationEnabled();

  // Returns true if error page isolation is enabled.
  static bool IsErrorPageIsolationEnabled(bool in_main_frame);

  // Returns true if isolated origins may be added at runtime in response
  // to hints such as users typing in a password or sites serving headers like
  // Cross-Origin-Opener-Policy.
  static bool AreDynamicIsolatedOriginsEnabled();

  // Returns true if isolated origins preloaded with the browser should be
  // applied.  For example, this is used to apply memory limits to preloaded
  // isolated origins on Android.
  static bool ArePreloadedIsolatedOriginsEnabled();

  // Returns true if the "Origin-Agent-Cluster" header should result in a
  // separate process for isolated origins.  This is used to turn off opt-in
  // origin isolation on low-memory Android devices.
  static bool IsProcessIsolationForOriginAgentClusterEnabled();

  // Returns true if the OriginAgentCluster header will be respected.
  static bool IsOriginAgentClusterEnabled();

  // Returns true if the feature OriginKeyedProcessesByDefault is enabled, and
  // StrictSiteIsolation is also enabled (the latter will default to false on
  // Android).
  static bool AreOriginKeyedProcessesEnabledByDefault();

  // Returns whether defaulting to origin-keyed agent cluster (without
  // necessarily an origin-keyed process) is enabled.
  // OriginAgentClusters are enabled by default if kOriginIsolationHeader and
  // kOriginAgentClusterDefaultEnabled are enabled, and if there is no
  // enterprise policy forbidding it.
  static bool AreOriginAgentClustersEnabledByDefault(
      BrowserContext* browser_context);

  // Returns true if Cross-Origin-Opener-Policy headers may be used as
  // heuristics for turning on site isolation.
  static bool IsSiteIsolationForCOOPEnabled();

  // Return true if sites that were isolated due to COOP headers should be
  // persisted across restarts.
  static bool ShouldPersistIsolatedCOOPSites();

  // Applies isolated origins from all available sources, including the
  // command-line switch, field trials, enterprise policy, and the embedder.
  // See also AreIsolatedOriginsEnabled. These origins apply globally to the
  // whole browser in all profiles.  This should be called once on browser
  // startup.
  static void ApplyGlobalIsolatedOrigins();

  // Returns true if the given URL should be assigned the application isolation
  // level.
  // This must be called on the UI thread.
  static bool ShouldUrlUseApplicationIsolationLevel(
      BrowserContext* browser_context,
      const GURL& url);

  // Forces other methods in this class to reread flag values instead of using
  // their cached value.
  static void DisableFlagCachingForTesting();

  // Returns true when process-isolation of fenced frames from their embedders
  // is enabled.
  static bool IsProcessIsolationForFencedFramesEnabled();

 private:
  SiteIsolationPolicy();  // Not instantiable.

  // Gets isolated origins from cmdline and/or from field trial param.
  static std::string GetIsolatedOriginsFromCommandLine();
  static std::string GetIsolatedOriginsFromFieldTrial();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SITE_ISOLATION_POLICY_H_
