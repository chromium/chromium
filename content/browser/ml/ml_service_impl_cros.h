// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ML_ML_SERVICE_IMPL_CROS_H_
#define CONTENT_BROWSER_ML_ML_SERVICE_IMPL_CROS_H_

#include "components/ml/mojom/ml_service.mojom-forward.h"
#include "content/browser/ml/ml_service_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

class CONTENT_EXPORT CrOSMLServiceImpl
    : public ml::model_loader::mojom::MLService {
 public:
  ~CrOSMLServiceImpl() override;
  // The interface to create an `CrOSMLServiceImpl` object and bind the mojo
  // receiver, called by the ml service factory.
  static void Create(
      mojo::PendingReceiver<ml::model_loader::mojom::MLService> receiver);

  CrOSMLServiceImpl(const CrOSMLServiceImpl&) = delete;
  MLServiceImpl& operator=(const CrOSMLServiceImpl&) = delete;

 protected:
  CrOSMLServiceImpl();

 private:
  // ml::model_loader::mojom::MLService
  void CreateModelLoader(
      ml::model_loader::mojom::CreateModelLoaderOptionsPtr options,
      CreateModelLoaderCallback callback) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ML_ML_SERVICE_IMPL_CROS_H_
