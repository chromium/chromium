// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_category_classifier_tab_helper.h"

#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/page_content_annotations/core/simple_page_content_verbalization.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/page_stability_monitor.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

namespace {

// Maximum number of bytes of page content text to pass to classifier.
constexpr size_t kMaxExtractedTextBytes = 10000;

}  // namespace

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
    page_stability_monitor_.reset();
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
  page_stability_monitor_.reset();

  if (web_state_->GetBrowserState()->IsOffTheRecord()) {
    return;
  }

  const GURL& url = web_state_->GetVisibleURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  web::WebFramesManager* frames_manager =
      web_state_->GetPageWorldWebFramesManager();
  web::WebFrame* main_frame =
      frames_manager ? frames_manager->GetMainWebFrame() : nullptr;

  if (!main_frame) {
    return;
  }

  if (IsPageStabilityEnabled()) {
    page_stability_monitor_ =
        std::make_unique<actor::PageStabilityMonitor>(main_frame->AsWeakPtr());
    page_stability_monitor_->NotifyWhenStable(
        /*observation_delay=*/base::TimeDelta(),
        base::BindOnce(&OnDeviceCategoryClassifierTabHelper::ExtractPageContext,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    ExtractPageContext();
  }
}

void OnDeviceCategoryClassifierTabHelper::ExtractPageContext() {
  base::OnceCallback<void(PageContextWrapperCallbackResponse)> callback =
      base::BindOnce(
          &OnDeviceCategoryClassifierTabHelper::OnPageContextResponse,
          weak_ptr_factory_.GetWeakPtr());

  page_context_wrapper_ =
      [[PageContextWrapper alloc] initWithWebState:web_state_
                                completionCallback:std::move(callback)];
  [page_context_wrapper_ setShouldGetAnnotatedPageContent:YES];
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
  std::string extracted_text;

  if (page_context->has_annotated_page_content() &&
      page_context->annotated_page_content().has_root_node()) {
    std::vector<std::string> text;
    page_content_annotations::CollectTextForContentNodesRecursively(
        page_context->annotated_page_content().root_node(), text);
    extracted_text = base::JoinString(text, " ");
  }

  if (extracted_text.empty() && page_context->has_inner_text()) {
    extracted_text = page_context->inner_text();
  }

  if (extracted_text.length() > kMaxExtractedTextBytes) {
    extracted_text =
        base::TruncateUTF8ToByteSize(extracted_text, kMaxExtractedTextBytes);
  }

  std::string title;
  if (page_context->has_title()) {
    title = std::move(*page_context->mutable_title());
  }

  GURL url(page_context->url());
  OnPageContextExtracted(extracted_text, title, url);
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
    ukm::SourceId source_id = ukm::GetSourceIdForWebStateDocument(web_state_);
    auto callback = base::BindOnce(
        &OnDeviceCategoryClassifierTabHelper::OnCategoriesClassified,
        weak_ptr_factory_.GetWeakPtr(), source_id);
    service->ClassifyPageContext(url, title, page_content, source_id,
                                 std::move(callback));
  }
}

void OnDeviceCategoryClassifierTabHelper::OnCategoriesClassified(
    ukm::SourceId source_id,
    const std::vector<page_content_annotations::Category>& categories) {
  // Stub for CL 2.
}
