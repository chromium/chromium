// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/mock_clipboard_host.h"

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

MockClipboardHost::MockClipboardHost() = default;

MockClipboardHost::~MockClipboardHost() = default;

void MockClipboardHost::Bind(
    mojo::PendingReceiver<mojom::blink::ClipboardHost> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MockClipboardHost::Reset() {
  plain_text_ = g_empty_string;
  html_text_ = g_empty_string;
  svg_text_ = g_empty_string;
  rtf_text_ = g_empty_string;
  files_ = mojom::blink::ClipboardFiles::New();
  url_ = KURL();
  png_.clear();
  custom_data_.clear();
  write_smart_paste_ = false;
  needs_reset_ = false;
}

void MockClipboardHost::WriteRtf(const String& rtf_text) {
  if (needs_reset_) {
    Reset();
  }
  rtf_text_ = rtf_text;
}

void MockClipboardHost::WriteFiles(mojom::blink::ClipboardFilesPtr files) {
  if (needs_reset_) {
    Reset();
  }
  files_ = std::move(files);
}

void MockClipboardHost::GetSequenceNumber(
    mojom::ClipboardBuffer clipboard_buffer,
    GetSequenceNumberCallback callback) {
  std::move(callback).Run(sequence_number_);
}

Vector<String> MockClipboardHost::ReadStandardFormatNames() {
  Vector<String> types;
  if (!plain_text_.empty())
    types.push_back(kMimeTypeTextPlain);
  if (!html_text_.empty())
    types.push_back(kMimeTypeTextHTML);
  if (!svg_text_.empty())
    types.push_back(kMimeTypeImageSvg);
  if (!png_.empty())
    types.push_back(kMimeTypeImagePng);
  for (auto& it : custom_data_) {
    CHECK(!base::Contains(types, it.key));
    types.push_back(it.key);
  }
  return types;
}

void MockClipboardHost::ReadAvailableTypes(
    mojom::ClipboardBuffer clipboard_buffer,
    ReadAvailableTypesCallback callback) {
  Vector<String> types = ReadStandardFormatNames();
  std::move(callback).Run(std::move(types));
}

void MockClipboardHost::IsFormatAvailable(
    mojom::ClipboardFormat format,
    mojom::ClipboardBuffer clipboard_buffer,
    IsFormatAvailableCallback callback) {
  bool result = false;
  switch (format) {
    case mojom::ClipboardFormat::kPlaintext:
      result = !plain_text_.empty();
      break;
    case mojom::ClipboardFormat::kHtml:
      result = !html_text_.empty();
      break;
    case mojom::ClipboardFormat::kSmartPaste:
      result = write_smart_paste_;
      break;
    case mojom::ClipboardFormat::kBookmark:
      result = false;
      break;
  }
  std::move(callback).Run(result);
}

void MockClipboardHost::ReadText(mojom::ClipboardBuffer clipboard_buffer,
                                 ReadTextCallback callback) {
  std::move(callback).Run(plain_text_);
}

void MockClipboardHost::ReadHtml(mojom::ClipboardBuffer clipboard_buffer,
                                 ReadHtmlCallback callback) {
  std::move(callback).Run(html_text_, url_, 0, html_text_.length());
}

void MockClipboardHost::ReadSvg(mojom::ClipboardBuffer clipboard_buffer,
                                ReadSvgCallback callback) {
  std::move(callback).Run(svg_text_);
}

void MockClipboardHost::ReadRtf(mojom::ClipboardBuffer clipboard_buffer,
                                ReadRtfCallback callback) {
  std::move(callback).Run(rtf_text_);
}

void MockClipboardHost::ReadPng(mojom::ClipboardBuffer clipboard_buffer,
                                ReadPngCallback callback) {
  std::move(callback).Run(mojo_base::BigBuffer(png_));
}

void MockClipboardHost::ReadFiles(mojom::ClipboardBuffer clipboard_buffer,
                                  ReadFilesCallback callback) {
  std::move(callback).Run(std::move(files_));
}

void MockClipboardHost::ReadDataTransferCustomData(
    mojom::ClipboardBuffer clipboard_buffer,
    const String& type,
    ReadDataTransferCustomDataCallback callback) {
  auto it = custom_data_.find(type);
  std::move(callback).Run(it != custom_data_.end() ? it->value
                                                   : g_empty_string);
}

void MockClipboardHost::WriteText(const String& text) {
  if (needs_reset_)
    Reset();
  plain_text_ = text;
}

void MockClipboardHost::WriteHtml(const String& markup, const KURL& url) {
  if (needs_reset_)
    Reset();
  html_text_ = markup;
  url_ = url;
}

void MockClipboardHost::WriteSvg(const String& markup) {
  if (needs_reset_)
    Reset();
  svg_text_ = markup;
}

void MockClipboardHost::WriteSmartPasteMarker() {
  if (needs_reset_)
    Reset();
  write_smart_paste_ = true;
}

void MockClipboardHost::WriteDataTransferCustomData(
    const HashMap<String, String>& data) {
  if (needs_reset_)
    Reset();
  for (auto& it : data)
    custom_data_.Set(it.key, it.value);
}

void MockClipboardHost::WriteBookmark(const String& url, const String& title) {}

void MockClipboardHost::WriteImage(const SkBitmap& bitmap) {
  if (needs_reset_)
    Reset();
  SkPixmap pixmap;
  bitmap.peekPixels(&pixmap);
  // Set encoding options to favor speed over size.
  SkPngEncoder::Options options;
  options.fZLibLevel = 1;
  options.fFilterFlags = SkPngEncoder::FilterFlag::kNone;

  ImageEncoder::Encode(&png_, pixmap, options);
}

void MockClipboardHost::CommitWrite() {
  sequence_number_ = ClipboardSequenceNumberToken();
  needs_reset_ = true;
}

void MockClipboardHost::ReadAvailableCustomAndStandardFormats(
    ReadAvailableCustomAndStandardFormatsCallback callback) {
  Vector<String> format_names = ReadStandardFormatNames();
  for (const auto& item : unsanitized_custom_data_map_)
    format_names.emplace_back(item.key);
  std::move(callback).Run(std::move(format_names));
}

void MockClipboardHost::ReadUnsanitizedCustomFormat(
    const String& format,
    ReadUnsanitizedCustomFormatCallback callback) {
  const auto it = unsanitized_custom_data_map_.find(format);
  if (it == unsanitized_custom_data_map_.end())
    return;

  mojo_base::BigBuffer buffer = mojo_base::BigBuffer(it->value);
  std::move(callback).Run(std::move(buffer));
}

void MockClipboardHost::WriteUnsanitizedCustomFormat(
    const String& format,
    mojo_base::BigBuffer data) {
  if (needs_reset_)
    Reset();
  // Simulate the underlying platform copying this data.
  Vector<uint8_t> data_copy(base::saturated_cast<wtf_size_t>(data.size()),
                            *data.data());
  // Append the "web " prefix since it is removed by the clipboard writer during
  // write.
  unsanitized_custom_data_map_.Set("web " + format, std::move(data_copy));
}

#if BUILDFLAG(IS_MAC)
void MockClipboardHost::WriteStringToFindPboard(const String& text) {}
#endif

}  // namespace blink
