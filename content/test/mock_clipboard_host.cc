// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/test/mock_clipboard_host.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/gfx/codec/png_codec.h"

namespace content {

MockClipboardHost::MockClipboardHost() = default;

MockClipboardHost::~MockClipboardHost() = default;

void MockClipboardHost::Bind(
    mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MockClipboardHost::Reset() {
  plain_text_ = std::u16string();
  html_text_ = std::u16string();
  svg_text_ = std::u16string();
  url_ = GURL();
  png_.clear();
  custom_data_.clear();
  write_smart_paste_ = false;
  needs_reset_ = false;
}

void MockClipboardHost::GetSequenceNumber(ui::ClipboardBuffer clipboard_buffer,
                                          GetSequenceNumberCallback callback) {
  std::move(callback).Run(sequence_number_);
}

std::vector<std::u16string> MockClipboardHost::ReadStandardFormatNames() {
  std::vector<std::u16string> types;
  if (!plain_text_.empty())
    types.push_back(base::ASCIIToUTF16(ui::kMimeTypeText));
  if (!html_text_.empty())
    types.push_back(base::ASCIIToUTF16(ui::kMimeTypeHTML));
  if (!svg_text_.empty())
    types.push_back(base::ASCIIToUTF16(ui::kMimeTypeSvg));
  if (!png_.empty())
    types.push_back(base::ASCIIToUTF16(ui::kMimeTypePNG));
  for (auto& it : custom_data_) {
    CHECK(!base::Contains(types, it.first));
    types.push_back(it.first);
  }
  return types;
}

void MockClipboardHost::ReadAvailableTypes(
    ui::ClipboardBuffer clipboard_buffer,
    ReadAvailableTypesCallback callback) {
  std::vector<std::u16string> types = ReadStandardFormatNames();
  std::move(callback).Run(std::move(types));
}

void MockClipboardHost::IsFormatAvailable(blink::mojom::ClipboardFormat format,
                                          ui::ClipboardBuffer clipboard_buffer,
                                          IsFormatAvailableCallback callback) {
  bool result = false;
  switch (format) {
    case blink::mojom::ClipboardFormat::kPlaintext:
      result = !plain_text_.empty();
      break;
    case blink::mojom::ClipboardFormat::kHtml:
      result = !html_text_.empty();
      break;
    case blink::mojom::ClipboardFormat::kSmartPaste:
      result = write_smart_paste_;
      break;
    case blink::mojom::ClipboardFormat::kBookmark:
      result = false;
      break;
  }
  std::move(callback).Run(result);
}

void MockClipboardHost::ReadText(ui::ClipboardBuffer clipboard_buffer,
                                 ReadTextCallback callback) {
  std::move(callback).Run(plain_text_);
}

void MockClipboardHost::ReadHtml(ui::ClipboardBuffer clipboard_buffer,
                                 ReadHtmlCallback callback) {
  std::move(callback).Run(html_text_, url_, 0, html_text_.length());
}

void MockClipboardHost::ReadSvg(ui::ClipboardBuffer clipboard_buffer,
                                ReadSvgCallback callback) {
  std::move(callback).Run(svg_text_);
}

void MockClipboardHost::ReadRtf(ui::ClipboardBuffer clipboard_buffer,
                                ReadRtfCallback callback) {
  std::move(callback).Run(std::string());
}

void MockClipboardHost::ReadPng(ui::ClipboardBuffer clipboard_buffer,
                                ReadPngCallback callback) {
  std::move(callback).Run(mojo_base::BigBuffer(png_));
}

void MockClipboardHost::ReadFiles(ui::ClipboardBuffer clipboard_buffer,
                                  ReadFilesCallback callback) {
  std::move(callback).Run(blink::mojom::ClipboardFiles::New());
}

void MockClipboardHost::ReadDataTransferCustomData(
    ui::ClipboardBuffer clipboard_buffer,
    const std::u16string& type,
    ReadDataTransferCustomDataCallback callback) {
  auto it = custom_data_.find(type);
  std::move(callback).Run(it != custom_data_.end() ? it->second
                                                   : std::u16string());
}

void MockClipboardHost::WriteText(const std::u16string& text) {
  if (needs_reset_)
    Reset();
  plain_text_ = text;
}

void MockClipboardHost::WriteHtml(const std::u16string& markup,
                                  const GURL& url) {
  if (needs_reset_)
    Reset();
  html_text_ = markup;
  url_ = url;
}

void MockClipboardHost::WriteSvg(const std::u16string& markup) {
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
    const base::flat_map<std::u16string, std::u16string>& data) {
  if (needs_reset_)
    Reset();
  for (auto& it : data)
    custom_data_[it.first] = it.second;
}

void MockClipboardHost::WriteBookmark(const std::string& url,
                                      const std::u16string& title) {}

void MockClipboardHost::WriteImage(const SkBitmap& bitmap) {
  if (needs_reset_)
    Reset();
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &png_);
}

void MockClipboardHost::CommitWrite() {
  sequence_number_ = ui::ClipboardSequenceNumberToken();
  needs_reset_ = true;
}

void MockClipboardHost::ReadAvailableCustomAndStandardFormats(
    ReadAvailableCustomAndStandardFormatsCallback callback) {
  std::vector<std::u16string> format_names = ReadStandardFormatNames();
  for (const auto& item : unsanitized_custom_data_map_)
    format_names.emplace_back(item.first);
  std::move(callback).Run(std::move(format_names));
}

void MockClipboardHost::ReadUnsanitizedCustomFormat(
    const std::u16string& format,
    ReadUnsanitizedCustomFormatCallback callback) {
  const auto it = unsanitized_custom_data_map_.find(format);
  if (it == unsanitized_custom_data_map_.end())
    return;

  mojo_base::BigBuffer buffer = mojo_base::BigBuffer(it->second);
  std::move(callback).Run(std::move(buffer));
}

void MockClipboardHost::WriteUnsanitizedCustomFormat(
    const std::u16string& format,
    mojo_base::BigBuffer data) {
  if (needs_reset_)
    Reset();
  // Simulate the underlying platform copying this data.
  std::vector<uint8_t> data_copy(data.data(), data.data() + data.size());
  // Append the "web " prefix since it is removed by the clipboard writer during
  // write.
  std::u16string web_format =
      base::StrCat({base::ASCIIToUTF16(ui::kWebClipboardFormatPrefix), format});
  unsanitized_custom_data_map_[web_format] = std::move(data_copy);
}

#if BUILDFLAG(IS_MAC)
void MockClipboardHost::WriteStringToFindPboard(const std::u16string& text) {}
#endif

}  // namespace content
