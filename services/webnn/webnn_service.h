// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_SERVICE_H_
#define SERVICES_WEBNN_WEBNN_SERVICE_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/webnn/public/mojom/webnn_service.mojom.h"

namespace webnn {

class WebNNService : public mojom::WebNNService {
 public:
  explicit WebNNService(mojo::PendingReceiver<mojom::WebNNService> receiver);

  WebNNService(const WebNNService&) = delete;
  WebNNService& operator=(const WebNNService&) = delete;

  ~WebNNService() override;

  void BindWebNNContextProvider(
      mojo::PendingReceiver<mojom::WebNNContextProvider> receiver) override;

 private:
  mojo::Receiver<mojom::WebNNService> receiver_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_SERVICE_H_
