// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_ON_DEVICE_PAGE_CLASSIFICATION_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_ON_DEVICE_PAGE_CLASSIFICATION_SERVICE_H_

#import <memory>
#import <optional>
#import <string>
#import <vector>

#import "base/containers/flat_map.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/page_content_annotations/core/page_content_annotations_common.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"
#import "services/metrics/public/cpp/ukm_source_id.h"
#import "url/gurl.h"

namespace actor {
class PageStabilityMonitor;
}  // namespace actor

class InProcessCategoryClassificationService;
@class PageContextWrapper;

// Profile-scoped service that coordinates on-device page classification for
// WebStates on iOS. It handles layout stability checks, DOM text extraction via
// PageContextWrapper, metrics logging, and delegates embedding and category
// classification to InProcessCategoryClassificationService.
class OnDevicePageClassificationService : public KeyedService {
 public:
  using PageClassificationCallback = base::OnceCallback<void(
      const std::optional<std::vector<page_content_annotations::Category>>&
          categories)>;

  explicit OnDevicePageClassificationService(
      InProcessCategoryClassificationService* in_process_classifier);
  ~OnDevicePageClassificationService() override;

  OnDevicePageClassificationService(const OnDevicePageClassificationService&) =
      delete;
  OnDevicePageClassificationService& operator=(
      const OnDevicePageClassificationService&) = delete;

  // KeyedService:
  void Shutdown() override;

  // Classifies the content of a WebState asynchronously.
  virtual void ClassifyWebState(web::WebState* web_state,
                                PageClassificationCallback callback);

  // Cancels any in-flight extraction or stability monitoring for `web_state`.
  virtual void CancelClassification(web::WebState* web_state);

 private:
  struct Classification {
    Classification();
    ~Classification();
    Classification(const Classification&) = delete;
    Classification& operator=(const Classification&) = delete;

    uint64_t request_id = 0;
    __strong PageContextWrapper* page_context_wrapper = nil;
    std::unique_ptr<actor::PageStabilityMonitor> stability_monitor;
  };

  void ExtractPageContextAndClassify(base::WeakPtr<web::WebState> web_state,
                                     uint64_t request_id,
                                     const GURL& expected_url,
                                     ukm::SourceId source_id,
                                     PageClassificationCallback callback);

  void OnPageContextExtracted(base::WeakPtr<web::WebState> web_state,
                              uint64_t request_id,
                              const GURL& expected_url,
                              ukm::SourceId source_id,
                              PageClassificationCallback callback,
                              PageContextWrapperCallbackResponse response);

  void OnCategoriesClassified(
      base::WeakPtr<web::WebState> web_state,
      uint64_t request_id,
      const GURL& expected_url,
      ukm::SourceId source_id,
      PageClassificationCallback callback,
      const std::vector<page_content_annotations::Category>& categories);

  raw_ptr<InProcessCategoryClassificationService> in_process_classifier_ =
      nullptr;
  uint64_t next_request_id_ = 0;
  base::flat_map<web::WebStateID, std::unique_ptr<Classification>>
      active_classifications_;

  base::WeakPtrFactory<OnDevicePageClassificationService> weak_ptr_factory_{
      this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_ON_DEVICE_PAGE_CLASSIFICATION_SERVICE_H_
