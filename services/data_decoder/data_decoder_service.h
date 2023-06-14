// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_DATA_DECODER_SERVICE_H_
#define SERVICES_DATA_DECODER_DATA_DECODER_SERVICE_H_

#include "build/chromeos_buildflags.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/data_decoder/public/mojom/cbor_parser.mojom.h"
#include "services/data_decoder/public/mojom/data_decoder_service.mojom.h"
#include "services/data_decoder/public/mojom/gzipper.mojom.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "services/data_decoder/public/mojom/structured_headers_parser.mojom.h"
#include "services/data_decoder/public/mojom/xml_parser.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "services/data_decoder/public/mojom/ble_scan_parser.mojom.h"
#endif

namespace data_decoder {

class DataDecoderService : public mojom::DataDecoderService {
 public:
  DataDecoderService();
  explicit DataDecoderService(
      mojo::PendingReceiver<mojom::DataDecoderService> receiver);

  DataDecoderService(const DataDecoderService&) = delete;
  DataDecoderService& operator=(const DataDecoderService&) = delete;

  ~DataDecoderService() override;

  // May be used to establish a latent DataDecoderService binding for this
  // instance. May only be called once, and only if this instance was default-
  // constructed.
  void BindReceiver(mojo::PendingReceiver<mojom::DataDecoderService> receiver);

  // Configures the service to drop ImageDecoder receivers instead of binding
  // them. Useful for tests simulating service failures.
  void SimulateImageDecoderCrashForTesting(bool drop) {
    drop_image_decoders_ = drop;
  }

  // Same as above but for JsonParser receivers.
  void SimulateJsonParserCrashForTesting(bool drop) {
    drop_json_parsers_ = drop;
  }

  // Configures the service to use |binder| to bind
  // WebBundleParserFactory in subsequent
  // BindWebBundleParserFactory() calls.
  void SetWebBundleParserFactoryBinderForTesting(
      base::RepeatingCallback<void(
          mojo::PendingReceiver<web_package::mojom::WebBundleParserFactory>)>
          binder) {
    web_bundle_parser_factory_binder_ = binder;
  }

 private:
  // mojom::DataDecoderService implementation:
  void BindImageDecoder(
      mojo::PendingReceiver<mojom::ImageDecoder> receiver) override;
  void BindJsonParser(
      mojo::PendingReceiver<mojom::JsonParser> receiver) override;
  void BindStructuredHeadersParser(
      mojo::PendingReceiver<mojom::StructuredHeadersParser> receiver) override;
  void BindXmlParser(mojo::PendingReceiver<mojom::XmlParser> receiver) override;
  void BindWebBundleParserFactory(
      mojo::PendingReceiver<web_package::mojom::WebBundleParserFactory>
          receiver) override;
  void BindGzipper(mojo::PendingReceiver<mojom::Gzipper> receiver) override;
  void BindCborParser(
      mojo::PendingReceiver<mojom::CborParser> receiver) override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void BindBleScanParser(
      mojo::PendingReceiver<mojom::BleScanParser> receiver) override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // In-process instances (e.g. on iOS or in tests) may have multiple concurrent
  // remote DataDecoderService clients.
  mojo::ReceiverSet<mojom::DataDecoderService> receivers_;

  bool drop_image_decoders_ = false;
  bool drop_json_parsers_ = false;
  base::RepeatingCallback<void(
      mojo::PendingReceiver<web_package::mojom::WebBundleParserFactory>)>
      web_bundle_parser_factory_binder_;
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_DATA_DECODER_SERVICE_H_
