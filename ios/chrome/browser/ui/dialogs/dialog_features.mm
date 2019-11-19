// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/dialog_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace dialogs {

const base::Feature kNonModalDialogs{"kNonModalDialogs",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace dialogs
