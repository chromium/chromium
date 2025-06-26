// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_distiller_viewer.h"

#import <string>
#import <utility>

#import "base/base64.h"
#import "base/functional/bind.h"
#import "base/strings/utf_string_conversions.h"
#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "components/dom_distiller/core/distiller.h"
#import "components/dom_distiller/core/dom_distiller_request_view_base.h"
#import "components/dom_distiller/core/proto/distilled_article.pb.h"
#import "components/dom_distiller/core/viewer.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"

ReaderModeDistillerViewer::ReaderModeDistillerViewer(
    web::WebState* web_state,
    DistillerService* distiller_service,
    std::unique_ptr<dom_distiller::DistillerPage> page,
    const GURL& url,
    DistillationFinishedCallback callback)
    : DistillerViewerInterface(distiller_service->GetDistilledPagePrefs()),
      url_(url),
      callback_(std::move(callback)),
      waiting_for_page_ready_(true),
      web_state_(web_state) {
  DCHECK(url.is_valid());
  DCHECK(!page->ShouldFetchOfflineData());

  distilled_page_prefs_->AddObserver(this);

  SendCommonJavaScript();
  distiller_service->DistillPage(
      url, std::move(page),
      base::BindOnce(&ReaderModeDistillerViewer::OnDistillerFinished,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &ReaderModeDistillerViewer::OnArticleDistillationUpdated,
          weak_ptr_factory_.GetWeakPtr()));
}

ReaderModeDistillerViewer::~ReaderModeDistillerViewer() {
  distilled_page_prefs_->RemoveObserver(this);
}

void ReaderModeDistillerViewer::OnArticleDistillationUpdated(
    const dom_distiller::ArticleDistillationUpdate& article_update) {}

void ReaderModeDistillerViewer::OnDistillerFinished(
    std::unique_ptr<dom_distiller::DistilledArticleProto> distilled_article) {
  OnArticleReady(distilled_article.get());
}

void ReaderModeDistillerViewer::OnArticleReady(
    const dom_distiller::DistilledArticleProto* article_proto) {
  DCHECK(!callback_.is_null());
  DomDistillerRequestViewBase::OnArticleReady(article_proto);
  bool is_empty = article_proto->pages_size() == 0 ||
                  article_proto->pages(0).html().empty();
  if (!is_empty) {
    const std::string html = dom_distiller::viewer::GetArticleTemplateHtml(
        distilled_page_prefs_->GetTheme(),
        distilled_page_prefs_->GetFontFamily(), std::string(),
        /*use_offline_data=*/false);
    std::string html_and_script(html);
    html_and_script += "<script>" + buffer_ + "</script>";
    std::move(callback_).Run(url_, html_and_script, {}, article_proto->title(),
                             std::string());
    buffer_.clear();
    waiting_for_page_ready_ = false;
  } else {
    std::move(callback_).Run(url_, std::string(), {}, std::string(),
                             std::string());
  }
}

void ReaderModeDistillerViewer::SendJavaScript(const std::string& buffer) {
  if (waiting_for_page_ready_) {
    buffer_ += buffer;
  } else {
    DCHECK(buffer_.empty());
    RunIsolatedJavaScript(buffer);
  }
}

void ReaderModeDistillerViewer::RunIsolatedJavaScript(
    const std::string& script) {
  if (!web_state_) {
    return;
  }
  web::WebFramesManager* web_frames_manager =
      web_state_->GetWebFramesManager(web::ContentWorld::kPageContentWorld);
  web::WebFrame* main_frame = web_frames_manager->GetMainWebFrame();
  if (!main_frame) {
    return;
  }
  main_frame->ExecuteJavaScript(base::UTF8ToUTF16(script));
}
