// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IMAGE_ANNOTATION_IMAGE_ANNOTATION_SERVICE_H_
#define SERVICES_IMAGE_ANNOTATION_IMAGE_ANNOTATION_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/image_annotation/annotator.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace image_annotation {

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
  // mojom::ImageAnnotationService implementation:
  void BindAnnotator(mojo::PendingReceiver<mojom::Annotator> receiver) override;

  mojo::Receiver<mojom::ImageAnnotationService> receiver_;
  Annotator annotator_;
};

}  // namespace image_annotation

#endif  // SERVICES_IMAGE_ANNOTATION_IMAGE_ANNOTATION_SERVICE_H_
