// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/safe_web_bundle_parser.h"

#include "base/functional/bind.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace data_decoder {

namespace {
constexpr char kConnectionError[] =
    "Cannot connect to the remote parser service";
}  // namespace

SafeWebBundleParser::SafeWebBundleParser(const absl::optional<GURL>& base_url)
    : base_url_(base_url) {}

SafeWebBundleParser::~SafeWebBundleParser() = default;

base::File::Error SafeWebBundleParser::OpenFile(base::File file) {
  if (!file.IsValid())
    return file.error_details();

  mojo::PendingRemote<web_package::mojom::BundleDataSource>
      file_data_source_pending_remote;
  GetFactory()->BindFileDataSource(
      file_data_source_pending_remote.InitWithNewPipeAndPassReceiver(),
      std::move(file));
  OpenDataSource(std::move(file_data_source_pending_remote));
  return base::File::FILE_OK;
}

void SafeWebBundleParser::OpenDataSource(
    mojo::PendingRemote<web_package::mojom::BundleDataSource> data_source) {
  DCHECK(disconnected_);
  GetFactory()->GetParserForDataSource(parser_.BindNewPipeAndPassReceiver(),
                                       base_url_, std::move(data_source));
  parser_.set_disconnect_handler(base::BindOnce(
      &SafeWebBundleParser::OnDisconnect, base::Unretained(this)));

  disconnected_ = false;
}

void SafeWebBundleParser::ParseIntegrityBlock(
    web_package::mojom::WebBundleParser::ParseIntegrityBlockCallback callback) {
  // This method is designed to be called once. So, allowing only once
  // simultaneous request is fine enough.
  if (disconnected_ || !integrity_block_callback_.is_null()) {
    std::move(callback).Run(
        nullptr,
        web_package::mojom::BundleIntegrityBlockParseError::New(
            web_package::mojom::BundleParseErrorType::kParserInternalError,
            kConnectionError));
    return;
  }
  integrity_block_callback_ = std::move(callback);
  parser_->ParseIntegrityBlock(base::BindOnce(
      &SafeWebBundleParser::OnIntegrityBlockParsed, base::Unretained(this)));
}

void SafeWebBundleParser::ParseMetadata(
    absl::optional<uint64_t> offset,
    web_package::mojom::WebBundleParser::ParseMetadataCallback callback) {
  // This method is designed to be called once. So, allowing only once
  // simultaneous request is fine enough.
  if (disconnected_ || !metadata_callback_.is_null()) {
    std::move(callback).Run(
        nullptr,
        web_package::mojom::BundleMetadataParseError::New(
            web_package::mojom::BundleParseErrorType::kParserInternalError,
            kConnectionError));
    return;
  }
  metadata_callback_ = std::move(callback);
  parser_->ParseMetadata(std::move(offset),
                         base::BindOnce(&SafeWebBundleParser::OnMetadataParsed,
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

void SafeWebBundleParser::Close(base::OnceClosure callback) {
  parser_->Close(base::BindOnce(&SafeWebBundleParser::OnParserClosed,
                                base::Unretained(this), std::move(callback)));
}

void SafeWebBundleParser::OnDisconnect() {
  disconnected_ = true;
  // Any of these callbacks could delete `this`, hence we need to make sure to
  // not access any instance variables after we run any of these callbacks.
  auto integrity_block_callback = std::move(integrity_block_callback_);
  auto metadata_callback = std::move(metadata_callback_);
  auto response_callbacks = std::exchange(response_callbacks_, {});
  auto disconnect_callback = std::move(disconnect_callback_);

  if (!integrity_block_callback.is_null()) {
    std::move(integrity_block_callback)
        .Run(nullptr,
             web_package::mojom::BundleIntegrityBlockParseError::New(
                 web_package::mojom::BundleParseErrorType::kParserInternalError,
                 kConnectionError));
  }
  if (!metadata_callback.is_null()) {
    std::move(metadata_callback)
        .Run(nullptr,
             web_package::mojom::BundleMetadataParseError::New(
                 web_package::mojom::BundleParseErrorType::kParserInternalError,
                 kConnectionError));
  }
  for (auto& [callback_id, callback] : response_callbacks) {
    std::move(callback).Run(
        nullptr,
        web_package::mojom::BundleResponseParseError::New(
            web_package::mojom::BundleParseErrorType::kParserInternalError,
            kConnectionError));
  }
  if (!disconnect_callback.is_null())
    std::move(disconnect_callback).Run();
}

void SafeWebBundleParser::OnIntegrityBlockParsed(
    web_package::mojom::BundleIntegrityBlockPtr integrity_block,
    web_package::mojom::BundleIntegrityBlockParseErrorPtr error) {
  DCHECK(!integrity_block_callback_.is_null());
  std::move(integrity_block_callback_)
      .Run(std::move(integrity_block), std::move(error));
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

void SafeWebBundleParser::OnParserClosed(base::OnceClosure callback) const {
  std::move(callback).Run();
}

}  // namespace data_decoder
