// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SITE_INSTANCE_PROCESS_ASSIGNMENT_H_
#define CONTENT_PUBLIC_BROWSER_SITE_INSTANCE_PROCESS_ASSIGNMENT_H_

namespace content {

// This enum describes how a renderer process is assigned to a SiteInstance.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SiteInstanceProcessAssignment)
enum class SiteInstanceProcessAssignment {
  // No renderer process has been assigned to the SiteInstance yet.
  UNKNOWN = 0,

  // Reused some pre-existing process.
  REUSED_EXISTING_PROCESS = 1,

  // Used an existing spare process.
  USED_SPARE_PROCESS = 2,

  // No renderer could be reused, so a new one was created for the SiteInstance.
  CREATED_NEW_PROCESS = 3,

  kMaxValue = CREATED_NEW_PROCESS,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:SiteInstanceProcessAssignment)

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SITE_INSTANCE_PROCESS_ASSIGNMENT_H_
