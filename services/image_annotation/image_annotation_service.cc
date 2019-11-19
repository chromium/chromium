// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_annotation/image_annotation_service.h"

#include <utility>

#include "base/bind.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace image_annotation {

// static
const base::Feature ImageAnnotationService::kExperiment{
    "ImageAnnotationServiceExperimental", base::FEATURE_DISABLED_BY_DEFAULT};
constexpr base::FeatureParam<std::string> ImageAnnotationService::kServerUrl;
constexpr base::FeatureParam<std::string> ImageAnnotationService::kApiKey;
constexpr base::FeatureParam<int> ImageAnnotationService::kThrottleMs;
constexpr base::FeatureParam<int> ImageAnnotationService::kBatchSize;
constexpr base::FeatureParam<double> ImageAnnotationService::kMinOcrConfidence;

ImageAnnotationService::ImageAnnotationService(
    mojo::PendingReceiver<mojom::ImageAnnotationService> receiver,
    std::string api_key,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    std::unique_ptr<Annotator::Client> annotator_client)
    : receiver_(this, std::move(receiver)),
      annotator_(GURL(kServerUrl.Get()),
                 kApiKey.Get().empty() ? std::move(api_key) : kApiKey.Get(),
                 base::TimeDelta::FromMilliseconds(kThrottleMs.Get()),
                 kBatchSize.Get(),
                 kMinOcrConfidence.Get(),
                 shared_url_loader_factory,
                 std::move(annotator_client)) {}

ImageAnnotationService::~ImageAnnotationService() = default;

void ImageAnnotationService::BindAnnotator(
    mojo::PendingReceiver<mojom::Annotator> receiver) {
  annotator_.BindReceiver(std::move(receiver));
}

}  // namespace image_annotation
