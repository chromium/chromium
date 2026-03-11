// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_COBALT_COBALT_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_COBALT_COBALT_API_H_

class Browser;
class TabHelperAttacher;

namespace ios::provider {

// Attaches the Cobalt tab helpers using the given `attacher`.
void AttachCobaltTabHelpers(TabHelperAttacher& attacher);

// Attaches the Cobalt browser agents to the given `browser`.
void AttachCobaltBrowserAgentsForActiveBrowser(Browser* browser);

// Ensures the Cobalt profile keyed service factories are built.
void EnsureCobaltProfileKeyedServiceFactoriesBuilt();

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_COBALT_COBALT_API_H_
