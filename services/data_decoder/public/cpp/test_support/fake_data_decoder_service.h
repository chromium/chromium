// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_TEST_SUPPORT_FAKE_DATA_DECODER_SERVICE_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_TEST_SUPPORT_FAKE_DATA_DECODER_SERVICE_H_

#include "build/chromeos_buildflags.h"
#include "components/facilitated_payments/core/mojom/pix_code_validator.mojom.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
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

// A test data decoder service implementation that will fail the test if any of
// its methods are called. Extend this class to use it in a test.
class FakeDataDecoderService : public mojom::DataDecoderService {
 public:
  FakeDataDecoderService();
  FakeDataDecoderService(const FakeDataDecoderService&) = delete;
  FakeDataDecoderService& operator=(const FakeDataDecoderService&) = delete;
  ~FakeDataDecoderService() override;

  // data_decoder::mojom::DataDecoderService:
  void BindImageDecoder(mojo::PendingReceiver<data_decoder::mojom::ImageDecoder>
                            receiver) override;
  void BindJsonParser(mojo::PendingReceiver<data_decoder::mojom::JsonParser>
                          receiver) override;
  void BindStructuredHeadersParser(
      mojo::PendingReceiver<data_decoder::mojom::StructuredHeadersParser>
          receiver) override;
  void BindXmlParser(
      mojo::PendingReceiver<data_decoder::mojom::XmlParser> receiver) override;
  void BindWebBundleParserFactory(
      mojo::PendingReceiver<web_package::mojom::WebBundleParserFactory>
          receiver) override;
  void BindGzipper(
      mojo::PendingReceiver<data_decoder::mojom::Gzipper> receiver) override;
  void BindCborParser(mojo::PendingReceiver<data_decoder::mojom::CborParser>
                          receiver) override;
  void BindPixCodeValidator(
      mojo::PendingReceiver<payments::facilitated::mojom::PixCodeValidator>
          receiver) override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void BindBleScanParser(
      mojo::PendingReceiver<mojom::BleScanParser> receiver) override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_TEST_SUPPORT_FAKE_DATA_DECODER_SERVICE_H_
