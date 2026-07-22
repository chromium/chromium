// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_CATEGORY_CLASSIFICATION_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_CATEGORY_CLASSIFICATION_SERVICE_H_

#include <string>
#include <vector>

#import "base/functional/callback.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/page_content_annotations/core/page_content_annotations_common.h"
#import "url/gurl.h"

class ProfileIOS;

// In-process service that manages passage embedding generation and on-device
// category classification.
class InProcessCategoryClassificationService : public KeyedService {
 public:
  using ClassificationCallback = base::OnceCallback<void(
      const std::vector<page_content_annotations::Category>&)>;

  static InProcessCategoryClassificationService* GetForProfile(
      ProfileIOS* profile);
  static void EnsureFactoryBuilt();

  InProcessCategoryClassificationService();
  ~InProcessCategoryClassificationService() override;

  InProcessCategoryClassificationService(
      const InProcessCategoryClassificationService&) = delete;
  InProcessCategoryClassificationService& operator=(
      const InProcessCategoryClassificationService&) = delete;

  // Classifies the extracted page context using local models.
  virtual void ClassifyPageContext(const GURL& url,
                                   const std::string& title,
                                   const std::string& page_content,
                                   ClassificationCallback callback);
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_CATEGORY_CLASSIFICATION_SERVICE_H_
