// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRELOADING_TRIGGER_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_PRELOADING_TRIGGER_TYPE_H_

namespace content {

// If you add a new type of preloading trigger, please refer to the internal
// document go/update-prerender-new-trigger-metrics to make sure that metrics
// include the newly added trigger type. The string for the trigger type is
// generated in PrerenderPageLoadMetricsObserver::AppendSuffix.
// LINT.IfChange
enum class PreloadingTriggerType {
  // https://wicg.github.io/nav-speculation/prerendering.html#speculation-rules
  kSpeculationRule,
  // Same as kSpeculationRule but triggered in isolated worlds like Extensions.
  kSpeculationRuleFromIsolatedWorld,
  // Same as kSpeculationRule but injected by the browser's auto speculation
  // rules feature.
  kSpeculationRuleFromAutoSpeculationRules,
  // Trigger used by content embedders.
  kEmbedder,
};
// LINT.ThenChange(//components/page_load_metrics/browser/observers/prerender_page_load_metrics_observer.cc)

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRELOADING_TRIGGER_TYPE_H_
