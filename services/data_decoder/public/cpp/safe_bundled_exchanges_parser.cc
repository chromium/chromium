// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/safe_bundled_exchanges_parser.h"

#include "base/bind.h"

namespace data_decoder {

namespace {
constexpr char kConnectionError[] =
    "Cannot connect to the remote parser service";
}  // namespace

SafeBundledExchangesParser::SafeBundledExchangesParser() = default;

SafeBundledExchangesParser::~SafeBundledExchangesParser() = default;

base::File::Error SafeBundledExchangesParser::OpenFile(base::File file) {
  DCHECK(disconnected_);

  if (!file.IsValid())
    return file.error_details();

  GetFactory()->GetParserForFile(parser_.BindNewPipeAndPassReceiver(),
                                 std::move(file));
  parser_.set_disconnect_handler(base::BindOnce(
      &SafeBundledExchangesParser::OnDisconnect, base::Unretained(this)));

  disconnected_ = false;

  return base::File::FILE_OK;
}

void SafeBundledExchangesParser::OpenDataSource(
    mojo::PendingRemote<mojom::BundleDataSource> data_source) {
  DCHECK(disconnected_);
  GetFactory()->GetParserForDataSource(parser_.BindNewPipeAndPassReceiver(),
                                       std::move(data_source));
  parser_.set_disconnect_handler(base::BindOnce(
      &SafeBundledExchangesParser::OnDisconnect, base::Unretained(this)));

  disconnected_ = false;
}

void SafeBundledExchangesParser::ParseMetadata(
    mojom::BundledExchangesParser::ParseMetadataCallback callback) {
  // This method is designed to be called once. So, allowing only once
  // simultaneous request is fine enough.
  if (disconnected_ || !metadata_callback_.is_null()) {
    std::move(callback).Run(
        nullptr, mojom::BundleMetadataParseError::New(
                     mojom::BundleParseErrorType::kParserInternalError,
                     GURL() /* fallback_url */, kConnectionError));
    return;
  }
  metadata_callback_ = std::move(callback);
  parser_->ParseMetadata(base::BindOnce(
      &SafeBundledExchangesParser::OnMetadataParsed, base::Unretained(this)));
}

void SafeBundledExchangesParser::ParseResponse(
    uint64_t response_offset,
    uint64_t response_length,
    mojom::BundledExchangesParser::ParseResponseCallback callback) {
  // This method allows simultaneous multiple requests. But if the unique ID
  // overflows, and the previous request that owns the same ID hasn't finished,
  // we just make the new request fail with kConnectionError.
  if (disconnected_ ||
      response_callbacks_.contains(response_callback_next_id_)) {
    std::move(callback).Run(
        nullptr, mojom::BundleResponseParseError::New(
                     mojom::BundleParseErrorType::kParserInternalError,
                     kConnectionError));
    return;
  }
  size_t callback_id = response_callback_next_id_++;
  response_callbacks_[callback_id] = std::move(callback);
  parser_->ParseResponse(
      response_offset, response_length,
      base::BindOnce(&SafeBundledExchangesParser::OnResponseParsed,
                     base::Unretained(this), callback_id));
}

mojom::BundledExchangesParserFactory* SafeBundledExchangesParser::GetFactory() {
  if (!factory_) {
    data_decoder_.GetService()->BindBundledExchangesParserFactory(
        factory_.BindNewPipeAndPassReceiver());
    factory_.reset_on_disconnect();
  }
  return factory_.get();
}

void SafeBundledExchangesParser::OnDisconnect() {
  disconnected_ = true;
  if (!metadata_callback_.is_null())
    std::move(metadata_callback_)
        .Run(nullptr, mojom::BundleMetadataParseError::New(
                          mojom::BundleParseErrorType::kParserInternalError,
                          GURL() /* fallback_url */, kConnectionError));
  for (auto& callback : response_callbacks_)
    std::move(callback.second)
        .Run(nullptr, mojom::BundleResponseParseError::New(
                          mojom::BundleParseErrorType::kParserInternalError,
                          kConnectionError));
  response_callbacks_.clear();
}

void SafeBundledExchangesParser::OnMetadataParsed(
    mojom::BundleMetadataPtr metadata,
    mojom::BundleMetadataParseErrorPtr error) {
  DCHECK(!metadata_callback_.is_null());
  std::move(metadata_callback_).Run(std::move(metadata), std::move(error));
}

void SafeBundledExchangesParser::OnResponseParsed(
    size_t callback_id,
    mojom::BundleResponsePtr response,
    mojom::BundleResponseParseErrorPtr error) {
  auto it = response_callbacks_.find(callback_id);
  DCHECK(it != response_callbacks_.end());
  auto callback = std::move(it->second);
  response_callbacks_.erase(it);
  std::move(callback).Run(std::move(response), std::move(error));
}

void SafeBundledExchangesParser::SetBundledExchangesParserFactoryForTesting(
    mojo::Remote<mojom::BundledExchangesParserFactory> factory) {
  factory_ = std::move(factory);
}

}  // namespace data_decoder
