// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/mime_handler_private/mime_handler_private.h"

#include <cmath>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "extensions/common/constants.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/http/http_response_headers.h"

namespace extensions {
namespace {

base::flat_map<std::string, std::string> CreateResponseHeadersMap(
    const net::HttpResponseHeaders* headers) {
  base::flat_map<std::string, std::string> result;
  if (!headers)
    return result;

  size_t iter = 0;
  std::string header_name;
  std::string header_value;
  while (headers->EnumerateHeaderLines(&iter, &header_name, &header_value)) {
    // mojo strings must be UTF-8 and headers might not be, so drop any headers
    // that aren't ASCII. The PDF plugin does not use any headers with non-ASCII
    // names and non-ASCII values are never useful for the headers the plugin
    // does use.
    //
    // TODO(sammc): Send as bytes instead of a string and let the client decide
    // how to decode.
    if (!base::IsStringASCII(header_name) || !base::IsStringASCII(header_value))
      continue;
    auto& current_value = result[header_name];
    if (!current_value.empty())
      current_value += ", ";
    current_value += header_value;
  }
  return result;
}

}  // namespace

MimeHandlerServiceImpl::MimeHandlerServiceImpl(
    base::WeakPtr<StreamContainer> stream_container)
    : stream_(stream_container) {}

MimeHandlerServiceImpl::~MimeHandlerServiceImpl() = default;

// static
void MimeHandlerServiceImpl::Create(
    base::WeakPtr<StreamContainer> stream_container,
    mojo::PendingReceiver<mime_handler::MimeHandlerService> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MimeHandlerServiceImpl>(stream_container),
      std::move(receiver));
}

void MimeHandlerServiceImpl::GetStreamInfo(GetStreamInfoCallback callback) {
  if (!stream_) {
    std::move(callback).Run(mime_handler::StreamInfoPtr());
    return;
  }
  std::move(callback).Run(
      mojo::ConvertTo<mime_handler::StreamInfoPtr>(*stream_));
}

void MimeHandlerServiceImpl::SetPdfPluginAttributes(
    mime_handler::PdfPluginAttributesPtr pdf_plugin_attributes) {
  if (!stream_)
    return;

  // Check the `background_color` is an integer.
  double whole = 0.0;
  if (std::modf(pdf_plugin_attributes->background_color, &whole) != 0.0)
    return;

  // Check the `background_color` is within the range of a uint32_t.
  if (!base::IsValueInRangeForNumericType<uint32_t>(
          pdf_plugin_attributes->background_color)) {
    return;
  }

  stream_->set_pdf_plugin_attributes(std::move(pdf_plugin_attributes));
}

}  // namespace extensions

namespace mojo {

extensions::mime_handler::StreamInfoPtr TypeConverter<
    extensions::mime_handler::StreamInfoPtr,
    extensions::StreamContainer>::Convert(const extensions::StreamContainer&
                                              stream) {
  if (stream.stream_url().is_empty())
    return extensions::mime_handler::StreamInfoPtr();

  auto result = extensions::mime_handler::StreamInfo::New();
  result->embedded = stream.embedded();
  result->tab_id = stream.tab_id();
  result->mime_type = stream.mime_type();
  result->original_url = stream.original_url().spec();
  result->stream_url = stream.stream_url().spec();
  result->response_headers =
      extensions::CreateResponseHeadersMap(stream.response_headers());
  return result;
}

}  // namespace mojo
