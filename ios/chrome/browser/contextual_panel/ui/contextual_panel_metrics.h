// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_CONTEXTUAL_PANEL_METRICS_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_CONTEXTUAL_PANEL_METRICS_H_

// Values of the UMA IOS.ContextualPanel.Model.Relevance histograms. Must be
// kept up to date with IOSContextualPanelModelRelevance in enums.xml. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
// LINT.IfChange(ModelRelevanceType)
enum class ModelRelevanceType {
  NoData = 0,
  Low = 1,
  High = 2,
  kMaxValue = High,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:IOSContextualPanelModelRelevance)

// Values of the UMA IOS.ContextualPanel.InfoBlockImpression histograms. Must be
// kept up to date with IOSContextualPanelInfoBlockImpression in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PanelBlockImpressionType)
enum class PanelBlockImpressionType {
  NeverVisible = 0,
  VisibleAndSmallEntrypoint = 1,
  VisibleAndLoudEntrypoint = 2,
  VisibleAndOtherWasSmallEntrypoint = 3,
  VisibleAndOtherWasLoudEntrypoint = 4,
  kMaxValue = VisibleAndOtherWasLoudEntrypoint,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:IOSContextualPanelInfoBlockImpression)

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_CONTEXTUAL_PANEL_METRICS_H_
