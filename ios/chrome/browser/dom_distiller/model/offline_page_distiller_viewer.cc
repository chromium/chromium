// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/dom_distiller/model/offline_page_distiller_viewer.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/distiller.h"
#include "components/dom_distiller/core/dom_distiller_request_view_base.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/viewer.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"
#include "ui/gfx/geometry/size.h"

OfflinePageDistillerViewer::OfflinePageDistillerViewer(
    DistillerService* distiller_service,
    std::unique_ptr<dom_distiller::DistillerPage> page,
    const GURL& url,
    DistillationFinishedCallback callback)
    : DistillerViewerInterface(distiller_service->GetDistilledPagePrefs()),
      url_(url),
      csp_nonce_(base::Base64Encode(base::RandBytesAsVector(16))),
      callback_(std::move(callback)) {
  DCHECK(url.is_valid());
  DCHECK(page->ShouldFetchOfflineData());
  SendCommonJavaScript();
  distiller_service->DistillPage(
      url, std::move(page),
      base::BindOnce(&OfflinePageDistillerViewer::OnDistillerFinished,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &OfflinePageDistillerViewer::OnArticleDistillationUpdated,
          weak_ptr_factory_.GetWeakPtr()));
}

OfflinePageDistillerViewer::~OfflinePageDistillerViewer() {}

void OfflinePageDistillerViewer::OnArticleDistillationUpdated(
    const dom_distiller::ArticleDistillationUpdate& article_update) {}

void OfflinePageDistillerViewer::OnDistillerFinished(
    std::unique_ptr<dom_distiller::DistilledArticleProto> distilled_article) {
  OnArticleReady(distilled_article.get());
}

void OfflinePageDistillerViewer::OnArticleReady(
    const dom_distiller::DistilledArticleProto* article_proto) {
  DCHECK(!callback_.is_null());
  DomDistillerRequestViewBase::OnArticleReady(article_proto);
  bool is_empty = article_proto->pages_size() == 0 ||
                  article_proto->pages(0).html().empty();
  if (!is_empty) {
    std::vector<ImageInfo> images;
    for (int p = 0; p < article_proto->pages_size(); p++) {
      for (int i = 0; i < article_proto->pages(p).image_size(); i++) {
        auto image = article_proto->pages(p).image(i);
        images.push_back(ImageInfo{GURL(image.url()), image.data()});
      }
    }

    const std::string html = dom_distiller::viewer::GetArticleTemplateHtml(
        distilled_page_prefs_->GetTheme(),
        distilled_page_prefs_->GetFontFamily(), csp_nonce_,
        /*use_offline_data=*/true);

    std::string html_and_script(html);
    html_and_script += "<script nonce=\"" + csp_nonce_ + "\">" +
                       "distillerOnIos = true; " + js_buffer_ + "</script>";

    std::move(callback_).Run(url_, html_and_script, images,
                             article_proto->title(), csp_nonce_);
  } else {
    std::move(callback_).Run(url_, std::string(), {}, std::string(),
                             std::string());
  }
}

void OfflinePageDistillerViewer::SendJavaScript(const std::string& buffer) {
  js_buffer_ += buffer;
}

std::string OfflinePageDistillerViewer::GetCspNonce() {
  return csp_nonce_;
}
