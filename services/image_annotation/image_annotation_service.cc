// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_annotation/image_annotation_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace image_annotation {

namespace {

constexpr char kPixelsServerUrl[] =
    "https://ckintersect-pa.googleapis.com/v1/intersect/pixels";
constexpr char kLangsServerUrl[] =
    "https://ckintersect-pa.googleapis.com/v1/intersect/langs";
constexpr int kThrottleMs = 300;
constexpr int kBatchSize = 10;
constexpr double kMinOcrConfidence = 0.7;

}  // namespace

ImageAnnotationService::ImageAnnotationService(
    mojo::PendingReceiver<mojom::ImageAnnotationService> receiver,
    std::string api_key,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    std::unique_ptr<manta::AnchovyProvider> anchovy_provider,
    std::unique_ptr<Annotator::Client> annotator_client)
    : receiver_(this, std::move(receiver)),
      annotator_(GURL(kPixelsServerUrl),
                 GURL(kLangsServerUrl),
                 std::move(api_key),
                 base::Milliseconds(kThrottleMs),
                 kBatchSize,
                 kMinOcrConfidence,
                 shared_url_loader_factory,
                 std::move(anchovy_provider),
                 std::move(annotator_client)) {}

ImageAnnotationService::~ImageAnnotationService() = default;

void ImageAnnotationService::BindAnnotator(
    mojo::PendingReceiver<mojom::Annotator> receiver) {
  annotator_.BindReceiver(std::move(receiver));
}

}  // namespace image_annotation
