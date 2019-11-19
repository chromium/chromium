// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/installedapp/installed_app_provider_impl_default.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

InstalledAppProviderImplDefault::InstalledAppProviderImplDefault() {}

InstalledAppProviderImplDefault::~InstalledAppProviderImplDefault() {}

void InstalledAppProviderImplDefault::FilterInstalledApps(
    std::vector<blink::mojom::RelatedApplicationPtr> related_apps,
    FilterInstalledAppsCallback callback) {
  // Do not return any results (in the default implementation, there are no
  // installed related apps).
  std::move(callback).Run(std::vector<blink::mojom::RelatedApplicationPtr>());
}

// static
void InstalledAppProviderImplDefault::Create(
    mojo::PendingReceiver<blink::mojom::InstalledAppProvider> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<InstalledAppProviderImplDefault>(), std::move(receiver));
}

}  // namespace content
