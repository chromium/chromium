// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/annotations/annotations_text_manager.h"

#import "base/strings/string_util.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/annotations/annotations_text_observer.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

static const int kMaxAnnotationsTextLength = 65535;

AnnotationsTextManager::AnnotationsTextManager(WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
  web_state_->AddObserver(this);
}

AnnotationsTextManager::~AnnotationsTextManager() {
  web_state_ = nullptr;
}

void AnnotationsTextManager::AddObserver(AnnotationsTextObserver* observer) {
  observers_.AddObserver(observer);
}

void AnnotationsTextManager::RemoveObserver(AnnotationsTextObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AnnotationsTextManager::DecorateAnnotations(WebState* web_state,
                                                 base::Value& annotations) {
  DCHECK_EQ(web_state_, web_state);
  AnnotationsJavaScriptFeature::GetInstance()->DecorateAnnotations(web_state_,
                                                                   annotations);
}

void AnnotationsTextManager::RemoveDecorations() {
  AnnotationsJavaScriptFeature::GetInstance()->RemoveDecorations(web_state_);
}

void AnnotationsTextManager::RemoveHighlight() {
  AnnotationsJavaScriptFeature::GetInstance()->RemoveHighlight(web_state_);
}

void AnnotationsTextManager::StartExtractingText() {
  DCHECK(web_state_);
  const GURL& url = web_state_->GetVisibleURL();
  if (observers_.empty() || !web::UrlHasWebScheme(url) ||
      !web_state_->ContentIsHTML()) {
    return;
  }

  AnnotationsJavaScriptFeature::GetInstance()->ExtractText(
      web_state_, kMaxAnnotationsTextLength);
}

#pragma mark - WebStateObserver methods.

void AnnotationsTextManager::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK_EQ(web_state_, web_state);
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    StartExtractingText();
  }
}

void AnnotationsTextManager::WebStateDestroyed(WebState* web_state) {
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

#pragma mark - JS Methods

void AnnotationsTextManager::OnTextExtracted(WebState* web_state,
                                             const std::string& text) {
  DCHECK(web_state_ == web_state);
  for (auto& observer : observers_) {
    observer.OnTextExtracted(web_state, text);
  }
}

void AnnotationsTextManager::OnDecorated(WebState* web_state,
                                         int successes,
                                         int annotations) {
  DCHECK(web_state_ == web_state);
  for (auto& observer : observers_) {
    observer.OnDecorated(web_state, successes, annotations);
  }
}

void AnnotationsTextManager::OnClick(WebState* web_state,
                                     const std::string& text,
                                     CGRect rect,
                                     const std::string& data) {
  DCHECK(web_state_ == web_state);
  for (auto& observer : observers_) {
    observer.OnClick(web_state, text, rect, data);
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(AnnotationsTextManager)

}  // namespace web
