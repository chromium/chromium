// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_DISTILLER_VIEWER_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_DISTILLER_VIEWER_H_

#include <memory>
#include <string>

#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_viewer_interface.h"
#import "ios/web/public/web_state.h"

class GURL;

// An implementation of the DistillerViewer for pages that will update the
// Reader Mode UI surface (e.g. fonts, themes) dynamically.
class ReaderModeDistillerViewer : public DistillerViewerInterface {
 public:
  // Creates a `DistillerView` without depending on the DistillerService.
  // Caller must provide `distiller_service` and `page` which cannot be null.
  // `callback` is called when distillation is finished with the protobuf
  // containing the distilled page.
  ReaderModeDistillerViewer(web::WebState* web_state,
                            DistillerService* distiller_service,
                            std::unique_ptr<dom_distiller::DistillerPage> page,
                            const GURL& url,
                            DistillationFinishedCallback callback);

  ReaderModeDistillerViewer(const ReaderModeDistillerViewer&) = delete;
  ReaderModeDistillerViewer& operator=(const ReaderModeDistillerViewer&) =
      delete;

  ~ReaderModeDistillerViewer() override;

  // DistillerViewerInterface implementation:
  // Called by the distiller service when article is ready.
  void OnArticleReady(
      const dom_distiller::DistilledArticleProto* article_proto) override;
  void SendJavaScript(const std::string& buffer) override;

 private:
  // Executes JavaScript in an isolated world.
  void RunIsolatedJavaScript(const std::string& script);

  // Called by the distiller when article is ready.
  void OnDistillerFinished(
      std::unique_ptr<dom_distiller::DistilledArticleProto> distilled_article);

  // Called by the distiller when article is updated.
  void OnArticleDistillationUpdated(
      const dom_distiller::ArticleDistillationUpdate& article_update);

  // The url of the distilled page.
  const GURL url_;
  // Callback to run once distillation is complete.
  DistillationFinishedCallback callback_;
  // Whether the page is sufficiently initialized to handle updates from the
  // distiller.
  bool waiting_for_page_ready_;
  // Temporary store of pending JavaScript if the page isn't ready to receive
  // data from distillation.
  std::string buffer_;

  raw_ptr<web::WebState> web_state_;
  base::WeakPtrFactory<ReaderModeDistillerViewer> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_DISTILLER_VIEWER_H_
