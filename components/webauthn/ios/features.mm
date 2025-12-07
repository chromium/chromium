// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/features.h"

// Enabled by default in M144. It should be kept around as a kill-switch
// indefinitely.
BASE_FEATURE(kIOSPasskeyShim, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSPasskeyModalLoginWithShim, base::FEATURE_DISABLED_BY_DEFAULT);
