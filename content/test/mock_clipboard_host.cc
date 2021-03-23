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
  plain_text_ = std::u16string();
  html_text_ = std::u16string();
  svg_text_ = std::u16string();
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
  std::vector<std::u16string> types;
  if (!plain_text_.empty())
    types.push_back(u"text/plain");
  if (!html_text_.empty())
    types.push_back(u"text/html");
  if (!svg_text_.empty())
    types.push_back(u"image/svg+xml");
  if (!image_.isNull())
    types.push_back(u"image/png");
  for (auto& it : custom_data_) {
    CHECK(!base::Contains(types, it.first));
    types.push_back(it.first);
  }
  std::move(callback).Run(types);
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

void MockClipboardHost::ReadImage(ui::ClipboardBuffer clipboard_buffer,
                                  ReadImageCallback callback) {
  std::move(callback).Run(image_);
}

void MockClipboardHost::ReadFiles(ui::ClipboardBuffer clipboard_buffer,
                                  ReadFilesCallback callback) {
  std::move(callback).Run(blink::mojom::ClipboardFiles::New());
}

void MockClipboardHost::ReadCustomData(ui::ClipboardBuffer clipboard_buffer,
                                       const std::u16string& type,
                                       ReadCustomDataCallback callback) {
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

void MockClipboardHost::WriteCustomData(
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
  image_ = bitmap;
}

void MockClipboardHost::CommitWrite() {
  ++sequence_number_;
  needs_reset_ = true;
}

#if defined(OS_MAC)
void MockClipboardHost::WriteStringToFindPboard(const std::u16string& text) {}
#endif

}  // namespace content
