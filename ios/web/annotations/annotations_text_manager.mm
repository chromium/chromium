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

// static
void AnnotationsTextManager::CreateForWebState(WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           std::make_unique<AnnotationsTextManager>(web_state));
  }
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
  GetJSFeature()->DecorateAnnotations(web_state_, annotations);
}

void AnnotationsTextManager::RemoveDecorations() {
  GetJSFeature()->RemoveDecorations(web_state_);
}

void AnnotationsTextManager::RemoveHighlight() {
  GetJSFeature()->RemoveHighlight(web_state_);
}

void AnnotationsTextManager::StartExtractingText() {
  DCHECK(web_state_);
  const GURL& url = web_state_->GetVisibleURL();
  if (observers_.empty() || !web::UrlHasWebScheme(url) ||
      !web_state_->ContentIsHTML()) {
    return;
  }

  GetJSFeature()->ExtractText(web_state_, kMaxAnnotationsTextLength);
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

void AnnotationsTextManager::DidFinishNavigation(
    WebState* web_state,
    NavigationContext* navigation_context) {
  DCHECK(web_state_ == web_state);
  // PageLoaded isn't called for same document navigation.
  // TODO(crbug.com/1350973): investigate if text should be extracted at all in
  // this case? Why navigating in the same page would require it?
  if (navigation_context->IsSameDocument()) {
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

AnnotationsJavaScriptFeature* AnnotationsTextManager::GetJSFeature() {
  return js_feature_for_testing_ ? js_feature_for_testing_
                                 : AnnotationsJavaScriptFeature::GetInstance();
}

#pragma mark - Testing Methods

void AnnotationsTextManager::SetJSFeatureForTesting(
    AnnotationsJavaScriptFeature* feature) {
  js_feature_for_testing_ = feature;
}

WEB_STATE_USER_DATA_KEY_IMPL(AnnotationsTextManager)

}  // namespace web
