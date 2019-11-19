// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STARTUP_HELPER_H_
#define CONTENT_BROWSER_STARTUP_HELPER_H_

#include "base/metrics/field_trial.h"
#include "content/common/content_export.h"

namespace content {

// Setups fields trials and the FeatureList, and returns the unique pointer of
// the field trials.
std::unique_ptr<base::FieldTrialList> CONTENT_EXPORT
SetUpFieldTrialsAndFeatureList();

// Starts the ThreadPool.
void CONTENT_EXPORT StartBrowserThreadPool();

}  // namespace content

#endif  // CONTENT_BROWSER_STARTUP_HELPER_H_
