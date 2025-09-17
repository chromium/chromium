// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/data_decoder_service.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "components/facilitated_payments/core/validation/pix_code_validator.h"
#include "components/web_package/web_bundle_parser_factory.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/data_decoder/cbor_parser_impl.h"
#include "services/data_decoder/gzipper.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "services/data_decoder/structured_headers_parser_impl.h"
#include "services/data_decoder/xml_parser.h"

#if BUILDFLAG(USE_BLINK)
#include "services/data_decoder/image_decoder_impl.h"
#endif

namespace data_decoder {

DataDecoderService::DataDecoderService() = default;

DataDecoderService::DataDecoderService(
    mojo::PendingReceiver<mojom::DataDecoderService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

DataDecoderService::~DataDecoderService() = default;

void DataDecoderService::BindReceiver(
    mojo::PendingReceiver<mojom::DataDecoderService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DataDecoderService::BindImageDecoder(
    mojo::PendingReceiver<mojom::ImageDecoder> receiver) {
#if !BUILDFLAG(USE_BLINK)
  LOG(FATAL) << "ImageDecoder not supported on non-Blink platforms.";
#else
  mojo::MakeSelfOwnedReceiver(std::make_unique<ImageDecoderImpl>(),
                              std::move(receiver));
#endif
}

void DataDecoderService::BindStructuredHeadersParser(
    mojo::PendingReceiver<mojom::StructuredHeadersParser> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<StructuredHeadersParserImpl>(),
                              std::move(receiver));
}

void DataDecoderService::BindXmlParser(
    mojo::PendingReceiver<mojom::XmlParser> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<XmlParser>(),
                              std::move(receiver));
}

void DataDecoderService::BindWebBundleParserFactory(
    mojo::PendingReceiver<web_package::mojom::WebBundleParserFactory>
        receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<web_package::WebBundleParserFactory>(),
      std::move(receiver));
}

void DataDecoderService::BindGzipper(
    mojo::PendingReceiver<mojom::Gzipper> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<Gzipper>(), std::move(receiver));
}

void DataDecoderService::BindCborParser(
    mojo::PendingReceiver<mojom::CborParser> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<CborParserImpl>(),
                              std::move(receiver));
}

void DataDecoderService::BindPixCodeValidator(
    mojo::PendingReceiver<payments::facilitated::mojom::PixCodeValidator>
        receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<payments::facilitated::PixCodeValidator>(),
      std::move(receiver));
}

}  // namespace data_decoder
