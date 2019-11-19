// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_MOCK_BUNDLED_EXCHANGES_READER_FACTORY_H_
#define CONTENT_BROWSER_WEB_PACKAGE_MOCK_BUNDLED_EXCHANGES_READER_FACTORY_H_

#include "base/macros.h"
#include "content/browser/web_package/bundled_exchanges_reader.h"
#include "services/data_decoder/public/mojom/bundled_exchanges_parser.mojom.h"

namespace content {

// A class to prepare a BundledExchangesReader instance that uses a mocked
// BundledExchangesParser instance so that the BundledExchangesReader can run
// without external utility processes. It also allows to craft arbitrary
// responses for each parsing request.
class MockBundledExchangesReaderFactory {
 public:
  static std::unique_ptr<MockBundledExchangesReaderFactory> Create();

  MockBundledExchangesReaderFactory() = default;
  virtual ~MockBundledExchangesReaderFactory() = default;

  // Creates BundledExchangesReader instance. A temporary file is created and
  // |test_file_data| is stored. This temporary file is used when
  // BundledExchangesReader::ReadResponseBody() is called.
  virtual scoped_refptr<BundledExchangesReader> CreateReader(
      const std::string& test_file_data) = 0;

  // Calls ReadMetadata with |callback| for |reader|, and simulates the call as
  // |metadata| is read.
  virtual void ReadAndFullfillMetadata(
      BundledExchangesReader* reader,
      data_decoder::mojom::BundleMetadataPtr metadata,
      BundledExchangesReader::MetadataCallback callback) = 0;

  // Calls ReadResponse with |callback| for |reader|, and simulates the call as
  // |response| is read.
  virtual void ReadAndFullfillResponse(
      BundledExchangesReader* reader,
      const GURL& url,
      data_decoder::mojom::BundleResponsePtr response,
      BundledExchangesReader::ResponseCallback callback) = 0;

  // Sets up the mocked factory so that the created BundledExchangesReader
  // instance can read |response| when BundledExchangesReader::ReaderResponse is
  // called.
  virtual void FullfillResponse(
      data_decoder::mojom::BundleResponsePtr response,
      BundledExchangesReader::ResponseCallback callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBundledExchangesReaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_MOCK_BUNDLED_EXCHANGES_READER_FACTORY_H_
