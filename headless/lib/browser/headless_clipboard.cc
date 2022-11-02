// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_clipboard.h"

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/gfx/codec/png_codec.h"

namespace headless {

HeadlessClipboard::HeadlessClipboard()
    : default_store_buffer_(ui::ClipboardBuffer::kCopyPaste) {}

HeadlessClipboard::~HeadlessClipboard() = default;

void HeadlessClipboard::OnPreShutdown() {}

// DataTransferEndpoint is not used on this platform.
ui::DataTransferEndpoint* HeadlessClipboard::GetSource(
    ui::ClipboardBuffer buffer) const {
  return nullptr;
}

const ui::ClipboardSequenceNumberToken& HeadlessClipboard::GetSequenceNumber(
    ui::ClipboardBuffer buffer) const {
  return GetStore(buffer).sequence_number;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
bool HeadlessClipboard::IsFormatAvailable(
    const ui::ClipboardFormatType& format,
    ui::ClipboardBuffer buffer,
    const ui::DataTransferEndpoint* data_dst) const {
  return base::Contains(GetStore(buffer).data, format);
}

void HeadlessClipboard::Clear(ui::ClipboardBuffer buffer) {
  GetStore(buffer).Clear();
}

std::vector<std::u16string> HeadlessClipboard::GetStandardFormats(
    ui::ClipboardBuffer buffer,
    const ui::DataTransferEndpoint* data_dst) const {
  std::vector<std::u16string> types;
  if (IsFormatAvailable(ui::ClipboardFormatType::PlainTextType(), buffer,
                        data_dst)) {
    types.push_back(base::UTF8ToUTF16(ui::kMimeTypeText));
  }
  if (IsFormatAvailable(ui::ClipboardFormatType::HtmlType(), buffer, data_dst))
    types.push_back(base::UTF8ToUTF16(ui::kMimeTypeHTML));
  if (IsFormatAvailable(ui::ClipboardFormatType::SvgType(), buffer, data_dst))
    types.push_back(base::UTF8ToUTF16(ui::kMimeTypeSvg));
  if (IsFormatAvailable(ui::ClipboardFormatType::RtfType(), buffer, data_dst))
    types.push_back(base::UTF8ToUTF16(ui::kMimeTypeRTF));
  if (IsFormatAvailable(ui::ClipboardFormatType::PngType(), buffer, data_dst))
    types.push_back(base::UTF8ToUTF16(ui::kMimeTypePNG));

  return types;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadAvailableTypes(
    ui::ClipboardBuffer buffer,
    const ui::DataTransferEndpoint* data_dst,
    std::vector<std::u16string>* types) const {
  DCHECK(types);
  types->clear();
  *types = GetStandardFormats(buffer, data_dst);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadText(ui::ClipboardBuffer buffer,
                                 const ui::DataTransferEndpoint* data_dst,
                                 std::u16string* result) const {
  std::string result8;
  ReadAsciiText(buffer, data_dst, &result8);
  *result = base::UTF8ToUTF16(result8);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadAsciiText(ui::ClipboardBuffer buffer,
                                      const ui::DataTransferEndpoint* data_dst,
                                      std::string* result) const {
  result->clear();
  const DataStore& store = GetStore(buffer);
  auto it = store.data.find(ui::ClipboardFormatType::PlainTextType());
  if (it != store.data.end())
    *result = it->second;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadHTML(ui::ClipboardBuffer buffer,
                                 const ui::DataTransferEndpoint* data_dst,
                                 std::u16string* markup,
                                 std::string* src_url,
                                 uint32_t* fragment_start,
                                 uint32_t* fragment_end) const {
  markup->clear();
  src_url->clear();
  const DataStore& store = GetStore(buffer);
  auto it = store.data.find(ui::ClipboardFormatType::HtmlType());
  if (it != store.data.end())
    *markup = base::UTF8ToUTF16(it->second);
  *src_url = store.html_src_url;
  *fragment_start = 0;
  *fragment_end = base::checked_cast<uint32_t>(markup->size());
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadSvg(ui::ClipboardBuffer buffer,
                                const ui::DataTransferEndpoint* data_dst,
                                std::u16string* result) const {
  result->clear();
  const DataStore& store = GetStore(buffer);
  auto it = store.data.find(ui::ClipboardFormatType::SvgType());
  if (it != store.data.end())
    *result = base::UTF8ToUTF16(it->second);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadRTF(ui::ClipboardBuffer buffer,
                                const ui::DataTransferEndpoint* data_dst,
                                std::string* result) const {
  result->clear();
  const DataStore& store = GetStore(buffer);
  auto it = store.data.find(ui::ClipboardFormatType::RtfType());
  if (it != store.data.end())
    *result = it->second;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadPng(ui::ClipboardBuffer buffer,
                                const ui::DataTransferEndpoint* data_dst,
                                ReadPngCallback callback) const {
  std::move(callback).Run(GetStore(buffer).png);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadCustomData(ui::ClipboardBuffer clipboard_buffer,
                                       const std::u16string& type,
                                       const ui::DataTransferEndpoint* data_dst,
                                       std::u16string* result) const {}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadFilenames(ui::ClipboardBuffer buffer,
                                      const ui::DataTransferEndpoint* data_dst,
                                      std::vector<ui::FileInfo>* result) const {
  *result = GetStore(buffer).filenames;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadBookmark(const ui::DataTransferEndpoint* data_dst,
                                     std::u16string* title,
                                     std::string* url) const {
  const DataStore& store = GetDefaultStore();
  auto it = store.data.find(ui::ClipboardFormatType::UrlType());
  if (it != store.data.end())
    *url = it->second;
  *title = base::UTF8ToUTF16(store.url_title);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadData(const ui::ClipboardFormatType& format,
                                 const ui::DataTransferEndpoint* data_dst,
                                 std::string* result) const {
  result->clear();
  const DataStore& store = GetDefaultStore();
  auto it = store.data.find(format);
  if (it != store.data.end())
    *result = it->second;
}

#if BUILDFLAG(IS_OZONE)
bool HeadlessClipboard::IsSelectionBufferAvailable() const {
  return false;
}
#endif  // BUILDFLAG(IS_OZONE)

// |data_src| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::WritePortableAndPlatformRepresentations(
    ui::ClipboardBuffer buffer,
    const ObjectMap& objects,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<ui::DataTransferEndpoint> data_src) {
  Clear(buffer);
  default_store_buffer_ = buffer;
  DispatchPlatformRepresentations(std::move(platform_representations));
  for (const auto& kv : objects)
    DispatchPortableRepresentation(kv.first, kv.second);
  default_store_buffer_ = ui::ClipboardBuffer::kCopyPaste;
}

void HeadlessClipboard::WriteText(const char* text_data, size_t text_len) {
  std::string text(text_data, text_len);
  GetDefaultStore().data[ui::ClipboardFormatType::PlainTextType()] = text;
  if (IsSupportedClipboardBuffer(ui::ClipboardBuffer::kSelection)) {
    GetStore(ui::ClipboardBuffer::kSelection)
        .data[ui::ClipboardFormatType::PlainTextType()] = text;
  }
}

void HeadlessClipboard::WriteHTML(const char* markup_data,
                                  size_t markup_len,
                                  const char* url_data,
                                  size_t url_len) {
  std::u16string markup;
  base::UTF8ToUTF16(markup_data, markup_len, &markup);
  GetDefaultStore().data[ui::ClipboardFormatType::HtmlType()] =
      base::UTF16ToUTF8(markup);
  GetDefaultStore().html_src_url = std::string(url_data, url_len);
}

void HeadlessClipboard::WriteSvg(const char* markup_data, size_t markup_len) {
  std::string markup(markup_data, markup_len);
  GetDefaultStore().data[ui::ClipboardFormatType::SvgType()] = markup;
}

void HeadlessClipboard::WriteRTF(const char* rtf_data, size_t data_len) {
  GetDefaultStore().data[ui::ClipboardFormatType::RtfType()] =
      std::string(rtf_data, data_len);
}

void HeadlessClipboard::WriteFilenames(std::vector<ui::FileInfo> filenames) {
  GetDefaultStore().filenames = std::move(filenames);
}

void HeadlessClipboard::WriteBookmark(const char* title_data,
                                      size_t title_len,
                                      const char* url_data,
                                      size_t url_len) {
  GetDefaultStore().data[ui::ClipboardFormatType::UrlType()] =
      std::string(url_data, url_len);
  GetDefaultStore().url_title = std::string(title_data, title_len);
}

void HeadlessClipboard::WriteWebSmartPaste() {
  // Create a dummy entry.
  GetDefaultStore().data[ui::ClipboardFormatType::WebKitSmartPasteType()];
}

void HeadlessClipboard::WriteBitmap(const SkBitmap& bitmap) {
  // Create a dummy entry.
  GetDefaultStore().data[ui::ClipboardFormatType::PngType()];
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false,
                                    &GetDefaultStore().png);
}

void HeadlessClipboard::WriteData(const ui::ClipboardFormatType& format,
                                  const char* data_data,
                                  size_t data_len) {
  GetDefaultStore().data[format] = std::string(data_data, data_len);
}

HeadlessClipboard::DataStore::DataStore() = default;

HeadlessClipboard::DataStore::DataStore(const DataStore& other) = default;

HeadlessClipboard::DataStore::~DataStore() = default;

void HeadlessClipboard::DataStore::Clear() {
  data.clear();
  url_title.clear();
  html_src_url.clear();
  png.clear();
}

const HeadlessClipboard::DataStore& HeadlessClipboard::GetStore(
    ui::ClipboardBuffer buffer) const {
  CHECK(IsSupportedClipboardBuffer(buffer));
  return stores_[buffer];
}

HeadlessClipboard::DataStore& HeadlessClipboard::GetStore(
    ui::ClipboardBuffer buffer) {
  CHECK(IsSupportedClipboardBuffer(buffer));
  DataStore& store = stores_[buffer];
  store.sequence_number = ui::ClipboardSequenceNumberToken();
  return store;
}

const HeadlessClipboard::DataStore& HeadlessClipboard::GetDefaultStore() const {
  return GetStore(default_store_buffer_);
}

HeadlessClipboard::DataStore& HeadlessClipboard::GetDefaultStore() {
  return GetStore(default_store_buffer_);
}

}  // namespace headless
