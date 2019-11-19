// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/url_conversion.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "url/gurl.h"

namespace blink {

GURL WebStringToGURL(const WebString& web_string) {
  if (web_string.IsEmpty())
    return GURL();

  String str = web_string;
  if (str.Is8Bit()) {
    // Ensure the (possibly Latin-1) 8-bit string is UTF-8 for GURL.
    StringUTF8Adaptor utf8(str);
    return GURL(utf8.AsStringPiece());
  }

  // GURL can consume UTF-16 directly.
  return GURL(base::StringPiece16(str.Characters16(), str.length()));
}

mojo::ScopedMessagePipeHandle DataURLToMessagePipeHandle(
    const WebString& data_url) {
  auto blob_data = std::make_unique<blink::BlobData>();
  blob_data->AppendBytes(data_url.Utf8().data(), data_url.length());
  scoped_refptr<blink::BlobDataHandle> blob_data_handle =
      blink::BlobDataHandle::Create(std::move(blob_data), data_url.length());
  mojo::PendingRemote<mojom::blink::Blob> data_url_blob =
      blob_data_handle->CloneBlobRemote();
  return data_url_blob.PassPipe();
}

}  // namespace blink
