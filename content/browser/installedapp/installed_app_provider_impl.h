// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INSTALLEDAPP_INSTALLED_APP_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_INSTALLEDAPP_INSTALLED_APP_PROVIDER_IMPL_H_

#include <vector>

#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom.h"

namespace content {

class RenderFrameHost;

class InstalledAppProviderImpl
    : public DocumentService<blink::mojom::InstalledAppProvider> {
 public:
  static void Create(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::InstalledAppProvider> receiver);

 private:
  // InstalledAppProvider overrides:
  void FilterInstalledApps(
      std::vector<blink::mojom::RelatedApplicationPtr> related_apps,
      const GURL& manifest_url,
      FilterInstalledAppsCallback callback) override;

  explicit InstalledAppProviderImpl(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::InstalledAppProvider>
          pending_receiver);
  ~InstalledAppProviderImpl() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INSTALLEDAPP_INSTALLED_APP_PROVIDER_IMPL_H_
