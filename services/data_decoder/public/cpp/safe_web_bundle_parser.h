// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_WEB_BUNDLE_PARSER_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_WEB_BUNDLE_PARSER_H_

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/functional/callback.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"

namespace data_decoder {

// This interface specifies the requirements how a
// `web_package::mojom::BundleDataSource` should be created.
class DataSourceCreatingStrategy {
 public:
  virtual ~DataSourceCreatingStrategy() {}

  // Checks if the connection can be established. E.g. if the source of the
  // data is readable, etc.
  virtual base::expected<void, std::string> ExpectReady() const = 0;

  // Creates DataSource. The WebBundleParserFactory is used to bind the
  // appropriate receiver on the side of the data decoder process.
  // The return result will be used to create a parser.
  virtual mojo::PendingRemote<web_package::mojom::BundleDataSource>
  CreateDataSource(web_package::mojom::WebBundleParserFactory* binder) = 0;

  // Closes the underlying source of data (e.g. file). The callback must
  // be called when the source of data is closed.
  virtual void Close(base::OnceClosure callback) = 0;
};

// This class is used for safe parsing of the Web Bundles. Even though it
// internally uses parsing in the data decoder process by means of
// mojo and IPC, the aim of this class is to isolate users from any knowledge
// about mojo and IPC.
//
// Every parsing method will try to reestablish the IPC connection (if
// necessary) before returning an error.
//
// It is safe to delete this object from within the callbacks passed to its
// methods.
class SafeWebBundleParser {
 public:
  // Returns the strategy that lets the provided signed web bundle file
  // be a data source for parsing.
  static std::unique_ptr<DataSourceCreatingStrategy> GetFileStrategy(
      base::File file);

  SafeWebBundleParser(
      std::optional<GURL> base_url,
      std::unique_ptr<DataSourceCreatingStrategy> data_source_creator);

  SafeWebBundleParser(const SafeWebBundleParser&) = delete;
  SafeWebBundleParser& operator=(const SafeWebBundleParser&) = delete;

  // Remaining callbacks on flight will be dropped.
  ~SafeWebBundleParser();

  // Parses the integrity block of a Signed Web Bundle. See
  // `web_package::mojom::WebBundleParser::ParseIntegrityBlock` for
  // details. This method fails when it's called before the previous call
  // finishes.
  void ParseIntegrityBlock(
      web_package::mojom::WebBundleParser::ParseIntegrityBlockCallback
          callback);

  // Parses metadata of a (Signed) Web Bundle. If `offset` is specified, then
  // parsing of the metadata starts at that offset. If `offset` is not set, then
  // parsing either starts at offset 0 for non-random-access data sources, or at
  // a position based on the `length` field of the Web Bundle for random access
  // data sources. See `web_package::mojom::WebBundleParser::ParseMetadata` for
  // details. This method fails when it's called before the previous call
  // finishes.
  void ParseMetadata(
      std::optional<uint64_t> offset,
      web_package::mojom::WebBundleParser::ParseMetadataCallback callback);

  // Parses a response from a (Signed) Web Bundle. See
  // `web_package::mojom::WebBundleParser::ParseResponse` for details.
  void ParseResponse(
      uint64_t response_offset,
      uint64_t response_length,
      web_package::mojom::WebBundleParser::ParseResponseCallback callback);

  // Closes the data source this class is reading from by calling
  // `DataSourceCreatingStrategy::Close`.
  // One can use this function to know the point after which the source file
  // is closed and can be removed.
  // If the caller doesn't care when the file is closed, then it is fine
  // to destroy the instance of this class without calling this function.
  // The callback will not be called if the instance of this class
  // is destroyed.
  void Close(base::OnceClosure callback);

 private:
  web_package::mojom::WebBundleParserFactory* GetFactory();
  void OnDisconnect();
  void OnIntegrityBlockParsed(
      web_package::mojom::BundleIntegrityBlockPtr integrity_block,
      web_package::mojom::BundleIntegrityBlockParseErrorPtr error);
  void OnMetadataParsed(web_package::mojom::BundleMetadataPtr metadata,
                        web_package::mojom::BundleMetadataParseErrorPtr error);
  void OnResponseParsed(size_t callback_id,
                        web_package::mojom::BundleResponsePtr response,
                        web_package::mojom::BundleResponseParseErrorPtr error);

  base::expected<void, std::string> ConnectIfNecessary();
  void CloseDataSourceCreator();
  void ReplyClosed();

  struct Connection {
    Connection();
    ~Connection();
    DataDecoder data_decoder_;
    mojo::Remote<web_package::mojom::WebBundleParserFactory> factory_;
    mojo::Remote<web_package::mojom::WebBundleParser> parser_;
  };

  bool is_connected() const { return connection_.get(); }

  std::unique_ptr<Connection> connection_;
  std::unique_ptr<DataSourceCreatingStrategy> data_source_creator_;

  web_package::mojom::WebBundleParser::ParseIntegrityBlockCallback
      integrity_block_callback_;
  web_package::mojom::WebBundleParser::ParseMetadataCallback metadata_callback_;
  base::flat_map<size_t,
                 web_package::mojom::WebBundleParser::ParseResponseCallback>
      response_callbacks_;
  base::OnceClosure close_callback_;
  size_t response_callback_next_id_ = 0;

  std::optional<GURL> base_url_;

  base::WeakPtrFactory<SafeWebBundleParser> weak_factory_{this};
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_WEB_BUNDLE_PARSER_H_
