// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_OFFLINE_PAGE_DISTILLER_VIEWER_H_
#define IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_OFFLINE_PAGE_DISTILLER_VIEWER_H_

#include <memory>
#include <string>

#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_viewer_interface.h"

class GURL;

// A very simple and naive implementation of the DistillerViewer for pages that
// will be viewed in offline mode.
class OfflinePageDistillerViewer : public DistillerViewerInterface {
 public:
  // Creates a `DistillerView` without depending on the DistillerService.
  // Caller must provide `distiller_service` and `page` which cannot be null.
  // `callback` is called when distillation is finished with the protobuf
  // containing the distilled page.
  OfflinePageDistillerViewer(DistillerService* distiller_service,
                             std::unique_ptr<dom_distiller::DistillerPage> page,
                             const GURL& url,
                             DistillationFinishedCallback callback);

  OfflinePageDistillerViewer(const OfflinePageDistillerViewer&) = delete;
  OfflinePageDistillerViewer& operator=(const OfflinePageDistillerViewer&) =
      delete;

  ~OfflinePageDistillerViewer() override;

  // DistillerViewerInterface implementation
  // Called by the distiller service when article is ready.
  void OnArticleReady(
      const dom_distiller::DistilledArticleProto* article_proto) override;

  void SendJavaScript(const std::string& buffer) override;

 private:
  std::string GetCspNonce();

  // Called by the distiller when article is ready.
  void OnDistillerFinished(
      std::unique_ptr<dom_distiller::DistilledArticleProto> distilled_article);

  // Called by the distiller when article is updated.
  void OnArticleDistillationUpdated(
      const dom_distiller::ArticleDistillationUpdate& article_update);

  // The url of the distilled page.
  const GURL url_;
  // JavaScript buffer.
  std::string js_buffer_;
  // CSP nonce value.
  std::string csp_nonce_;
  // Callback to run once distillation is complete.
  DistillationFinishedCallback callback_;

  base::WeakPtrFactory<OfflinePageDistillerViewer> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_OFFLINE_PAGE_DISTILLER_VIEWER_H_
