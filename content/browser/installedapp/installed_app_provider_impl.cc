// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/installedapp/installed_app_provider_impl.h"

#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"

#if BUILDFLAG(IS_WIN)
#include "content/browser/installedapp/installed_app_provider_impl_win.h"
#endif

namespace content {

namespace {

#if BUILDFLAG(IS_WIN)
void DidGetInstalledApps(
    bool is_off_the_record,
    InstalledAppProviderImpl::FilterInstalledAppsCallback callback,
    std::vector<blink::mojom::RelatedApplicationPtr> apps) {
  if (is_off_the_record) {
    // Don't expose the installed apps if this is off the record.
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(std::move(apps));
}
#endif

}  // namespace

InstalledAppProviderImpl::InstalledAppProviderImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::InstalledAppProvider> pending_receiver)
    : DocumentService(render_frame_host, std::move(pending_receiver)) {}

InstalledAppProviderImpl::~InstalledAppProviderImpl() = default;

void InstalledAppProviderImpl::FilterInstalledApps(
    std::vector<blink::mojom::RelatedApplicationPtr> related_apps,
    const GURL& manifest_url,
    FilterInstalledAppsCallback callback) {
  bool is_implemented = false;
  if (base::FeatureList::IsEnabled(features::kInstalledAppProvider)) {
#if BUILDFLAG(IS_WIN)
    is_implemented = true;
    bool is_off_the_record =
        render_frame_host().GetProcess()->GetBrowserContext()->IsOffTheRecord();
    installed_app_provider_win::FilterInstalledAppsForWin(
        std::move(related_apps),
        base::BindOnce(&DidGetInstalledApps, is_off_the_record,
                       std::move(callback)),
        render_frame_host().GetLastCommittedURL());
#endif
  }

  if (!is_implemented) {
    // Do not return any results.
    std::move(callback).Run(std::vector<blink::mojom::RelatedApplicationPtr>());
  }
}

// static
void InstalledAppProviderImpl::Create(
    RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::InstalledAppProvider> receiver) {
  if (host.GetParentOrOuterDocument()) {
    // The renderer is supposed to disallow this and we shouldn't end up here.
    mojo::ReportBadMessage(
        "InstalledAppProvider only allowed for outermost main frame.");
    return;
  }

  // The object is bound to the lifetime of |host|'s current document and the
  // mojo connection. See DocumentService for details.
  new InstalledAppProviderImpl(host, std::move(receiver));
}

}  // namespace content
