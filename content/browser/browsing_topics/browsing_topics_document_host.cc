// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_topics/browsing_topics_document_host.h"

#include "base/functional/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"

namespace content {

BrowsingTopicsDocumentHost::BrowsingTopicsDocumentHost(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::BrowsingTopicsDocumentService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

// static
void BrowsingTopicsDocumentHost::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::BrowsingTopicsDocumentService>
        receiver) {
  CHECK(render_frame_host);

  if (render_frame_host->GetLastCommittedOrigin().opaque()) {
    mojo::ReportBadMessage(
        "Unexpected BrowsingTopicsDocumentHost::CreateMojoService in an opaque "
        "origin document.");
    return;
  }

  if (render_frame_host->IsNestedWithinFencedFrame()) {
    mojo::ReportBadMessage(
        "Unexpected BrowsingTopicsDocumentHost::CreateMojoService in a fenced "
        "frame.");
    return;
  }

  if (render_frame_host->GetLifecycleState() ==
      RenderFrameHost::LifecycleState::kPrerendering) {
    mojo::ReportBadMessage(
        "Unexpected BrowsingTopicsDocumentHost::CreateMojoService when the "
        "page is being prerendered.");
    return;
  }

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new BrowsingTopicsDocumentHost(*render_frame_host, std::move(receiver));
}

void BrowsingTopicsDocumentHost::GetBrowsingTopics(
    bool observe,
    GetBrowsingTopicsCallback callback) {
  // IPCs may race with navigation events, so:
  // - Ignore non-active frames, e.g. a frame placed into bfcache, or a
  //   frame that has been detached (this could happen as a result of
  //   cross-process races when navigating).
  if (!render_frame_host().IsActive() ||
      // Ignore non-primary frames. Fenced frames and prerendered pages are
      // covered in this condition but they should have already been checked in
      // `CreateMojoService()`.
      !render_frame_host().GetPage().IsPrimary()) {
    std::move(callback).Run(
        blink::mojom::GetBrowsingTopicsResult::NewErrorMessage(
            "document.browsingTopics() is only allowed in the outermost page "
            "and when the page is active."));
    return;
  }

  // Skip if the RFH's `StoragePartition` isn't the `BrowserContext`'s default
  // one. The desired way to process it is TBD.
  if (render_frame_host().GetStoragePartition() !=
      render_frame_host().GetBrowserContext()->GetDefaultStoragePartition()) {
    std::move(callback).Run(
        blink::mojom::GetBrowsingTopicsResult::NewBrowsingTopics(
            std::vector<blink::mojom::EpochTopicPtr>()));
    return;
  }

  std::vector<blink::mojom::EpochTopicPtr> topics;
  GetContentClient()->browser()->HandleTopicsWebApi(
      render_frame_host().GetLastCommittedOrigin(),
      render_frame_host().GetMainFrame(),
      browsing_topics::ApiCallerSource::kJavaScript,
      /*get_topics=*/true, observe, topics);

  std::move(callback).Run(
      blink::mojom::GetBrowsingTopicsResult::NewBrowsingTopics(
          std::move(topics)));
}

BrowsingTopicsDocumentHost::~BrowsingTopicsDocumentHost() = default;

}  // namespace content
