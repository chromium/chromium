// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_READER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_READER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "net/base/net_errors.h"
#include "services/data_decoder/public/cpp/safe_bundled_exchanges_parser.h"
#include "services/data_decoder/public/mojom/bundled_exchanges_parser.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/gurl.h"

namespace content {

class BundledExchangesSource;
class BundledExchangesBlobDataSource;

// A class to handle a BundledExchanges that is specified by |source|.
// It asks the utility process to parse metadata and response structures, and
// provides body data based on parsed information.
// This class is typically owned by BundledExchangesURLLoaderFactory, and also
// could be co-owned by BundledExchangesHandleTracker during navigations.
// Running on the UI thread.
class CONTENT_EXPORT BundledExchangesReader final
    : public base::RefCounted<BundledExchangesReader> {
 public:
  explicit BundledExchangesReader(
      std::unique_ptr<BundledExchangesSource> source);
  BundledExchangesReader(std::unique_ptr<BundledExchangesSource> source,
                         int64_t content_length,
                         mojo::ScopedDataPipeConsumerHandle outer_response_body,
                         network::mojom::URLLoaderClientEndpointsPtr endpoints,
                         BrowserContext::BlobContextGetter blob_context_getter);

  // Starts parsing, and runs |callback| when meta data gets to be available.
  // |error| is set only on failures.
  // Other methods below are only available after this |callback| invocation.
  using MetadataCallback = base::OnceCallback<void(
      data_decoder::mojom::BundleMetadataParseErrorPtr error)>;
  void ReadMetadata(MetadataCallback callback);

  // Gets data_decoder::mojom::BundleResponsePtr for the given |url| that
  // contains response headers and range information for its body.
  // Should be called after ReadMetadata finishes.
  using ResponseCallback = base::OnceCallback<void(
      data_decoder::mojom::BundleResponsePtr,
      data_decoder::mojom::BundleResponseParseErrorPtr)>;
  void ReadResponse(const GURL& url, ResponseCallback callback);

  // Starts loading response body. |response| should be obtained by
  // ReadResponse above beforehand. Body will be written into |producer_handle|.
  // After all body data is written, |callback| will be invoked.
  using BodyCompletionCallback = base::OnceCallback<void(net::Error net_error)>;
  void ReadResponseBody(data_decoder::mojom::BundleResponsePtr response,
                        mojo::ScopedDataPipeProducerHandle producer_handle,
                        BodyCompletionCallback callback);

  // Returns true if the BundledExchangesSource this object was constructed with
  // contains an exchange for |url|.
  // Should be called after ReadMetadata finishes.
  bool HasEntry(const GURL& url) const;

  // Returns the bundle's primary URL.
  // Should be called after ReadMetadata finishes.
  const GURL& GetPrimaryURL() const;

  // Returns the BundledExchangesSource.
  const BundledExchangesSource& source() const;

  void SetBundledExchangesParserFactoryForTesting(
      mojo::Remote<data_decoder::mojom::BundledExchangesParserFactory> factory);

 private:
  friend class base::RefCounted<BundledExchangesReader>;

  // A simple wrapper class to share a single base::File instance among multiple
  // SharedFileDataSource instances.
  class SharedFile final : public base::RefCountedThreadSafe<SharedFile> {
   public:
    explicit SharedFile(std::unique_ptr<BundledExchangesSource> source);
    void DuplicateFile(base::OnceCallback<void(base::File)> callback);
    base::File* operator->();

   private:
    friend class base::RefCountedThreadSafe<SharedFile>;
    ~SharedFile();

    void SetFile(std::unique_ptr<base::File> file);

    base::FilePath file_path_;
    std::unique_ptr<base::File> file_;
    base::OnceCallback<void(base::File)> duplicate_callback_;

    DISALLOW_COPY_AND_ASSIGN(SharedFile);
  };
  class SharedFileDataSource;

  ~BundledExchangesReader();

  void ReadMetadataInternal(MetadataCallback callback, base::File file);

  void OnMetadataParsed(MetadataCallback callback,
                        data_decoder::mojom::BundleMetadataPtr metadata,
                        data_decoder::mojom::BundleMetadataParseErrorPtr error);
  void OnResponseParsed(ResponseCallback callback,
                        data_decoder::mojom::BundleResponsePtr response,
                        data_decoder::mojom::BundleResponseParseErrorPtr error);

  SEQUENCE_CHECKER(sequence_checker_);

  const std::unique_ptr<BundledExchangesSource> source_;

  data_decoder::SafeBundledExchangesParser parser_;
  // Used when loading a web bundle from file.
  scoped_refptr<SharedFile> file_;
  // Used when loading a web bundle from network.
  std::unique_ptr<BundledExchangesBlobDataSource> blob_data_source_;

  GURL primary_url_;
  base::flat_map<GURL, data_decoder::mojom::BundleIndexValuePtr> entries_;
  bool metadata_ready_ = false;

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesReader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_READER_H_
