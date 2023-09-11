// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COOKIE_DEPRECATION_LABEL_COOKIE_DEPRECATION_LABEL_DOCUMENT_SERVICE_H_
#define CONTENT_BROWSER_COOKIE_DEPRECATION_LABEL_COOKIE_DEPRECATION_LABEL_DOCUMENT_SERVICE_H_

#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/cookie_deprecation_label/cookie_deprecation_label.mojom.h"

namespace content {

class RenderFrameHost;

class CookieDeprecationLabelDocumentService
    : public DocumentService<
          blink::mojom::CookieDeprecationLabelDocumentService> {
 public:
  static void CreateMojoService(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::CookieDeprecationLabelDocumentService>
          receiver);

  CookieDeprecationLabelDocumentService(
      const CookieDeprecationLabelDocumentService&) = delete;
  CookieDeprecationLabelDocumentService& operator=(
      const CookieDeprecationLabelDocumentService&) = delete;

 private:
  CookieDeprecationLabelDocumentService(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::CookieDeprecationLabelDocumentService>
          receiver);

  // `this` can only be destroyed by `DocumentService`.
  ~CookieDeprecationLabelDocumentService() override;

  // blink::mojom::CookieDeprecationLabelDocumentService:
  void GetValue(GetValueCallback callback) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COOKIE_DEPRECATION_LABEL_COOKIE_DEPRECATION_LABEL_DOCUMENT_SERVICE_H_
