// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_clipboard_host.h"

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"

namespace content {

MockClipboardHost::MockClipboardHost() = default;

MockClipboardHost::~MockClipboardHost() = default;

void MockClipboardHost::Bind(
    mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MockClipboardHost::Reset() {
  plain_text_ = base::string16();
  html_text_ = base::string16();
  url_ = GURL();
  image_.reset();
  custom_data_.clear();
  write_smart_paste_ = false;
  needs_reset_ = false;
}

void MockClipboardHost::GetSequenceNumber(ui::ClipboardBuffer clipboard_buffer,
                                          GetSequenceNumberCallback callback) {
  std::move(callback).Run(sequence_number_);
}

void MockClipboardHost::ReadAvailableTypes(
    ui::ClipboardBuffer clipboard_buffer,
    ReadAvailableTypesCallback callback) {
  std::vector<base::string16> types;
  if (!plain_text_.empty())
    types.push_back(base::UTF8ToUTF16("text/plain"));
  if (!html_text_.empty())
    types.push_back(base::UTF8ToUTF16("text/html"));
  if (!image_.isNull())
    types.push_back(base::UTF8ToUTF16("image/png"));
  for (auto& it : custom_data_) {
    CHECK(!base::Contains(types, it.first));
    types.push_back(it.first);
  }
  std::move(callback).Run(types, false);
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

void MockClipboardHost::ReadRtf(ui::ClipboardBuffer clipboard_buffer,
                                ReadRtfCallback callback) {
  std::move(callback).Run(std::string());
}

void MockClipboardHost::ReadImage(ui::ClipboardBuffer clipboard_buffer,
                                  ReadImageCallback callback) {
  std::move(callback).Run(image_);
}

void MockClipboardHost::ReadCustomData(ui::ClipboardBuffer clipboard_buffer,
                                       const base::string16& type,
                                       ReadCustomDataCallback callback) {
  auto it = custom_data_.find(type);
  std::move(callback).Run(it != custom_data_.end() ? it->second
                                                   : base::string16());
}

void MockClipboardHost::WriteText(const base::string16& text) {
  if (needs_reset_)
    Reset();
  plain_text_ = text;
}

void MockClipboardHost::WriteHtml(const base::string16& markup,
                                  const GURL& url) {
  if (needs_reset_)
    Reset();
  html_text_ = markup;
  url_ = url;
}

void MockClipboardHost::WriteSmartPasteMarker() {
  if (needs_reset_)
    Reset();
  write_smart_paste_ = true;
}

void MockClipboardHost::WriteCustomData(
    const base::flat_map<base::string16, base::string16>& data) {
  if (needs_reset_)
    Reset();
  for (auto& it : data)
    custom_data_[it.first] = it.second;
}

void MockClipboardHost::WriteRawData(const base::string16& format,
                                     mojo_base::BigBuffer data) {
  if (needs_reset_)
    Reset();
  raw_data_[format] = std::move(data);
}

void MockClipboardHost::WriteBookmark(const std::string& url,
                                      const base::string16& title) {}

void MockClipboardHost::WriteImage(const SkBitmap& bitmap) {
  if (needs_reset_)
    Reset();
  image_ = bitmap;
}

void MockClipboardHost::CommitWrite() {
  ++sequence_number_;
  needs_reset_ = true;
}

#if defined(OS_MACOSX)
void MockClipboardHost::WriteStringToFindPboard(const base::string16& text) {}
#endif

}  // namespace content
