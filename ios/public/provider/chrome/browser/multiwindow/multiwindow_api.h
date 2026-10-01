// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MULTIWINDOW_MULTIWINDOW_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MULTIWINDOW_MULTIWINDOW_API_H_

@class UIWindowScene;

namespace ios::provider {

// On a device that supports multiple scenes, returns whether a new scene can
// currently be created given that `scene` is active. The returned value is only
// meaningful when `base::ios::IsMultipleScenesSupported()` is `true`.
bool IsWindowSceneActivationAllowed(UIWindowScene* scene);

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MULTIWINDOW_MULTIWINDOW_API_H_
