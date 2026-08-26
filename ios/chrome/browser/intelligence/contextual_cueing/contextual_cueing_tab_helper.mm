// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_tab_helper.h"

#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_page_classification_service.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_page_classification_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

namespace contextual_cueing {

ContextualCueingTabHelper::ContextualCueingTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  CHECK(web_state_);
  web_state_observation_.Observe(web_state_);
}

ContextualCueingTabHelper::~ContextualCueingTabHelper() = default;

void ContextualCueingTabHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ContextualCueingTabHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const std::optional<std::vector<page_content_annotations::Category>>&
ContextualCueingTabHelper::GetCategories() const {
  return categories_;
}

#pragma mark - web::WebStateObserver

void ContextualCueingTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context->HasCommitted()) {
    return;
  }

  const GURL& new_url_without_ref =
      navigation_context->GetUrl().GetWithoutRef();
  if (new_url_without_ref == current_url_.GetWithoutRef()) {
    return;
  }
  current_url_ = navigation_context->GetUrl();

  CancelClassification();
  categories_.reset();

  if (navigation_context->IsSameDocument()) {
    StartClassification();
  }
}

void ContextualCueingTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    StartClassification();
  }
}

void ContextualCueingTabHelper::WasHidden(web::WebState* web_state) {
  CancelClassification();
}

void ContextualCueingTabHelper::WebStateDestroyed(web::WebState* web_state) {
  CancelClassification();
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

void ContextualCueingTabHelper::CancelClassification() {
  if (!web_state_) {
    return;
  }
  weak_ptr_factory_.InvalidateWeakPtrs();

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  CHECK(profile);
  if (auto* service =
          OnDevicePageClassificationServiceFactory::GetForProfile(profile)) {
    service->CancelClassification(web_state_);
  }
}

void ContextualCueingTabHelper::StartClassification() {
  if (!web_state_) {
    return;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  CHECK(profile);
  if (profile->IsOffTheRecord()) {
    return;
  }

  const GURL& url = web_state_->GetLastCommittedURL();
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  // TODO(crbug.com/549660446): Add PageClassificationService via verticals
  // here.
  if (IsGeminiContextualSuggestionsCuesOnDeviceClassifierEnabled()) {
    OnDevicePageClassificationService* service =
        OnDevicePageClassificationServiceFactory::GetForProfile(profile);
    if (!service) {
      return;
    }

    service->ClassifyWebState(
        web_state_, base::BindOnce(&ContextualCueingTabHelper::OnPageClassified,
                                   weak_ptr_factory_.GetWeakPtr(), url));
  }
}

void ContextualCueingTabHelper::OnPageClassified(
    const GURL& expected_url,
    const std::optional<std::vector<page_content_annotations::Category>>&
        categories) {
  if (!web_state_ || web_state_->GetLastCommittedURL() != expected_url) {
    return;
  }

  categories_ = categories;

  for (auto& observer : observers_) {
    observer.OnPageClassificationCompleted(web_state_, categories_);
  }
}

}  // namespace contextual_cueing
