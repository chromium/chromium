// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/test_support/fake_data_decoder_service.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace data_decoder {

FakeDataDecoderService::FakeDataDecoderService() = default;

FakeDataDecoderService::~FakeDataDecoderService() = default;

void FakeDataDecoderService::BindImageDecoder(
    mojo::PendingReceiver<data_decoder::mojom::ImageDecoder> receiver) {
  FAIL();
}

void FakeDataDecoderService::BindJsonParser(
    mojo::PendingReceiver<data_decoder::mojom::JsonParser> receiver) {
  FAIL();
}

void FakeDataDecoderService::BindStructuredHeadersParser(
    mojo::PendingReceiver<data_decoder::mojom::StructuredHeadersParser>
        receiver) {
  FAIL();
}

void FakeDataDecoderService::BindXmlParser(
    mojo::PendingReceiver<data_decoder::mojom::XmlParser> receiver) {
  FAIL();
}

void FakeDataDecoderService::BindWebBundleParserFactory(
    mojo::PendingReceiver<web_package::mojom::WebBundleParserFactory>
        receiver) {
  FAIL();
}

void FakeDataDecoderService::BindGzipper(
    mojo::PendingReceiver<data_decoder::mojom::Gzipper> receiver) {
  FAIL();
}

void FakeDataDecoderService::BindCborParser(
    mojo::PendingReceiver<data_decoder::mojom::CborParser> receiver) {
  FAIL();
}

void FakeDataDecoderService::BindPixCodeValidator(
    mojo::PendingReceiver<payments::facilitated::mojom::PixCodeValidator>
        receiver) {
  FAIL();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void FakeDataDecoderService::BindBleScanParser(
    mojo::PendingReceiver<mojom::BleScanParser> receiver) {
  FAIL();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace data_decoder
