// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/mock_clipboard_host.h"

#include "build/build_config.h"

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
  url_ = KURL();
  image_.reset();
  custom_data_.clear();
  write_smart_paste_ = false;
  needs_reset_ = false;
}

void MockClipboardHost::GetSequenceNumber(
    mojom::ClipboardBuffer clipboard_buffer,
    GetSequenceNumberCallback callback) {
  std::move(callback).Run(sequence_number_);
}

void MockClipboardHost::ReadAvailableTypes(
    mojom::ClipboardBuffer clipboard_buffer,
    ReadAvailableTypesCallback callback) {
  Vector<String> types;
  if (!plain_text_.IsEmpty())
    types.push_back("text/plain");
  if (!html_text_.IsEmpty())
    types.push_back("text/html");
  if (!svg_text_.IsEmpty())
    types.push_back("image/svg+xml");
  if (!image_.isNull())
    types.push_back("image/png");
  for (auto& it : custom_data_) {
    CHECK(!base::Contains(types, it.key));
    types.push_back(it.key);
  }
  std::move(callback).Run(types);
}

void MockClipboardHost::IsFormatAvailable(
    mojom::ClipboardFormat format,
    mojom::ClipboardBuffer clipboard_buffer,
    IsFormatAvailableCallback callback) {
  bool result = false;
  switch (format) {
    case mojom::ClipboardFormat::kPlaintext:
      result = !plain_text_.IsEmpty();
      break;
    case mojom::ClipboardFormat::kHtml:
      result = !html_text_.IsEmpty();
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
  std::move(callback).Run(g_empty_string);
}

void MockClipboardHost::ReadImage(mojom::ClipboardBuffer clipboard_buffer,
                                  ReadImageCallback callback) {
  std::move(callback).Run(image_);
}

void MockClipboardHost::ReadCustomData(mojom::ClipboardBuffer clipboard_buffer,
                                       const String& type,
                                       ReadCustomDataCallback callback) {
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

void MockClipboardHost::WriteCustomData(const HashMap<String, String>& data) {
  if (needs_reset_)
    Reset();
  for (auto& it : data)
    custom_data_.Set(it.key, it.value);
}

void MockClipboardHost::WriteBookmark(const String& url, const String& title) {}

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
void MockClipboardHost::WriteStringToFindPboard(const String& text) {}
#endif

}  // namespace blink
