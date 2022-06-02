// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_params.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "content/browser/speculation_rules/prefetch/prefetch_features.h"

namespace content {

bool PrefetchContentRefactorIsEnabled() {
  return base::FeatureList::IsEnabled(features::kPrefetchUseContentRefactor);
}

GURL PrefetchProxyHost() {
  // Command line overrides take priority.
  std::string cmd_line_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "isolated-prerender-tunnel-proxy");
  if (!cmd_line_value.empty()) {
    GURL cmd_line_url(cmd_line_value);
    if (cmd_line_url.is_valid()) {
      return cmd_line_url;
    }
    LOG(ERROR) << "--isolated-prerender-tunnel-proxy value is invalid";
  }

  GURL url(base::GetFieldTrialParamValueByFeature(
      features::kPrefetchUseContentRefactor, "proxy_host"));
  if (url.is_valid() && url.SchemeIs(url::kHttpsScheme)) {
    return url;
  }

  // TODO(https://crbug.com/1299059): Get default URL of the prefetch proxy
  // server via a delegate.
  return GURL("");
}

std::string PrefetchProxyHeaderKey() {
  std::string header = base::GetFieldTrialParamValueByFeature(
      features::kPrefetchUseContentRefactor, "proxy_header_key");
  if (!header.empty()) {
    return header;
  }
  return "chrome-tunnel";
}

std::string PrefetchProxyServerExperimentGroup() {
  return base::GetFieldTrialParamValueByFeature(
      features::kPrefetchUseContentRefactor, "server_experiment_group");
}

int PrefetchServiceMaximumNumberOfConcurrentPrefetches() {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kPrefetchUseContentRefactor, "max_concurrent_prefetches", 1);
}

bool PrefetchServiceSendDecoyRequestForIneligblePrefetch() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "prefetch-proxy-never-send-decoy-requests-for-testing")) {
    return false;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "prefetch-proxy-always-send-decoy-requests-for-testing")) {
    return true;
  }

  // TODO(https://crbug.com/1299059): Check if the user has opted-in to Make
  // Search and Browsing Better. If so, then we don't need to send decoys. Doing
  // this will require a delegate.

  double probability = base::GetFieldTrialParamByFeatureAsDouble(
      features::kPrefetchUseContentRefactor,
      "ineligible_decoy_request_probability", 1.0);

  // Clamp to [0.0, 1.0].
  probability = std::max(0.0, probability);
  probability = std::min(1.0, probability);

  // RandDouble returns [0.0, 1.0) so don't use <= here since that may return
  // true when the probability is supposed to be 0 (i.e.: always false).
  return base::RandDouble() < probability;
}

base::TimeDelta PrefetchTimeoutDuration() {
  return base::Milliseconds(base::GetFieldTrialParamByFeatureAsInt(
      features::kPrefetchUseContentRefactor, "prefetch_timeout_ms",
      10 * 1000 /* 10 seconds */));
}

size_t PrefetchMainframeBodyLengthLimit() {
  return 1024 * base::GetFieldTrialParamByFeatureAsInt(
                    features::kPrefetchUseContentRefactor,
                    "max_mainframe_body_length_kb", 5 * 1024);
}

bool PrefetchCloseIdleSockets() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kPrefetchUseContentRefactor, "close_idle_sockets", true);
}

base::TimeDelta PrefetchContainerLifetimeInPrefetchService() {
  // A value of 0 or less, indicates that |PrefetchService| should keep the
  // prefetch forever.
  return base::Seconds(base::GetFieldTrialParamByFeatureAsInt(
      features::kPrefetchUseContentRefactor, "prefetch_container_lifetime_s",
      10 * 60 /* 10 minutes */));
}

bool PrefetchServiceHTMLOnly() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kPrefetchUseContentRefactor, "html_only", false);
}

base::TimeDelta PrefetchCacheableDuration() {
  return base::Seconds(base::GetFieldTrialParamByFeatureAsInt(
      features::kPrefetchUseContentRefactor, "cacheable_duration", 300));
}

}  // namespace content
