// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_FEATURES_H_

#include "base/feature_list.h"

namespace session::features {

// Feature flag to enable the Check to make sure the restore session ID are
// lower than the next SessionID.
BASE_DECLARE_FEATURE(kSessionRestorationSessionIDCheck);

}  // namespace session::features

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_FEATURES_H_
