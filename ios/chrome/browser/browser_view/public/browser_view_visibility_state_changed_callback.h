// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_PUBLIC_BROWSER_VIEW_VISIBILITY_STATE_CHANGED_CALLBACK_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_PUBLIC_BROWSER_VIEW_VISIBILITY_STATE_CHANGED_CALLBACK_H_

#include "base/functional/callback.h"

enum class BrowserViewVisibilityState;

// Callback invoked when the browser view visibility state changed.
using BrowserViewVisibilityStateChangedCallback =
    base::RepeatingCallback<void(BrowserViewVisibilityState current_state,
                                 BrowserViewVisibilityState previous_state)>;

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_PUBLIC_BROWSER_VIEW_VISIBILITY_STATE_CHANGED_CALLBACK_H_
