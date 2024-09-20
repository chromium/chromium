// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_TEST_SUPPORT_IN_PROCESS_DATA_DECODER_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_TEST_SUPPORT_IN_PROCESS_DATA_DECODER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/data_decoder/data_decoder_service.h"
#include "services/data_decoder/public/cpp/service_provider.h"
#include "services/data_decoder/public/mojom/data_decoder_service.mojom-test-utils.h"
#include "services/data_decoder/public/mojom/data_decoder_service.mojom.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"

namespace data_decoder {
namespace test {

// As long as an object of this type exists, attempts to launch the Data Decoder
// service will route to an internal, in-process instance. Only used for tests
// to avoid the complexity and dependencies of a multiprocess environment.
class InProcessDataDecoder
    : public ServiceProvider,
      public mojom::DataDecoderServiceInterceptorForTesting {
 public:
  InProcessDataDecoder();

  InProcessDataDecoder(const InProcessDataDecoder&) = delete;
  InProcessDataDecoder& operator=(const InProcessDataDecoder&) = delete;

  ~InProcessDataDecoder() override;

  const mojo::ReceiverSet<mojom::DataDecoderService>& receivers() const {
    return receivers_;
  }

  // Configures the service to drop ImageDecoder receivers instead of binding
  // them. Useful for tests simulating service failures.
  void SimulateImageDecoderCrash(bool drop) { drop_image_decoders_ = drop; }

  // Same as above but for JsonParser receivers.
  void SimulateJsonParserCrash(bool drop);

  // Configures the service to use |binder| to bind
  // WebBundleParserFactory in subsequent
  // BindWebBundleParserFactory() calls.
  void SetWebBundleParserFactoryBinder(
      base::RepeatingCallback<void(
          mojo::PendingReceiver<web_package::mojom::WebBundleParserFactory>)>
          binder) {
    web_bundle_parser_factory_binder_ = binder;
  }

 private:
  // ServiceProvider implementation:
  void BindDataDecoderService(
      mojo::PendingReceiver<mojom::DataDecoderService> receiver) override;

  // mojom::DataDecoderServiceInterceptorForTesting implementation:
  mojom::DataDecoderService* GetForwardingInterface() override;
  void BindImageDecoder(
      mojo::PendingReceiver<mojom::ImageDecoder> receiver) override;
  void BindJsonParser(
      mojo::PendingReceiver<mojom::JsonParser> receiver) override;
  void BindWebBundleParserFactory(
      mojo::PendingReceiver<web_package::mojom::WebBundleParserFactory>
          receiver) override;

  // Optionally allows subclasses to specify a custom `ImageDecoder`
  // implementation. If not overridden, the image decoder implementation in
  // `DataDecoderService` is used.
  virtual std::unique_ptr<data_decoder::mojom::ImageDecoder>
  CreateCustomImageDecoder();

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  ::data_decoder::DataDecoderService service_;
  mojo::ReceiverSet<mojom::DataDecoderService> receivers_;
  bool drop_image_decoders_ = false;
  bool drop_json_parsers_ = false;
  base::RepeatingCallback<void(
      mojo::PendingReceiver<web_package::mojom::WebBundleParserFactory>)>
      web_bundle_parser_factory_binder_;
  base::WeakPtrFactory<InProcessDataDecoder> weak_ptr_factory_{this};
};

}  // namespace test
}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_TEST_SUPPORT_IN_PROCESS_DATA_DECODER_H_
