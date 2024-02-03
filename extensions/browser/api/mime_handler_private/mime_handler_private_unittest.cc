// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "extensions/browser/api/mime_handler_private/mime_handler_private.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"

namespace extensions {

class MimeHandlerServiceImplTest : public testing::Test {
 public:
  void SetUp() override {
    const ExtensionId extension_id =
        extension_misc::kMimeHandlerPrivateTestExtensionId;
    auto transferrable_loader = blink::mojom::TransferrableURLLoader::New();
    transferrable_loader->url = GURL("stream://url");
    transferrable_loader->head = network::mojom::URLResponseHead::New();
    transferrable_loader->head->mime_type = "application/pdf";
    transferrable_loader->head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/2 200 OK");

    stream_container_ = std::make_unique<StreamContainer>(
        /*tab_id=*/1, /*embedded=*/true,
        Extension::GetBaseURLFromExtensionId(extension_id), extension_id,
        std::move(transferrable_loader), GURL("test://extensions_unittests"));
    service_ = std::make_unique<MimeHandlerServiceImpl>(
        stream_container_->GetWeakPtr());
  }

  void TearDown() override {
    service_.reset();
    stream_container_.reset();
  }

  std::unique_ptr<StreamContainer> stream_container_;
  std::unique_ptr<mime_handler::MimeHandlerService> service_;
};

TEST_F(MimeHandlerServiceImplTest, SetValidPdfPluginAttributes) {
  {
    const double kBackgroundColor = 4292533472.0f;
    service_->SetPdfPluginAttributes(mime_handler::PdfPluginAttributes::New(
        /*background_color=*/kBackgroundColor, /*allow_javascript=*/true));
    ASSERT_TRUE(stream_container_->pdf_plugin_attributes());

    EXPECT_EQ(kBackgroundColor,
              stream_container_->pdf_plugin_attributes()->background_color);
    EXPECT_TRUE(stream_container_->pdf_plugin_attributes()->allow_javascript);
  }

  {
    service_->SetPdfPluginAttributes(mime_handler::PdfPluginAttributes::New(
        /*background_color=*/0.0f, /*allow_javascript=*/true));
    ASSERT_TRUE(stream_container_->pdf_plugin_attributes());

    EXPECT_EQ(0.0f,
              stream_container_->pdf_plugin_attributes()->background_color);
    EXPECT_TRUE(stream_container_->pdf_plugin_attributes()->allow_javascript);
  }

  {
    service_->SetPdfPluginAttributes(mime_handler::PdfPluginAttributes::New(
        /*background_color=*/UINT32_MAX, /*allow_javascript=*/false));
    ASSERT_TRUE(stream_container_->pdf_plugin_attributes());

    EXPECT_EQ(static_cast<double>(UINT32_MAX),
              stream_container_->pdf_plugin_attributes()->background_color);
    EXPECT_FALSE(stream_container_->pdf_plugin_attributes()->allow_javascript);
  }
}

TEST_F(MimeHandlerServiceImplTest,
       SetPdfPluginAttributesInvalidBackgroundColor) {
  {
    // Background is not an integer.
    service_->SetPdfPluginAttributes(mime_handler::PdfPluginAttributes::New(
        /*background_color=*/12.34, /*allow_javascript=*/true));
    EXPECT_FALSE(stream_container_->pdf_plugin_attributes());
  }

  {
    // Background color is beyond the range of an uint32_t.
    uint64_t color_beyond_range = UINT32_MAX + static_cast<uint64_t>(1);
    service_->SetPdfPluginAttributes(mime_handler::PdfPluginAttributes::New(
        static_cast<double>(color_beyond_range), /*allow_javascript=*/true));
    EXPECT_FALSE(stream_container_->pdf_plugin_attributes());
  }
}

}  // namespace extensions
