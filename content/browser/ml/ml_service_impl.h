// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ML_ML_SERVICE_IMPL_H_
#define CONTENT_BROWSER_ML_ML_SERVICE_IMPL_H_

#include "components/ml/mojom/ml_service.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

class CONTENT_EXPORT MLServiceImpl : public ml::model_loader::mojom::MLService {
 public:
  ~MLServiceImpl() override;
  // The interface to create an object, called by the ml service factory.
  static void Create(
      mojo::PendingReceiver<ml::model_loader::mojom::MLService> receiver);
  MLServiceImpl(const MLServiceImpl&) = delete;
  MLServiceImpl& operator=(const MLServiceImpl&) = delete;

 protected:
  MLServiceImpl();

 private:
  // ml::model_loader::mojom::MLService
  void CreateModelLoader(
      ml::model_loader::mojom::CreateModelLoaderOptionsPtr options,
      CreateModelLoaderCallback callback) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ML_ML_SERVICE_IMPL_H_
