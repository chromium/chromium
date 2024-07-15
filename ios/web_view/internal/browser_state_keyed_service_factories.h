// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_BROWSER_STATE_KEYED_SERVICE_FACTORIES_H_
#define IOS_WEB_VIEW_INTERNAL_BROWSER_STATE_KEYED_SERVICE_FACTORIES_H_

namespace ios_web_view {

// Instantiate all factories to setup dependency graph for pref registration.
// Must be called before the first WebViewBrowserState is constructed.
void EnsureBrowserStateKeyedServiceFactoriesBuilt();

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_BROWSER_STATE_KEYED_SERVICE_FACTORIES_H_
