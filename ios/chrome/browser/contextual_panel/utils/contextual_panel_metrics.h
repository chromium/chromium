// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UTILS_CONTEXTUAL_PANEL_METRICS_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UTILS_CONTEXTUAL_PANEL_METRICS_H_

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
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:IOSContextualPanelModelRelevance)

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
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:IOSContextualPanelInfoBlockImpression)

// Values of the UMA IOS.ContextualPanel.DismissedReason histogram. Must be
// kept up to date with IOSContextualPanelDismissedReason in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ContextualPanelDismissedReason)
enum class ContextualPanelDismissedReason {
  UserDismissed = 0,
  TabChanged = 1,
  NavigationInitiated = 2,
  BlockInteraction = 3,
  KeyboardOpened = 4,
  kMaxValue = KeyboardOpened,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:IOSContextualPanelDismissedReason)

// Values of the UMA IOS.ContextualPanel.Entrypoint histograms. Must be
// kept up to date with IOSContextualPanelEntrypointInteractionType in
// enums.xml. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
// LINT.IfChange(EntrypointInteractionType)
enum class EntrypointInteractionType {
  Displayed = 0,
  Tapped = 1,
  kMaxValue = Tapped,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:IOSContextualPanelEntrypointInteractionType)

// Values of the UMA IOS.ContextualPanel.IPH.DismissedReason histogram (IPH here
// is an acronym for in-product help). Must be kept up to date with
// IOSContextualPanelIPHDismissedReason in enums.xml. These values are persisted
// to logs. Entries should not be renumbered and numeric values should never be
// reused.
// LINT.IfChange(ContextualPanelIPHDismissedReason)
enum class ContextualPanelIPHDismissedReason {
  Other = 0,
  UserDismissed = 1,
  TimedOut = 2,
  UserInteracted = 3,  // The user clicked on the IPH or the entrypoint.
  kMaxValue = UserInteracted,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:IOSContextualPanelIPHDismissedReason)

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UTILS_CONTEXTUAL_PANEL_METRICS_H_
