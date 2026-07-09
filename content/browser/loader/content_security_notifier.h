// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_CONTENT_SECURITY_NOTIFIER_H_
#define CONTENT_BROWSER_LOADER_CONTENT_SECURITY_NOTIFIER_H_

#include "content/public/browser/weak_document_ptr.h"
#include "third_party/blink/public/mojom/loader/content_security_notifier.mojom.h"

namespace content {

// This is the implementation of blink::mojom::ContentSecurityNotifier that
// forwards notifications about content security (e.g., mixed contents,
// certificate errors) from a renderer process to WebContents.
class ContentSecurityNotifier final
    : public blink::mojom::ContentSecurityNotifier {
 public:
  explicit ContentSecurityNotifier(WeakDocumentPtr weak_document);
  ~ContentSecurityNotifier() override = default;

  ContentSecurityNotifier(const ContentSecurityNotifier&) = delete;
  ContentSecurityNotifier& operator=(const ContentSecurityNotifier&) = delete;

  // blink::mojom::ContentSecurityNotifier implementation.
  void NotifyContentWithCertificateErrorsRan() override;
  void NotifyContentWithCertificateErrorsDisplayed() override;
  void NotifyInsecureContentRan(
      const GURL& insecure_url,
      blink::mojom::ContentSecurityNotifier::InsecureContentOrigin origin_type)
      override;

 private:
  const WeakDocumentPtr weak_document_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_CONTENT_SECURITY_NOTIFIER_H_
