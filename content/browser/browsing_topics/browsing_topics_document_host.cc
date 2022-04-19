// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_topics/browsing_topics_document_host.h"

#include "base/bind.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"

namespace content {

BrowsingTopicsDocumentHost::BrowsingTopicsDocumentHost(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::BrowsingTopicsDocumentService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

// static
void BrowsingTopicsDocumentHost::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::BrowsingTopicsDocumentService>
        receiver) {
  DCHECK(render_frame_host);

  if (render_frame_host->GetLastCommittedOrigin().opaque()) {
    mojo::ReportBadMessage(
        "Unexpected BrowsingTopicsDocumentHost::CreateMojoService in an opaque "
        "origin document");
    return;
  }

  if (!render_frame_host->GetMainFrame()->IsInPrimaryMainFrame()) {
    mojo::ReportBadMessage(
        "Unexpected BrowsingTopicsDocumentHost::CreateMojoService in a "
        "non-primary main frame context.");
    return;
  }

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new BrowsingTopicsDocumentHost(render_frame_host, std::move(receiver));
}

void BrowsingTopicsDocumentHost::GetBrowsingTopics(
    GetBrowsingTopicsCallback callback) {
  std::vector<blink::mojom::EpochTopicPtr> browsing_topics =
      GetContentClient()->browser()->GetBrowsingTopicsForJsApi(
          render_frame_host()->GetLastCommittedOrigin(),
          render_frame_host()->GetMainFrame());

  std::move(callback).Run(std::move(browsing_topics));
}

BrowsingTopicsDocumentHost::~BrowsingTopicsDocumentHost() = default;

}  // namespace content
