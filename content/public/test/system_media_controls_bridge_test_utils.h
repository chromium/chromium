// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SYSTEM_MEDIA_CONTROLS_BRIDGE_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_SYSTEM_MEDIA_CONTROLS_BRIDGE_TEST_UTILS_H_

#include "base/functional/callback.h"

namespace content {

// Tests that want to know when SystemMediaControlsBridge objects have
// been created in the app shim process should set this.
void SetOnSystemMediaControlsBridgeCreatedCallbackForTesting(
    base::RepeatingCallback<void()> callback);

// Tests that want to know when the SystemMediaControlsBridge object
// has been created in the browser process should set this.
void SetOnBrowserSystemMediaControlsBridgeCreatedCallbackForTesting(
    base::RepeatingCallback<void()> callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SYSTEM_MEDIA_CONTROLS_BRIDGE_TEST_UTILS_H_
