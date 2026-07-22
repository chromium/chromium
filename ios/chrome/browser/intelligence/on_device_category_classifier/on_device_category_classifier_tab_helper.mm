// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_category_classifier_tab_helper.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

OnDeviceCategoryClassifierTabHelper::OnDeviceCategoryClassifierTabHelper(
    web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

OnDeviceCategoryClassifierTabHelper::~OnDeviceCategoryClassifierTabHelper() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

#pragma mark - web::WebStateObserver

void OnDeviceCategoryClassifierTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->HasCommitted() &&
      navigation_context->IsSameDocument()) {
    StartExtraction();
  } else {
    weak_ptr_factory_.InvalidateWeakPtrs();
    page_context_wrapper_ = nil;
  }
}

void OnDeviceCategoryClassifierTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    StartExtraction();
  }
}

void OnDeviceCategoryClassifierTabHelper::WasHidden(web::WebState* web_state) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  page_context_wrapper_ = nil;
}

void OnDeviceCategoryClassifierTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

#pragma mark - Private

void OnDeviceCategoryClassifierTabHelper::StartExtraction() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  page_context_wrapper_ = nil;

  if (web_state_->GetBrowserState()->IsOffTheRecord()) {
    return;
  }

  const GURL& url = web_state_->GetVisibleURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  base::OnceCallback<void(PageContextWrapperCallbackResponse)> callback =
      base::BindOnce(
          &OnDeviceCategoryClassifierTabHelper::OnPageContextResponse,
          weak_ptr_factory_.GetWeakPtr());

  page_context_wrapper_ =
      [[PageContextWrapper alloc] initWithWebState:web_state_
                                completionCallback:std::move(callback)];
  [page_context_wrapper_ setShouldGetInnerText:YES];
  [page_context_wrapper_ setIsLowPriorityExtraction:YES];
  [page_context_wrapper_ populatePageContextFieldsAsync];
}

void OnDeviceCategoryClassifierTabHelper::OnPageContextResponse(
    PageContextWrapperCallbackResponse response) {
  page_context_wrapper_ = nil;

  if (!response.has_value()) {
    return;
  }

  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());
  std::string page_content;
  if (page_context->has_inner_text()) {
    page_content = std::move(*page_context->mutable_inner_text());
  }

  std::string title;
  if (page_context->has_title()) {
    title = std::move(*page_context->mutable_title());
  }

  GURL url(page_context->url());
  OnPageContextExtracted(page_content, title, url);
}

void OnDeviceCategoryClassifierTabHelper::OnPageContextExtracted(
    const std::string& page_content,
    const std::string& title,
    const GURL& url) {
  if (page_content.empty() || !web_state_) {
    return;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  if (!profile) {
    return;
  }

  InProcessCategoryClassificationService* service =
      InProcessCategoryClassificationService::GetForProfile(profile);
  if (service) {
    auto callback = base::BindOnce(
        &OnDeviceCategoryClassifierTabHelper::OnCategoriesClassified,
        weak_ptr_factory_.GetWeakPtr());
    service->ClassifyPageContext(url, title, page_content, std::move(callback));
  }
}

void OnDeviceCategoryClassifierTabHelper::OnCategoriesClassified(
    const std::vector<page_content_annotations::Category>& categories) {
  // No-op stub for CL 1.
}
