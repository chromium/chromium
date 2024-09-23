// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_FIELD_TRIALS_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_FIELD_TRIALS_H_

#include "base/files/file_path.h"

class PrefService;

namespace headless {

// Returns positive if --enable-field-trial-config switch is specified in the
// command line.
bool ShouldEnableFieldTrials();

// Instantiates metrics state manager and sets up field trials.
void SetUpFieldTrials(PrefService* local_state,
                      const base::FilePath& user_data_dir);

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_FIELD_TRIALS_H_
