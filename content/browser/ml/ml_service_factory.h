// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ML_ML_SERVICE_FACTORY_H_
#define CONTENT_BROWSER_ML_ML_SERVICE_FACTORY_H_

#include "components/ml/mojom/ml_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

// A simple API used by browser interface broker. The main motivation is to
// expose an identical `Create()` interface over different platforms.
void CreateMLService(mojo::PendingReceiver<ml::model_loader::mojom::MLService>);

}  // namespace content

#endif  // CONTENT_BROWSER_ML_ML_SERVICE_FACTORY_H_
