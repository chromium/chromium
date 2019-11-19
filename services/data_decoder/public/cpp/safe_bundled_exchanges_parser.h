// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_BUNDLED_EXCHANGES_PARSER_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_BUNDLED_EXCHANGES_PARSER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/mojom/bundled_exchanges_parser.mojom.h"

namespace data_decoder {

// A class to wrap remote mojom::BundledExchangesParserFactory and
// mojom::BundledExchangesParser service.
class SafeBundledExchangesParser {
 public:
  SafeBundledExchangesParser();
  // Remaining callbacks on flight will be dropped.
  ~SafeBundledExchangesParser();

  // Binds |this| instance to the given |file| for subsequent parse calls.
  base::File::Error OpenFile(base::File file);

  // Binds |this| instance to the given |data_source| for subsequent parse
  // calls.
  void OpenDataSource(mojo::PendingRemote<mojom::BundleDataSource> data_source);

  // Parses metadata. See mojom::BundledExchangesParser::ParseMetadata for
  // details. This method fails when it's called before the previous call
  // finishes.
  void ParseMetadata(
      mojom::BundledExchangesParser::ParseMetadataCallback callback);

  // Parses response. See mojom::BundledExchangesParser::ParseResponse for
  // details.
  void ParseResponse(
      uint64_t response_offset,
      uint64_t response_length,
      mojom::BundledExchangesParser::ParseResponseCallback callback);

  // Sets alternative BundledExchangesParserFactory that will be used to create
  // BundledExchangesParser for testing purpose.
  void SetBundledExchangesParserFactoryForTesting(
      mojo::Remote<mojom::BundledExchangesParserFactory> factory);

 private:
  mojom::BundledExchangesParserFactory* GetFactory();
  void OnDisconnect();
  void OnMetadataParsed(mojom::BundleMetadataPtr metadata,
                        mojom::BundleMetadataParseErrorPtr error);
  void OnResponseParsed(size_t callback_id,
                        mojom::BundleResponsePtr response,
                        mojom::BundleResponseParseErrorPtr error);

  DataDecoder data_decoder_;
  mojo::Remote<mojom::BundledExchangesParserFactory> factory_;
  mojo::Remote<mojom::BundledExchangesParser> parser_;
  mojom::BundledExchangesParser::ParseMetadataCallback metadata_callback_;
  base::flat_map<size_t, mojom::BundledExchangesParser::ParseResponseCallback>
      response_callbacks_;
  size_t response_callback_next_id_ = 0;
  bool disconnected_ = true;

  DISALLOW_COPY_AND_ASSIGN(SafeBundledExchangesParser);
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_BUNDLED_EXCHANGES_PARSER_H_
