// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fileapi/file_reader_data.h"

#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

DOMArrayBuffer* ToDOMArrayBuffer(ArrayBufferContents raw_data) {
  return DOMArrayBuffer::Create(std::move(raw_data));
}

String ToDataURL(ArrayBufferContents raw_data, const String& data_type) {
  StringBuilder builder;
  builder.Append("data:");

  if (!raw_data.IsValid()) {
    return builder.ToString();
  }

  if (data_type.empty()) {
    // Match Firefox in defaulting to application/octet-stream when the MIME
    // type is unknown. See https://crbug.com/48368.
    builder.Append("application/octet-stream");
  } else {
    builder.Append(data_type);
  }
  builder.Append(";base64,");

  if (raw_data.DataLength()) {
    Vector<char> out;
    Base64Encode(raw_data.ByteSpan(), out);
    builder.Append(out.data(), out.size());
  }

  return builder.ToString();
}

String ToBinaryString(ArrayBufferContents raw_data) {
  CHECK(raw_data.IsValid());
  return String(static_cast<const char*>(raw_data.Data()),
                static_cast<size_t>(raw_data.DataLength()));
}

String ToTextString(ArrayBufferContents raw_data,
                    const WTF::TextEncoding& encoding) {
  if (!raw_data.IsValid() || !raw_data.DataLength()) {
    return "";
  }
  // Decode the data.
  // The File API spec says that we should use the supplied encoding if it is
  // valid. However, we choose to ignore this requirement in order to be
  // consistent with how WebKit decodes the web content: always has the BOM
  // override the provided encoding.
  // FIXME: consider supporting incremental decoding to improve the perf.
  StringBuilder builder;
  auto decoder = TextResourceDecoder(TextResourceDecoderOptions(
      TextResourceDecoderOptions::kPlainTextContent,
      encoding.IsValid() ? encoding : UTF8Encoding()));
  builder.Append(decoder.Decode(raw_data.ByteSpan()));

  builder.Append(decoder.Flush());

  return builder.ToString();
}

String ToString(ArrayBufferContents raw_data,
                FileReadType read_type,
                const WTF::TextEncoding& encoding,
                const String& data_type) {
  switch (read_type) {
    case FileReadType::kReadAsBinaryString:
      return ToBinaryString(std::move(raw_data));
    case FileReadType::kReadAsText:
      return ToTextString(std::move(raw_data), std::move(encoding));
    case FileReadType::kReadAsDataURL:
      return ToDataURL(std::move(raw_data), std::move(data_type));
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return "";
}

}  // namespace

ArrayBufferContents FileReaderData::AsArrayBufferContents() && {
  CHECK(raw_data_.IsValid());
  return std::move(raw_data_);
}
DOMArrayBuffer* FileReaderData::AsDOMArrayBuffer() && {
  CHECK(raw_data_.IsValid());
  return ToDOMArrayBuffer(std::move(raw_data_));
}
String FileReaderData::AsBinaryString() && {
  CHECK(raw_data_.IsValid());
  return ToBinaryString(std::move(raw_data_));
}
String FileReaderData::AsText(const String& encoding) && {
  CHECK(raw_data_.IsValid());
  return ToTextString(std::move(raw_data_), WTF::TextEncoding(encoding));
}
String FileReaderData::AsDataURL(const String& data_type) && {
  CHECK(raw_data_.IsValid());
  return ToDataURL(std::move(raw_data_), data_type);
}
String FileReaderData::AsString(FileReadType read_type,
                                const String& encoding,
                                const String& data_type) && {
  CHECK(raw_data_.IsValid());
  return ToString(std::move(raw_data_), read_type, WTF::TextEncoding(encoding),
                  data_type);
}

}  // namespace blink
