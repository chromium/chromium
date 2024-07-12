// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/safe_web_bundle_parser.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "url/gurl.h"

namespace data_decoder {

namespace {
constexpr char kConnectionError[] =
    "Cannot connect to the remote parser service";

void CloseFile(base::File file, base::OnceClosure close_callback) {
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce([](base::File file) { file.Close(); }, std::move(file)),
      std::move(close_callback));
}

// The strategy that creates a BundleDataSource from a file.
class FileDataSourceStrategy : public DataSourceCreatingStrategy {
 public:
  explicit FileDataSourceStrategy(base::File file) : file_(std::move(file)) {}
  FileDataSourceStrategy(const FileDataSourceStrategy&) = delete;
  FileDataSourceStrategy& operator=(const FileDataSourceStrategy&) = delete;

  ~FileDataSourceStrategy() override {
    if (file_.IsValid()) {
      CloseFile(std::move(file_), base::DoNothing());
    }
  }

  base::expected<void, std::string> ExpectReady() const override {
    if (!file_.IsValid()) {
      return base::unexpected(base::File::ErrorToString(file_.error_details()));
    }
    return base::ok();
  }

  mojo::PendingRemote<web_package::mojom::BundleDataSource> CreateDataSource(
      web_package::mojom::WebBundleParserFactory* binder) override {
    mojo::PendingRemote<web_package::mojom::BundleDataSource>
        file_data_source_pending_remote;
    binder->BindFileDataSource(
        file_data_source_pending_remote.InitWithNewPipeAndPassReceiver(),
        file_.Duplicate());
    return file_data_source_pending_remote;
  }

  void Close(base::OnceClosure callback) override {
    CloseFile(std::move(file_), std::move(callback));
  }

 private:
  base::File file_;
};

void ReplyWithInternalError(
    web_package::mojom::WebBundleParser::ParseIntegrityBlockCallback callback,
    std::string error) {
  std::move(callback).Run(
      nullptr,
      web_package::mojom::BundleIntegrityBlockParseError::New(
          web_package::mojom::BundleParseErrorType::kParserInternalError,
          std::move(error)));
}

void ReplyWithInternalError(
    web_package::mojom::WebBundleParser::ParseMetadataCallback callback,
    std::string error) {
  std::move(callback).Run(
      nullptr,
      web_package::mojom::BundleMetadataParseError::New(
          web_package::mojom::BundleParseErrorType::kParserInternalError,
          std::move(error)));
}

void ReplyWithInternalError(
    web_package::mojom::WebBundleParser::ParseResponseCallback callback,
    std::string error) {
  std::move(callback).Run(
      nullptr,
      web_package::mojom::BundleResponseParseError::New(
          web_package::mojom::BundleParseErrorType::kParserInternalError,
          std::move(error)));
}

}  // namespace

// static
std::unique_ptr<DataSourceCreatingStrategy>
SafeWebBundleParser::GetFileStrategy(base::File file) {
  return std::make_unique<data_decoder::FileDataSourceStrategy>(
      std::move(file));
}

SafeWebBundleParser::Connection::Connection() = default;
SafeWebBundleParser::Connection::~Connection() = default;

SafeWebBundleParser::SafeWebBundleParser(
    std::optional<GURL> base_url,
    std::unique_ptr<DataSourceCreatingStrategy> data_source_creator)
    : data_source_creator_(std::move(data_source_creator)),
      base_url_(std::move(base_url)) {}

SafeWebBundleParser::~SafeWebBundleParser() = default;

void SafeWebBundleParser::ParseIntegrityBlock(
    web_package::mojom::WebBundleParser::ParseIntegrityBlockCallback callback) {
  // This method is designed to be called once. So, allowing only once
  // simultaneous request is fine enough.
  if (!integrity_block_callback_.is_null()) {
    ReplyWithInternalError(std::move(callback), kConnectionError);
    return;
  }

  auto connection_status = ConnectIfNecessary();
  if (!connection_status.has_value()) {
    ReplyWithInternalError(std::move(callback), connection_status.error());
    return;
  }

  integrity_block_callback_ = std::move(callback);
  connection_->parser_->ParseIntegrityBlock(base::BindOnce(
      &SafeWebBundleParser::OnIntegrityBlockParsed, base::Unretained(this)));
}

void SafeWebBundleParser::ParseMetadata(
    std::optional<uint64_t> offset,
    web_package::mojom::WebBundleParser::ParseMetadataCallback callback) {
  // This method is designed to be called once. So, allowing only once
  // simultaneous request is fine enough.
  if (!metadata_callback_.is_null()) {
    ReplyWithInternalError(std::move(callback), kConnectionError);
    return;
  }

  auto connection_status = ConnectIfNecessary();
  if (!connection_status.has_value()) {
    ReplyWithInternalError(std::move(callback), connection_status.error());
    return;
  }

  metadata_callback_ = std::move(callback);
  connection_->parser_->ParseMetadata(
      std::move(offset), base::BindOnce(&SafeWebBundleParser::OnMetadataParsed,
                                        base::Unretained(this)));
}

void SafeWebBundleParser::ParseResponse(
    uint64_t response_offset,
    uint64_t response_length,
    web_package::mojom::WebBundleParser::ParseResponseCallback callback) {
  // This method allows simultaneous multiple requests. But if the unique ID
  // overflows, and the previous request that owns the same ID hasn't finished,
  // we just make the new request fail with kConnectionError.
  if (response_callbacks_.contains(response_callback_next_id_)) {
    ReplyWithInternalError(std::move(callback), kConnectionError);
    return;
  }

  auto connection_status = ConnectIfNecessary();
  if (!connection_status.has_value()) {
    ReplyWithInternalError(std::move(callback), connection_status.error());
    return;
  }

  size_t callback_id = response_callback_next_id_++;
  response_callbacks_[callback_id] = std::move(callback);
  connection_->parser_->ParseResponse(
      response_offset, response_length,
      base::BindOnce(&SafeWebBundleParser::OnResponseParsed,
                     base::Unretained(this), callback_id));
}

void SafeWebBundleParser::Close(base::OnceClosure callback) {
  close_callback_ = std::move(callback);
  if (is_connected()) {
    connection_->parser_->Close(base::BindOnce(
        &SafeWebBundleParser::CloseDataSourceCreator, base::Unretained(this)));
  } else {
    CloseDataSourceCreator();
  }
}

web_package::mojom::WebBundleParserFactory* SafeWebBundleParser::GetFactory() {
  CHECK(is_connected());
  if (!connection_->factory_) {
    connection_->data_decoder_.GetService()->BindWebBundleParserFactory(
        connection_->factory_.BindNewPipeAndPassReceiver());
    connection_->factory_.reset_on_disconnect();
  }
  return connection_->factory_.get();
}

base::expected<void, std::string> SafeWebBundleParser::ConnectIfNecessary() {
  if (is_connected()) {
    return base::ok();
  }

  RETURN_IF_ERROR(data_source_creator_->ExpectReady());

  connection_ = std::make_unique<Connection>();
  auto data_source = data_source_creator_->CreateDataSource(GetFactory());

  GetFactory()->GetParserForDataSource(
      connection_->parser_.BindNewPipeAndPassReceiver(), base_url_,
      std::move(data_source));
  connection_->parser_.set_disconnect_handler(base::BindOnce(
      &SafeWebBundleParser::OnDisconnect, base::Unretained(this)));

  return base::ok();
}

void SafeWebBundleParser::OnDisconnect() {
  CHECK(connection_);
  connection_.reset();

  // Any of these callbacks could delete `this`, hence we need to make sure to
  // not access any instance variables after we run any of these callbacks.
  auto integrity_block_callback = std::move(integrity_block_callback_);
  auto metadata_callback = std::move(metadata_callback_);
  auto response_callbacks = std::exchange(response_callbacks_, {});
  auto close_callback = std::move(close_callback_);

  if (!integrity_block_callback.is_null()) {
    ReplyWithInternalError(std::move(integrity_block_callback),
                           kConnectionError);
  }
  if (!metadata_callback.is_null()) {
    ReplyWithInternalError(std::move(metadata_callback), kConnectionError);
  }
  for (auto& [callback_id, callback] : response_callbacks) {
    ReplyWithInternalError(std::move(callback), kConnectionError);
  }
  if (!close_callback.is_null()) {
    std::move(close_callback).Run();
  }
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
  CHECK(it != response_callbacks_.end(), base::NotFatalUntil::M130);
  auto callback = std::move(it->second);
  response_callbacks_.erase(it);
  std::move(callback).Run(std::move(response), std::move(error));
}

void SafeWebBundleParser::CloseDataSourceCreator() {
  data_source_creator_->Close(base::BindOnce(&SafeWebBundleParser::ReplyClosed,
                                             weak_factory_.GetWeakPtr()));
}

void SafeWebBundleParser::ReplyClosed() {
  CHECK(!close_callback_.is_null());
  std::move(close_callback_).Run();
}

}  // namespace data_decoder
