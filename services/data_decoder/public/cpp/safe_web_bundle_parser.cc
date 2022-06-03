// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/safe_web_bundle_parser.h"

#include "base/bind.h"

namespace data_decoder {

namespace {
constexpr char kConnectionError[] =
    "Cannot connect to the remote parser service";
}  // namespace

SafeWebBundleParser::SafeWebBundleParser() = default;

SafeWebBundleParser::~SafeWebBundleParser() = default;

base::File::Error SafeWebBundleParser::OpenFile(base::File file) {
  DCHECK(disconnected_);

  if (!file.IsValid())
    return file.error_details();

  GetFactory()->GetParserForFile(parser_.BindNewPipeAndPassReceiver(),
                                 std::move(file));
  parser_.set_disconnect_handler(base::BindOnce(
      &SafeWebBundleParser::OnDisconnect, base::Unretained(this)));

  disconnected_ = false;

  return base::File::FILE_OK;
}

void SafeWebBundleParser::OpenDataSource(
    mojo::PendingRemote<web_package::mojom::BundleDataSource> data_source) {
  DCHECK(disconnected_);
  GetFactory()->GetParserForDataSource(parser_.BindNewPipeAndPassReceiver(),
                                       std::move(data_source));
  parser_.set_disconnect_handler(base::BindOnce(
      &SafeWebBundleParser::OnDisconnect, base::Unretained(this)));

  disconnected_ = false;
}

void SafeWebBundleParser::ParseMetadata(
    web_package::mojom::WebBundleParser::ParseMetadataCallback callback) {
  // This method is designed to be called once. So, allowing only once
  // simultaneous request is fine enough.
  if (disconnected_ || !metadata_callback_.is_null()) {
    std::move(callback).Run(
        nullptr,
        web_package::mojom::BundleMetadataParseError::New(
            web_package::mojom::BundleParseErrorType::kParserInternalError,
            GURL() /* fallback_url */, kConnectionError));
    return;
  }
  metadata_callback_ = std::move(callback);
  parser_->ParseMetadata(base::BindOnce(&SafeWebBundleParser::OnMetadataParsed,
                                        base::Unretained(this)));
}

void SafeWebBundleParser::ParseResponse(
    uint64_t response_offset,
    uint64_t response_length,
    web_package::mojom::WebBundleParser::ParseResponseCallback callback) {
  // This method allows simultaneous multiple requests. But if the unique ID
  // overflows, and the previous request that owns the same ID hasn't finished,
  // we just make the new request fail with kConnectionError.
  if (disconnected_ ||
      response_callbacks_.contains(response_callback_next_id_)) {
    std::move(callback).Run(
        nullptr,
        web_package::mojom::BundleResponseParseError::New(
            web_package::mojom::BundleParseErrorType::kParserInternalError,
            kConnectionError));
    return;
  }
  size_t callback_id = response_callback_next_id_++;
  response_callbacks_[callback_id] = std::move(callback);
  parser_->ParseResponse(response_offset, response_length,
                         base::BindOnce(&SafeWebBundleParser::OnResponseParsed,
                                        base::Unretained(this), callback_id));
}

web_package::mojom::WebBundleParserFactory* SafeWebBundleParser::GetFactory() {
  if (!factory_) {
    data_decoder_.GetService()->BindWebBundleParserFactory(
        factory_.BindNewPipeAndPassReceiver());
    factory_.reset_on_disconnect();
  }
  return factory_.get();
}

void SafeWebBundleParser::SetDisconnectCallback(base::OnceClosure callback) {
  DCHECK(!disconnect_callback_);
  disconnect_callback_ = std::move(callback);
}

void SafeWebBundleParser::OnDisconnect() {
  disconnected_ = true;
  if (!metadata_callback_.is_null())
    std::move(metadata_callback_)
        .Run(nullptr,
             web_package::mojom::BundleMetadataParseError::New(
                 web_package::mojom::BundleParseErrorType::kParserInternalError,
                 GURL() /* fallback_url */, kConnectionError));
  for (auto& callback : response_callbacks_)
    std::move(callback.second)
        .Run(nullptr,
             web_package::mojom::BundleResponseParseError::New(
                 web_package::mojom::BundleParseErrorType::kParserInternalError,
                 kConnectionError));
  response_callbacks_.clear();
  if (disconnect_callback_)
    std::move(disconnect_callback_).Run();
}

void SafeWebBundleParser::OnMetadataParsed(
    web_package::mojom::BundleMetadataPtr metadata,
    web_package::mojom::BundleMetadataParseErrorPtr error) {
  DCHECK(!metadata_callback_.is_null());
  std::move(metadata_callback_).Run(std::move(metadata), std::move(error));
}

void SafeWebBundleParser::OnResponseParsed(
    size_t callback_id,
    web_package::mojom::BundleResponsePtr response,
    web_package::mojom::BundleResponseParseErrorPtr error) {
  auto it = response_callbacks_.find(callback_id);
  DCHECK(it != response_callbacks_.end());
  auto callback = std::move(it->second);
  response_callbacks_.erase(it);
  std::move(callback).Run(std::move(response), std::move(error));
}

}  // namespace data_decoder
