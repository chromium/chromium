// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_page_classification_service.h"

#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/numerics/safe_conversions.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/page_content_annotations/core/simple_page_content_verbalization.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/page_stability_monitor.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "services/metrics/public/cpp/ukm_recorder.h"

namespace {

constexpr size_t kMaxExtractedTextBytes = 10000;

}  // namespace

OnDevicePageClassificationService::Classification::Classification() = default;
OnDevicePageClassificationService::Classification::~Classification() = default;

OnDevicePageClassificationService::OnDevicePageClassificationService(
    InProcessCategoryClassificationService* in_process_classifier)
    : in_process_classifier_(in_process_classifier) {
  CHECK(in_process_classifier_);
}

OnDevicePageClassificationService::~OnDevicePageClassificationService() =
    default;

void OnDevicePageClassificationService::Shutdown() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  active_classifications_.clear();
  in_process_classifier_ = nullptr;
}

void OnDevicePageClassificationService::CancelClassification(
    web::WebState* web_state) {
  if (!web_state) {
    return;
  }
  active_classifications_.erase(web_state->GetUniqueIdentifier());
}

void OnDevicePageClassificationService::ClassifyWebState(
    web::WebState* web_state,
    PageClassificationCallback callback) {
  if (!web_state) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  if (web_state->GetBrowserState()->IsOffTheRecord()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  CancelClassification(web_state);

  const GURL& url = web_state->GetLastCommittedURL();
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  ukm::SourceId source_id = ukm::GetSourceIdForWebStateDocument(web_state);
  base::WeakPtr<web::WebState> weak_web_state = web_state->GetWeakPtr();
  uint64_t request_id = ++next_request_id_;

  if (in_process_classifier_ &&
      in_process_classifier_->HasCachedEmbeddings(url)) {
    auto classification = std::make_unique<Classification>();
    classification->request_id = request_id;
    active_classifications_[web_state->GetUniqueIdentifier()] =
        std::move(classification);
    in_process_classifier_->ClassifyWithCachedEmbeddings(
        url, source_id,
        base::BindOnce(
            &OnDevicePageClassificationService::OnCategoriesClassified,
            weak_ptr_factory_.GetWeakPtr(), weak_web_state, request_id, url,
            source_id, std::move(callback)));
    return;
  }

  if (IsGeminiContextualSuggestionsCuesTitleAndUrlOnlyEnabled()) {
    std::string title = base::UTF16ToUTF8(web_state->GetTitle());
    if (title.empty()) {
      title = std::string(url.host());
    }
    if (in_process_classifier_) {
      auto classification = std::make_unique<Classification>();
      classification->request_id = request_id;
      active_classifications_[web_state->GetUniqueIdentifier()] =
          std::move(classification);
      in_process_classifier_->ClassifyPageContext(
          url, title, /*page_content=*/"", source_id,
          base::BindOnce(
              &OnDevicePageClassificationService::OnCategoriesClassified,
              weak_ptr_factory_.GetWeakPtr(), weak_web_state, request_id, url,
              source_id, std::move(callback)));
    } else {
      std::move(callback).Run(std::nullopt);
    }
    return;
  }

  web::WebFrame* main_frame = nullptr;
  if (web_state->GetPageWorldWebFramesManager()) {
    main_frame = web_state->GetPageWorldWebFramesManager()->GetMainWebFrame();
  }
  if (!main_frame) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto classification = std::make_unique<Classification>();
  classification->request_id = request_id;

  if (IsPageStabilityEnabled()) {
    auto stability_monitor =
        std::make_unique<actor::PageStabilityMonitor>(main_frame->AsWeakPtr());
    auto* monitor_ptr = stability_monitor.get();
    classification->stability_monitor = std::move(stability_monitor);
    active_classifications_[web_state->GetUniqueIdentifier()] =
        std::move(classification);
    monitor_ptr->NotifyWhenStable(
        base::TimeDelta(),
        base::BindOnce(
            &OnDevicePageClassificationService::ExtractPageContextAndClassify,
            weak_ptr_factory_.GetWeakPtr(), weak_web_state, request_id, url,
            source_id, std::move(callback)));
  } else {
    active_classifications_[web_state->GetUniqueIdentifier()] =
        std::move(classification);
    ExtractPageContextAndClassify(weak_web_state, request_id, url, source_id,
                                  std::move(callback));
  }
}

void OnDevicePageClassificationService::ExtractPageContextAndClassify(
    base::WeakPtr<web::WebState> web_state,
    uint64_t request_id,
    const GURL& expected_url,
    ukm::SourceId source_id,
    PageClassificationCallback callback) {
  if (!web_state) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (web_state->GetLastCommittedURL() != expected_url) {
    CancelClassification(web_state.get());
    std::move(callback).Run(std::nullopt);
    return;
  }

  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  auto it = active_classifications_.find(web_state_id);
  if (it == active_classifications_.end() ||
      it->second->request_id != request_id) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  it->second->stability_monitor.reset();

  PageContextWrapper* wrapper = [[PageContextWrapper alloc]
        initWithWebState:web_state.get()
      completionCallback:
          base::BindOnce(
              &OnDevicePageClassificationService::OnPageContextExtracted,
              weak_ptr_factory_.GetWeakPtr(), web_state, request_id,
              expected_url, source_id, std::move(callback))];
  wrapper.isLowPriorityExtraction = YES;
  wrapper.shouldGetAnnotatedPageContent = YES;
  wrapper.shouldGetInnerText = YES;
  it->second->page_context_wrapper = wrapper;
  [wrapper populatePageContextFieldsAsync];
}

void OnDevicePageClassificationService::OnPageContextExtracted(
    base::WeakPtr<web::WebState> web_state,
    uint64_t request_id,
    const GURL& expected_url,
    ukm::SourceId source_id,
    PageClassificationCallback callback,
    PageContextWrapperCallbackResponse response) {
  if (!web_state) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (web_state->GetLastCommittedURL() != expected_url) {
    CancelClassification(web_state.get());
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto it = active_classifications_.find(web_state->GetUniqueIdentifier());
  if (it == active_classifications_.end() ||
      it->second->request_id != request_id) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  it->second->page_context_wrapper = nil;

  if (!response.has_value() || !response.value()) {
    active_classifications_.erase(it);
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response).value();
  std::string extracted_text;
  if (page_context->has_annotated_page_content() &&
      page_context->annotated_page_content().has_root_node()) {
    std::vector<std::string> text_pieces;
    page_content_annotations::CollectTextForContentNodesRecursively(
        page_context->annotated_page_content().root_node(), text_pieces);
    extracted_text = base::JoinString(text_pieces, " ");
  }
  if (extracted_text.empty() && page_context->has_inner_text()) {
    extracted_text = page_context->inner_text();
  }

  if (extracted_text.empty()) {
    active_classifications_.erase(it);
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (extracted_text.length() > kMaxExtractedTextBytes) {
    extracted_text =
        base::TruncateUTF8ToByteSize(extracted_text, kMaxExtractedTextBytes);
  }

  std::string title;
  if (page_context->has_title()) {
    title = page_context->title();
  }

  if (!in_process_classifier_) {
    active_classifications_.erase(it);
    std::move(callback).Run(std::nullopt);
    return;
  }

  in_process_classifier_->ClassifyPageContext(
      expected_url, title, extracted_text, source_id,
      base::BindOnce(&OnDevicePageClassificationService::OnCategoriesClassified,
                     weak_ptr_factory_.GetWeakPtr(), web_state, request_id,
                     expected_url, source_id, std::move(callback)));
}

void OnDevicePageClassificationService::OnCategoriesClassified(
    base::WeakPtr<web::WebState> web_state,
    uint64_t request_id,
    const GURL& expected_url,
    ukm::SourceId source_id,
    PageClassificationCallback callback,
    const std::vector<page_content_annotations::Category>& categories) {
  if (!web_state) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (web_state->GetLastCommittedURL() != expected_url) {
    CancelClassification(web_state.get());
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto it = active_classifications_.find(web_state->GetUniqueIdentifier());
  if (it == active_classifications_.end() ||
      it->second->request_id != request_id) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  active_classifications_.erase(it);

  ukm::builders::PageContentAnnotations2 builder(source_id);
  bool has_ukm = false;

  for (const page_content_annotations::Category& category : categories) {
    int64_t score = base::ClampRound(category.score * 100);
    int64_t noisy_score =
        page_content_annotations::GenerateRapporNoisedScore(category.score);
    switch (category.category_type) {
      case page_content_annotations::CategoryType::kEducation:
        base::UmaHistogramPercentage(
            "OptimizationGuide.PageContentAnnotations.CategoryClassifier."
            "EducationScore",
            score);
        builder.SetCategoryClassifier_EducationScore(noisy_score);
        has_ukm = true;
        break;
      case page_content_annotations::CategoryType::kShopping:
        base::UmaHistogramPercentage(
            "OptimizationGuide.PageContentAnnotations.CategoryClassifier."
            "ShoppingScore",
            score);
        builder.SetCategoryClassifier_ShoppingScore(noisy_score);
        has_ukm = true;
        break;
      default:
        break;
    }
  }

  if (has_ukm) {
    builder.Record(ukm::UkmRecorder::Get());
  }

  std::move(callback).Run(categories);
}
