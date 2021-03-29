// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INSTALLEDAPP_INSTALLED_APP_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_INSTALLEDAPP_INSTALLED_APP_PROVIDER_IMPL_H_

#include <string>
#include <vector>

#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom.h"

namespace content {

class RenderFrameHost;

class InstalledAppProviderImpl : public blink::mojom::InstalledAppProvider,
                                 public content::WebContentsObserver {
 public:
  explicit InstalledAppProviderImpl(RenderFrameHost* render_frame_host);
  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::InstalledAppProvider> receiver);

  ~InstalledAppProviderImpl() override = default;

  // InstalledAppProvider overrides:
  void FilterInstalledApps(
      std::vector<blink::mojom::RelatedApplicationPtr> related_apps,
      const GURL& manifest_url,
      FilterInstalledAppsCallback callback) override;

  // WebContentsObserver
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;

 private:
  RenderFrameHost* render_frame_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INSTALLEDAPP_INSTALLED_APP_PROVIDER_IMPL_H_
