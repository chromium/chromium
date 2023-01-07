// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_SERVICE_PROVIDER_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_SERVICE_PROVIDER_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/data_decoder/public/mojom/data_decoder_service.mojom.h"

namespace data_decoder {

// ServiceProvider is an API that service embedders can implement to control how
// instance of the Data Decoder service are launched.
class COMPONENT_EXPORT(DATA_DECODER_PUBLIC) ServiceProvider {
 public:
  virtual ~ServiceProvider() {}

  // Sets the global ServiceProvider instance. This must be set by the embedding
  // application prior to using the DataDecoder API in this library.
  static void Set(ServiceProvider* provider);
  static ServiceProvider* Get();

  // Binds |receiver| to an instance of the Data Decoder service.
  //
  // Implementations should assume that each call to this method is a request
  // for a new instance of the service, dedicated exclusively to |receiver|.
  //
  // In some cases like unit tests or single-process environments,
  // implementations may bind multiple receivers to the same instance.
  virtual void BindDataDecoderService(
      mojo::PendingReceiver<mojom::DataDecoderService> receiver) = 0;
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_SERVICE_PROVIDER_H_
