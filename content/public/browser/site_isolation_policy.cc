// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/site_isolation_policy.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/site_isolation_mode.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "url/origin.h"

namespace content {

namespace {

bool g_disable_flag_caching_for_tests = false;

bool IsDisableSiteIsolationFlagPresent() {
  static const bool site_isolation_disabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSiteIsolation);
  if (g_disable_flag_caching_for_tests) {
    return base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableSiteIsolation);
  }
  return site_isolation_disabled;
}

#if BUILDFLAG(IS_ANDROID)
bool IsDisableSiteIsolationForPolicyFlagPresent() {
  static const bool site_isolation_disabled_by_policy =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSiteIsolationForPolicy);
  if (g_disable_flag_caching_for_tests) {
    return base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableSiteIsolationForPolicy);
  }
  return site_isolation_disabled_by_policy;
}
#endif

bool IsSiteIsolationDisabled(SiteIsolationMode site_isolation_mode) {
  if (IsDisableSiteIsolationFlagPresent()) {
    return true;
  }

#if BUILDFLAG(IS_ANDROID)
  // Desktop platforms no longer support disabling Site Isolation by policy.
  if (IsDisableSiteIsolationForPolicyFlagPresent()) {
    return true;
  }
#endif

  // Check with the embedder.  In particular, chrome/ uses this to disable site
  // isolation when below a memory threshold.
  return GetContentClient() &&
         GetContentClient()->browser()->ShouldDisableSiteIsolation(
             site_isolation_mode);
}

url::Origin RemovePort(const url::Origin& origin) {
  return url::Origin::CreateFromNormalizedTuple(origin.scheme(), origin.host(),
                                                /*port=*/0);
}

base::flat_set<url::Origin> CreateIsolatedAppOriginSet() {
  std::string cmdline_origins(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kIsolatedAppOrigins));

  std::vector<std::string> origin_strings = base::SplitString(
      cmdline_origins, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  base::flat_set<url::Origin> origin_set;
  for (const std::string& origin_string : origin_strings) {
    GURL allowed_url(origin_string);
    url::Origin allowed_origin = url::Origin::Create(allowed_url);
    if (!allowed_origin.opaque()) {
      // Site isolation is currently based on Site URLs, which don't include
      // ports. Ideally we'd use origin-based isolation for the origins in
      // kIsolatedAppOrigins, but long term the origins used in the flag will
      // be equivalent to their Site URL-ified version. Because of this, we
      // just remove the port here instead of hooking up origin-based isolation
      // that won't be needed long term.
      if (allowed_url.has_port()) {
        LOG(WARNING) << "Ignoring port number for Isolated App origin: "
                     << allowed_origin;
      }
      origin_set.insert(RemovePort(allowed_origin));
    } else {
      LOG(ERROR) << "Error parsing Isolated App origin: " << origin_string;
    }
  }
  return origin_set;
}

const base::flat_set<url::Origin>& GetIsolatedAppOriginSet() {
  static base::NoDestructor<base::flat_set<url::Origin>> kIsolatedAppOrigins(
      CreateIsolatedAppOriginSet());
  return *kIsolatedAppOrigins;
}

}  // namespace

// static
bool SiteIsolationPolicy::UseDedicatedProcessesForAllSites() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess)) {
    return true;
  }

  if (IsSiteIsolationDisabled(SiteIsolationMode::kStrictSiteIsolation))
    return false;

  // The switches above needs to be checked first, because if the
  // ContentBrowserClient consults a base::Feature, then it will activate the
  // field trial and assigns the client either to a control or an experiment
  // group - such assignment should be final.
  return GetContentClient() &&
         GetContentClient()->browser()->ShouldEnableStrictSiteIsolation();
}

// static
bool SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled() {
  // This feature is controlled by kIsolateSandboxedIframes, and depends on
  // partial Site Isolation being enabled. It also requires new base URL
  // behavior, so it implicitly causes
  // blink::features::IsNewBaseUrlInheritanceBehaviorEnabled() to return true,
  // and can't be enabled if the new base URL behavior has been disabled by
  // enterprise policy.
  return base::FeatureList::IsEnabled(
             blink::features::kIsolateSandboxedIframes) &&
         !IsSiteIsolationDisabled(SiteIsolationMode::kPartialSiteIsolation) &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             blink::switches::kDisableNewBaseUrlInheritanceBehavior);
}

// static
bool SiteIsolationPolicy::AreIsolatedOriginsEnabled() {
  // NOTE: Because it is possible for --isolate-origins to be isolating origins
  // at a finer-than-site granularity, we do not suppress --isolate-origins when
  // --site-per-process is also enabled.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kIsolateOrigins)) {
    return true;
  }

  if (IsSiteIsolationDisabled(SiteIsolationMode::kPartialSiteIsolation))
    return false;

  // The feature needs to be checked last, because checking the feature
  // activates the field trial and assigns the client either to a control or an
  // experiment group - such assignment should be final.
  return base::FeatureList::IsEnabled(features::kIsolateOrigins);
}

// static
bool SiteIsolationPolicy::IsStrictOriginIsolationEnabled() {
  // If the feature is explicitly enabled by the user (e.g., from
  // chrome://flags), honor this regardless of checks to disable site isolation
  // below.  This means this takes precedence over memory thresholds or
  // switches to disable site isolation.
  if (base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
          features::kStrictOriginIsolation.name,
          base::FeatureList::OVERRIDE_ENABLE_FEATURE)) {
    return true;
  }

  // TODO(wjmaclean): Figure out what should happen when this feature is
  // combined with --isolate-origins.
  //
  // TODO(alexmos): For now, use the same memory threshold for strict origin
  // isolation and strict site isolation.  In the future, strict origin
  // isolation may need its own memory threshold.
  if (IsSiteIsolationDisabled(SiteIsolationMode::kStrictSiteIsolation))
    return false;

  // The feature needs to be checked last, because checking the feature
  // activates the field trial and assigns the client either to a control or an
  // experiment group - such assignment should be final.
  return base::FeatureList::IsEnabled(features::kStrictOriginIsolation);
}

// static
bool SiteIsolationPolicy::IsErrorPageIsolationEnabled(bool in_main_frame) {
  return GetContentClient()->browser()->ShouldIsolateErrorPage(in_main_frame);
}

// static
bool SiteIsolationPolicy::AreDynamicIsolatedOriginsEnabled() {
  return !IsSiteIsolationDisabled(SiteIsolationMode::kPartialSiteIsolation);
}

// static
bool SiteIsolationPolicy::ArePreloadedIsolatedOriginsEnabled() {
  if (IsSiteIsolationDisabled(SiteIsolationMode::kPartialSiteIsolation))
    return false;

  // Currently, preloaded isolated origins are redundant when full site
  // isolation is enabled.  This may be true on Android if full site isolation
  // is enabled manually or via field trials.
  if (UseDedicatedProcessesForAllSites())
    return false;

  return true;
}

// static
bool SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled() {
  // If strict site isolation is in use (either by default on desktop or via a
  // user opt-in on Android), unconditionally enable opt-in origin isolation.
  if (UseDedicatedProcessesForAllSites())
    return true;

  // Otherwise, if site isolation is disabled (e.g., on Android due to being
  // under a memory threshold), turn off opt-in origin isolation.
  if (IsSiteIsolationDisabled(SiteIsolationMode::kPartialSiteIsolation))
    return false;

  return IsOriginAgentClusterEnabled();
}

// static
bool SiteIsolationPolicy::IsOriginAgentClusterEnabled() {
  return base::FeatureList::IsEnabled(features::kOriginIsolationHeader);
}

// static
bool SiteIsolationPolicy::IsSiteIsolationForCOOPEnabled() {
  // If the user has explicitly enabled site isolation for COOP sites from the
  // command line, honor this regardless of policies that may disable site
  // isolation.
  if (base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
          features::kSiteIsolationForCrossOriginOpenerPolicy.name,
          base::FeatureList::OVERRIDE_ENABLE_FEATURE)) {
    return true;
  }

  // Don't apply COOP isolation if site isolation has been disabled (e.g., due
  // to memory thresholds).
  if (!SiteIsolationPolicy::AreDynamicIsolatedOriginsEnabled())
    return false;

  // COOP isolation is only needed on platforms where strict site isolation is
  // not used.
  if (UseDedicatedProcessesForAllSites())
    return false;

  // The feature needs to be checked last, because checking the feature
  // activates the field trial and assigns the client either to a control or an
  // experiment group - such assignment should be final.
  return base::FeatureList::IsEnabled(
      features::kSiteIsolationForCrossOriginOpenerPolicy);
}

// static
bool SiteIsolationPolicy::ShouldPersistIsolatedCOOPSites() {
  if (!IsSiteIsolationForCOOPEnabled())
    return false;

  return features::kSiteIsolationForCrossOriginOpenerPolicyShouldPersistParam
      .Get();
}

// static
bool SiteIsolationPolicy::IsSiteIsolationForGuestsEnabled() {
  return base::FeatureList::IsEnabled(features::kSiteIsolationForGuests);
}

// static
std::string SiteIsolationPolicy::GetIsolatedOriginsFromCommandLine() {
  std::string cmdline_arg =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kIsolateOrigins);

  return cmdline_arg;
}

std::string SiteIsolationPolicy::GetIsolatedOriginsFromFieldTrial() {
  std::string origins;

  // Check if site isolation modes are turned off (e.g., due to an opt-out
  // flag).
  if (IsSiteIsolationDisabled(SiteIsolationMode::kPartialSiteIsolation))
    return origins;

  // The feature needs to be checked after the opt-out, because checking the
  // feature activates the field trial and assigns the client either to a
  // control or an experiment group - such assignment should be final.
  if (base::FeatureList::IsEnabled(features::kIsolateOrigins)) {
    origins = base::GetFieldTrialParamValueByFeature(
        features::kIsolateOrigins,
        features::kIsolateOriginsFieldTrialParamName);
  }

  return origins;
}

void SiteIsolationPolicy::ApplyGlobalIsolatedOrigins() {
  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();

  std::string from_cmdline = GetIsolatedOriginsFromCommandLine();
  policy->AddFutureIsolatedOrigins(
      from_cmdline,
      ChildProcessSecurityPolicy::IsolatedOriginSource::COMMAND_LINE);

  std::string from_trial = GetIsolatedOriginsFromFieldTrial();
  policy->AddFutureIsolatedOrigins(
      from_trial,
      ChildProcessSecurityPolicy::IsolatedOriginSource::FIELD_TRIAL);

  std::vector<url::Origin> from_embedder =
      GetContentClient()->browser()->GetOriginsRequiringDedicatedProcess();
  policy->AddFutureIsolatedOrigins(
      from_embedder,
      ChildProcessSecurityPolicy::IsolatedOriginSource::BUILT_IN);
}

// static
bool SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
    BrowserContext* browser_context,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  url::Origin origin = RemovePort(url::Origin::Create(url));
  bool origin_matches_flag = g_disable_flag_caching_for_tests
                                 ? CreateIsolatedAppOriginSet().contains(origin)
                                 : GetIsolatedAppOriginSet().contains(origin);
  return GetContentClient()->browser()->ShouldUrlUseApplicationIsolationLevel(
      browser_context, url, origin_matches_flag);
}

// static
void SiteIsolationPolicy::DisableFlagCachingForTesting() {
  g_disable_flag_caching_for_tests = true;
}

// static
bool SiteIsolationPolicy::IsProcessIsolationForFencedFramesEnabled() {
  // If the user has explicitly enabled process isolation for fenced frames from
  // the command line, honor this regardless of policies that may disable site
  // isolation.
  if (base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
          features::kIsolateFencedFrames.name,
          base::FeatureList::OVERRIDE_ENABLE_FEATURE)) {
    return true;
  }
  return UseDedicatedProcessesForAllSites() &&
         base::FeatureList::IsEnabled(features::kIsolateFencedFrames);
}

}  // namespace content
