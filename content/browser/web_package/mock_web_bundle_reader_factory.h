// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_MOCK_WEB_BUNDLE_READER_FACTORY_H_
#define CONTENT_BROWSER_WEB_PACKAGE_MOCK_WEB_BUNDLE_READER_FACTORY_H_

#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "content/browser/web_package/web_bundle_reader.h"

namespace content {

// A class to prepare a WebBundleReader instance that uses a mocked
// WebBundleParser instance so that the WebBundleReader can run
// without external utility processes. It also allows to craft arbitrary
// responses for each parsing request.
class MockWebBundleReaderFactory {
 public:
  static std::unique_ptr<MockWebBundleReaderFactory> Create();

  MockWebBundleReaderFactory() = default;

  MockWebBundleReaderFactory(const MockWebBundleReaderFactory&) = delete;
  MockWebBundleReaderFactory& operator=(const MockWebBundleReaderFactory&) =
      delete;

  virtual ~MockWebBundleReaderFactory() = default;

  // Creates WebBundleReader instance. A temporary file is created and
  // |test_file_data| is stored. This temporary file is used when
  // WebBundleReader::ReadResponseBody() is called.
  virtual scoped_refptr<WebBundleReader> CreateReader(
      const std::string& test_file_data) = 0;

  // Calls ReadMetadata with |callback| for |reader|, and simulates the call as
  // |metadata| is read.
  virtual void ReadAndFullfillMetadata(
      WebBundleReader* reader,
      web_package::mojom::BundleMetadataPtr metadata,
      WebBundleReader::MetadataCallback callback) = 0;

  // Calls ReadResponse on |reader| with |callback|, verifies that |reader|
  // calls ParseResponse with |expected_parse_args|, and responds as if
  // |response| is read.
  virtual void ReadAndFullfillResponse(
      WebBundleReader* reader,
      const network::ResourceRequest& resource_request,
      web_package::mojom::BundleResponseLocationPtr expected_parse_args,
      web_package::mojom::BundleResponsePtr response,
      WebBundleReader::ResponseCallback callback) = 0;

  // Sets up the mocked factory so that the created WebBundleReader instance
  // can read |response| when WebBundleReader::ReadResponse is called.
  virtual void FullfillResponse(
      web_package::mojom::BundleResponseLocationPtr expected_parse_args,
      web_package::mojom::BundleResponsePtr response) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_MOCK_WEB_BUNDLE_READER_FACTORY_H_
