// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_test_helpers.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "extensions/browser/mime_handler/stream_container.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"
#include "url/gurl.h"

namespace extensions::mime_handler {

std::unique_ptr<extensions::StreamContainer> GenerateSampleStreamContainer(
    int container_number) {
  const std::string container_number_string =
      base::NumberToString(container_number);
  const GURL handler_url =
      GURL("https://handler_url" + container_number_string);
  const std::string extension_id = "extension_id" + container_number_string;
  const GURL original_url =
      GURL("https://original_url" + container_number_string);

  auto transferrable_loader = blink::mojom::TransferrableURLLoader::New();
  transferrable_loader->url = GURL("stream://url" + container_number_string);
  transferrable_loader->head = network::mojom::URLResponseHead::New();
  transferrable_loader->head->mime_type = "application/pdf";
  transferrable_loader->head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/2 200 OK");

  return std::make_unique<extensions::StreamContainer>(
      /*tab_id=*/container_number, /*embedded=*/true, handler_url, extension_id,
      std::move(transferrable_loader), original_url);
}

void StringDrainerClient::OnDataAvailable(base::span<const uint8_t> data) {
  accumulated_.append(reinterpret_cast<const char*>(data.data()), data.size());
}

void StringDrainerClient::OnDataComplete() {
  complete_ = true;
}

}  // namespace extensions::mime_handler
