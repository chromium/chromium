// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IMAGE_ANNOTATION_IMAGE_ANNOTATION_SERVICE_H_
#define SERVICES_IMAGE_ANNOTATION_IMAGE_ANNOTATION_SERVICE_H_

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "services/image_annotation/annotator.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace image_annotation {

// Whether or not to override service parameters for experimentation.
BASE_DECLARE_FEATURE(kImageAnnotationServiceExperimental);

class ImageAnnotationService : public mojom::ImageAnnotationService {
 public:
  ImageAnnotationService(
      mojo::PendingReceiver<mojom::ImageAnnotationService> receiver,
      std::string api_key,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      std::unique_ptr<manta::AnchovyProvider> anchovy_provider,
      std::unique_ptr<Annotator::Client> annotator_client);

  ImageAnnotationService(const ImageAnnotationService&) = delete;
  ImageAnnotationService& operator=(const ImageAnnotationService&) = delete;

  ~ImageAnnotationService() override;

 private:
  // Service params:

  // The url of the service that fetches descriptions given image pixels.
  static constexpr base::FeatureParam<std::string> kPixelsServerUrl{
      &kImageAnnotationServiceExperimental, "server_url",
      "https://ckintersect-pa.googleapis.com/v1/intersect/pixels"};
  // The url of the service that returns the supported languages.
  static constexpr base::FeatureParam<std::string> kLangsServerUrl{
      &kImageAnnotationServiceExperimental, "langs_server_url",
      "https://ckintersect-pa.googleapis.com/v1/intersect/langs"};
  // An override Google API key. If empty, the API key with which the browser
  // was built (if any) will be used instead.
  static constexpr base::FeatureParam<std::string> kApiKey{
      &kImageAnnotationServiceExperimental, "api_key", ""};
  static constexpr base::FeatureParam<int> kThrottleMs{
      &kImageAnnotationServiceExperimental, "throttle_ms", 300};
  static constexpr base::FeatureParam<int> kBatchSize{
      &kImageAnnotationServiceExperimental, "batch_size", 10};
  static constexpr base::FeatureParam<double> kMinOcrConfidence{
      &kImageAnnotationServiceExperimental, "min_ocr_confidence", 0.7};

  // mojom::ImageAnnotationService implementation:
  void BindAnnotator(mojo::PendingReceiver<mojom::Annotator> receiver) override;

  mojo::Receiver<mojom::ImageAnnotationService> receiver_;
  Annotator annotator_;
};

}  // namespace image_annotation

#endif  // SERVICES_IMAGE_ANNOTATION_IMAGE_ANNOTATION_SERVICE_H_
