// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_BUNDLED_EXCHANGES_PARSER_FACTORY_H_
#define SERVICES_DATA_DECODER_BUNDLED_EXCHANGES_PARSER_FACTORY_H_

#include <memory>

#include "base/files/file.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/mojom/bundled_exchanges_parser.mojom.h"

namespace data_decoder {

class BundledExchangesParserFactory
    : public mojom::BundledExchangesParserFactory {
 public:
  BundledExchangesParserFactory();
  ~BundledExchangesParserFactory() override;

  std::unique_ptr<mojom::BundleDataSource> CreateFileDataSourceForTesting(
      mojo::PendingReceiver<mojom::BundleDataSource> receiver,
      base::File file);

 private:
  // mojom::BundledExchangesParserFactory implementation.
  void GetParserForFile(
      mojo::PendingReceiver<mojom::BundledExchangesParser> receiver,
      base::File file) override;
  void GetParserForDataSource(
      mojo::PendingReceiver<mojom::BundledExchangesParser> receiver,
      mojo::PendingRemote<mojom::BundleDataSource> data_source) override;

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesParserFactory);
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_BUNDLED_EXCHANGES_PARSER_FACTORY_H_
