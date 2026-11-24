// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SITE_ISOLATION_POLICY_H_
#define CONTENT_PUBLIC_BROWSER_SITE_ISOLATION_POLICY_H_

#include <string>

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// An enum for UMA reporting of why site isolation is disabled.
enum class SiteIsolationDisabledReason {
  kNotDisabled = 0,
  kDisabledBySwitch = 1,
  kDisabledByPolicy = 2,  // Android only.
  kDisabledByEmbedder = 3,
  kNotEnabledByDefault = 4,
  kUnknownReason = 5,
  // Site isolation is enabled for all sites except ads. See
  // SiteIsolationPolicy::UseDedicatedProcessesForAllSitesExceptAds().
  kAdsExcludedFromSiteIsolation = 6,
  kMaxValue = kAdsExcludedFromSiteIsolation,
};

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

  // Returns true if every site except ads should be placed in a dedicated
  // process (features::kExcludeAdsFromSiteIsolation). This mode is only an
  // upgrade from partial site isolation: it is never true when
  // UseDedicatedProcessesForAllSites() is true, so that no user who has full
  // site isolation loses it.
  static bool UseDedicatedProcessesForAllSitesExceptAds();

  // Returns true if sandboxed iframes should be isolated.
  static bool AreIsolatedSandboxedIframesEnabled();

  // Returns true if isolated origins feature is enabled.
  static bool AreIsolatedOriginsEnabled();

  // Returns true if strict origin isolation is enabled. Controls whether site
  // isolation uses origins instead of scheme and eTLD+1.
  static bool IsStrictOriginIsolationEnabled();

  // Returns true if error page isolation is enabled for main frames (if
  // `in_main_frame` is true) or subframes (otherwise). Subframe error page
  // isolation is disabled whenever partial site isolation is disabled (e.g.,
  // on low-memory Android devices), unless it has been explicitly enabled from
  // the command line.
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
  static bool AreOriginKeyedProcessesEnabledByDefault(
      BrowserContext* browser_context);

  // Returns whether defaulting to origin-keyed agent cluster (without
  // necessarily an origin-keyed process) is enabled.
  // OriginAgentClusters are enabled by default if kOriginIsolationHeader and
  // kOriginAgentClusterDefaultEnabled are enabled, and if there is no
  // enterprise policy forbidding it.
  static bool AreOriginAgentClustersEnabledByDefault(
      BrowserContext* browser_context);

  // Returns true if ads without an Origin-Agent-Cluster header are excluded
  // from origin-keyed processes by default. A user (including the command line)
  // or enterprise override of origin-keyed processes turns this off, unless
  // kExcludeAdsFromOriginIsolation is also overridden from the command line.
  // The result can change mid-session, since it reads enterprise policy prefs
  // that can be updated dynamically. Callers that must stay consistent with an
  // earlier decision (e.g., an existing process lock) should store that
  // decision instead of calling this again; see NavigationRequest's frozen ad
  // decision and SiteInfo::GetAgentClusterKeyForURL().
  static bool AreAdsExcludedFromOriginKeyedProcesses(
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

  // Forces AreOriginKeyedProcessesEnabledByDefault() to not let command line or
  // enterprise overrides bypass the memory checks.
  static void IgnoreOriginKeyedProcessOverridesForTesting();

  // Returns true when process-isolation of fenced frames from their embedders
  // is enabled.
  static bool IsProcessIsolationForFencedFramesEnabled();

  // Returns true if site isolation is enabled and is at least as strict as
  // site-per-process.
  static bool IsSitePerProcessOrStricter();

  // Returns a reason why site isolation is not enabled to the extent of
  // site-per-process or stricter. For UMA logging.
  static SiteIsolationDisabledReason GetSiteIsolationDisabledReason();

 private:
  SiteIsolationPolicy();  // Not instantiable.

  // Gets isolated origins from cmdline and/or from field trial param.
  static std::string GetIsolatedOriginsFromCommandLine();
  static std::string GetIsolatedOriginsFromFieldTrial();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SITE_ISOLATION_POLICY_H_
