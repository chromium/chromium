// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INSTALLEDAPP_INSTALLED_APP_PROVIDER_IMPL_WIN_H_
#define CONTENT_BROWSER_INSTALLEDAPP_INSTALLED_APP_PROVIDER_IMPL_WIN_H_

#include <vector>

#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom.h"

class GURL;

namespace content {
namespace installed_app_provider_win {

// Windows specific implementation of getInstalledRelatedApps.
// It will filter the given related apps list against the Windows verified
// application list.
void FilterInstalledAppsForWin(
    std::vector<blink::mojom::RelatedApplicationPtr> related_apps,
    blink::mojom::InstalledAppProvider::FilterInstalledAppsCallback callback,
    const GURL frame_url);

}  // namespace installed_app_provider_win
}  // namespace content

#endif  // CONTENT_BROWSER_INSTALLEDAPP_INSTALLED_APP_PROVIDER_IMPL_WIN_H_
