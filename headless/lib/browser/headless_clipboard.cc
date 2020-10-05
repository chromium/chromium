// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_clipboard.h"

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_data_endpoint.h"

namespace headless {

HeadlessClipboard::HeadlessClipboard()
    : default_store_buffer_(ui::ClipboardBuffer::kCopyPaste) {}

HeadlessClipboard::~HeadlessClipboard() = default;

void HeadlessClipboard::OnPreShutdown() {}

uint64_t HeadlessClipboard::GetSequenceNumber(
    ui::ClipboardBuffer buffer) const {
  return GetStore(buffer).sequence_number;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
bool HeadlessClipboard::IsFormatAvailable(
    const ui::ClipboardFormatType& format,
    ui::ClipboardBuffer buffer,
    const ui::ClipboardDataEndpoint* data_dst) const {
  return base::Contains(GetStore(buffer).data, format);
}

void HeadlessClipboard::Clear(ui::ClipboardBuffer buffer) {
  GetStore(buffer).Clear();
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadAvailableTypes(
    ui::ClipboardBuffer buffer,
    const ui::ClipboardDataEndpoint* data_dst,
    std::vector<base::string16>* types) const {
  DCHECK(types);
  types->clear();

  if (IsFormatAvailable(ui::ClipboardFormatType::GetPlainTextType(), buffer,
                        data_dst))
    types->push_back(base::UTF8ToUTF16(ui::kMimeTypeText));
  if (IsFormatAvailable(ui::ClipboardFormatType::GetHtmlType(), buffer,
                        data_dst))
    types->push_back(base::UTF8ToUTF16(ui::kMimeTypeHTML));

  if (IsFormatAvailable(ui::ClipboardFormatType::GetRtfType(), buffer,
                        data_dst))
    types->push_back(base::UTF8ToUTF16(ui::kMimeTypeRTF));
  if (IsFormatAvailable(ui::ClipboardFormatType::GetBitmapType(), buffer,
                        data_dst))
    types->push_back(base::UTF8ToUTF16(ui::kMimeTypePNG));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
std::vector<base::string16>
HeadlessClipboard::ReadAvailablePlatformSpecificFormatNames(
    ui::ClipboardBuffer buffer,
    const ui::ClipboardDataEndpoint* data_dst) const {
  const auto& data = GetStore(buffer).data;
  std::vector<base::string16> types;
  types.reserve(data.size());
  for (const auto& it : data) {
    base::string16 type = base::UTF8ToUTF16(it.first.GetName());
    types.push_back(type);
  }

  return types;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadText(ui::ClipboardBuffer buffer,
                                 const ui::ClipboardDataEndpoint* data_dst,
                                 base::string16* result) const {
  std::string result8;
  ReadAsciiText(buffer, data_dst, &result8);
  *result = base::UTF8ToUTF16(result8);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadAsciiText(ui::ClipboardBuffer buffer,
                                      const ui::ClipboardDataEndpoint* data_dst,
                                      std::string* result) const {
  result->clear();
  const DataStore& store = GetStore(buffer);
  auto it = store.data.find(ui::ClipboardFormatType::GetPlainTextType());
  if (it != store.data.end())
    *result = it->second;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadHTML(ui::ClipboardBuffer buffer,
                                 const ui::ClipboardDataEndpoint* data_dst,
                                 base::string16* markup,
                                 std::string* src_url,
                                 uint32_t* fragment_start,
                                 uint32_t* fragment_end) const {
  markup->clear();
  src_url->clear();
  const DataStore& store = GetStore(buffer);
  auto it = store.data.find(ui::ClipboardFormatType::GetHtmlType());
  if (it != store.data.end())
    *markup = base::UTF8ToUTF16(it->second);
  *src_url = store.html_src_url;
  *fragment_start = 0;
  *fragment_end = base::checked_cast<uint32_t>(markup->size());
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadSvg(ui::ClipboardBuffer buffer,
                                const ui::ClipboardDataEndpoint* data_dst,
                                base::string16* result) const {
  result->clear();
  const DataStore& store = GetStore(buffer);
  auto it = store.data.find(ui::ClipboardFormatType::GetSvgType());
  if (it != store.data.end())
    *result = base::UTF8ToUTF16(it->second);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadRTF(ui::ClipboardBuffer buffer,
                                const ui::ClipboardDataEndpoint* data_dst,
                                std::string* result) const {
  result->clear();
  const DataStore& store = GetStore(buffer);
  auto it = store.data.find(ui::ClipboardFormatType::GetRtfType());
  if (it != store.data.end())
    *result = it->second;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadImage(ui::ClipboardBuffer buffer,
                                  const ui::ClipboardDataEndpoint* data_dst,
                                  ReadImageCallback callback) const {
  std::move(callback).Run(GetStore(buffer).image);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadCustomData(
    ui::ClipboardBuffer clipboard_buffer,
    const base::string16& type,
    const ui::ClipboardDataEndpoint* data_dst,
    base::string16* result) const {}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadBookmark(const ui::ClipboardDataEndpoint* data_dst,
                                     base::string16* title,
                                     std::string* url) const {
  const DataStore& store = GetDefaultStore();
  auto it = store.data.find(ui::ClipboardFormatType::GetUrlType());
  if (it != store.data.end())
    *url = it->second;
  *title = base::UTF8ToUTF16(store.url_title);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::ReadData(const ui::ClipboardFormatType& format,
                                 const ui::ClipboardDataEndpoint* data_dst,
                                 std::string* result) const {
  result->clear();
  const DataStore& store = GetDefaultStore();
  auto it = store.data.find(format);
  if (it != store.data.end())
    *result = it->second;
}

#if defined(USE_OZONE)
bool HeadlessClipboard::IsSelectionBufferAvailable() const {
  return false;
}
#endif  // defined(USE_OZONE)

// |data_src| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::WritePortableRepresentations(
    ui::ClipboardBuffer buffer,
    const ObjectMap& objects,
    std::unique_ptr<ui::ClipboardDataEndpoint> data_src) {
  Clear(buffer);
  default_store_buffer_ = buffer;
  for (const auto& kv : objects)
    DispatchPortableRepresentation(kv.first, kv.second);
  default_store_buffer_ = ui::ClipboardBuffer::kCopyPaste;
}

// |data_src| is not used. It's only passed to be consistent with other
// platforms.
void HeadlessClipboard::WritePlatformRepresentations(
    ui::ClipboardBuffer buffer,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<ui::ClipboardDataEndpoint> data_src) {
  Clear(buffer);
  default_store_buffer_ = buffer;
  DispatchPlatformRepresentations(std::move(platform_representations));
  default_store_buffer_ = ui::ClipboardBuffer::kCopyPaste;
}

void HeadlessClipboard::WriteText(const char* text_data, size_t text_len) {
  std::string text(text_data, text_len);
  GetDefaultStore().data[ui::ClipboardFormatType::GetPlainTextType()] = text;
  if (IsSupportedClipboardBuffer(ui::ClipboardBuffer::kSelection)) {
    GetStore(ui::ClipboardBuffer::kSelection)
        .data[ui::ClipboardFormatType::GetPlainTextType()] = text;
  }
}

void HeadlessClipboard::WriteHTML(const char* markup_data,
                                  size_t markup_len,
                                  const char* url_data,
                                  size_t url_len) {
  base::string16 markup;
  base::UTF8ToUTF16(markup_data, markup_len, &markup);
  GetDefaultStore().data[ui::ClipboardFormatType::GetHtmlType()] =
      base::UTF16ToUTF8(markup);
  GetDefaultStore().html_src_url = std::string(url_data, url_len);
}

void HeadlessClipboard::WriteSvg(const char* markup_data, size_t markup_len) {
  std::string markup(markup_data, markup_len);
  GetDefaultStore().data[ui::ClipboardFormatType::GetSvgType()] = markup;
}

void HeadlessClipboard::WriteRTF(const char* rtf_data, size_t data_len) {
  GetDefaultStore().data[ui::ClipboardFormatType::GetRtfType()] =
      std::string(rtf_data, data_len);
}

void HeadlessClipboard::WriteBookmark(const char* title_data,
                                      size_t title_len,
                                      const char* url_data,
                                      size_t url_len) {
  GetDefaultStore().data[ui::ClipboardFormatType::GetUrlType()] =
      std::string(url_data, url_len);
  GetDefaultStore().url_title = std::string(title_data, title_len);
}

void HeadlessClipboard::WriteWebSmartPaste() {
  // Create a dummy entry.
  GetDefaultStore().data[ui::ClipboardFormatType::GetWebKitSmartPasteType()];
}

void HeadlessClipboard::WriteBitmap(const SkBitmap& bitmap) {
  // Create a dummy entry.
  GetDefaultStore().data[ui::ClipboardFormatType::GetBitmapType()];
  SkBitmap& dst = GetDefaultStore().image;
  if (dst.tryAllocPixels(bitmap.info())) {
    bitmap.readPixels(dst.info(), dst.getPixels(), dst.rowBytes(), 0, 0);
  }
}

void HeadlessClipboard::WriteData(const ui::ClipboardFormatType& format,
                                  const char* data_data,
                                  size_t data_len) {
  GetDefaultStore().data[format] = std::string(data_data, data_len);
}

HeadlessClipboard::DataStore::DataStore() : sequence_number(0) {}

HeadlessClipboard::DataStore::DataStore(const DataStore& other) = default;

HeadlessClipboard::DataStore::~DataStore() = default;

void HeadlessClipboard::DataStore::Clear() {
  data.clear();
  url_title.clear();
  html_src_url.clear();
  image = SkBitmap();
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
  ++store.sequence_number;
  return store;
}

const HeadlessClipboard::DataStore& HeadlessClipboard::GetDefaultStore() const {
  return GetStore(default_store_buffer_);
}

HeadlessClipboard::DataStore& HeadlessClipboard::GetDefaultStore() {
  return GetStore(default_store_buffer_);
}

}  // namespace headless
