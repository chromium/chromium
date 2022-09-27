// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_DOCUMENT_HOST_H_
#define CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_DOCUMENT_HOST_H_

#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"

namespace content {

class RenderFrameHost;

// The object can only be bound to a document if the `kBrowsingTopics` feature
// is enabled, and if the document does not have an opaque origin.
class CONTENT_EXPORT BrowsingTopicsDocumentHost final
    : public DocumentService<blink::mojom::BrowsingTopicsDocumentService> {
 public:
  BrowsingTopicsDocumentHost(const BrowsingTopicsDocumentHost&) = delete;
  BrowsingTopicsDocumentHost& operator=(const BrowsingTopicsDocumentHost&) =
      delete;
  BrowsingTopicsDocumentHost(BrowsingTopicsDocumentHost&&) = delete;
  BrowsingTopicsDocumentHost& operator=(BrowsingTopicsDocumentHost&&) = delete;

  static void CreateMojoService(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::BrowsingTopicsDocumentService>
          receiver);

  // blink::mojom::BrowsingTopicsDocumentService.
  void GetBrowsingTopics(bool observe,
                         GetBrowsingTopicsCallback callback) override;

 private:
  BrowsingTopicsDocumentHost(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::BrowsingTopicsDocumentService>
          receiver);

  // |this| can only be destroyed by DocumentService.
  ~BrowsingTopicsDocumentHost() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_DOCUMENT_HOST_H_
