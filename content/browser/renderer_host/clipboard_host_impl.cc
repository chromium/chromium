// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/clipboard_host_impl.h"

#include <utility>

#include "base/location.h"
#include "base/macros.h"
#include "base/pickle.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "url/gurl.h"

namespace content {

ClipboardHostImpl::ClipboardHostImpl(blink::mojom::ClipboardHostRequest request)
    : binding_(this, std::move(request)),
      clipboard_(ui::Clipboard::GetForCurrentThread()),
      clipboard_writer_(
          new ui::ScopedClipboardWriter(ui::CLIPBOARD_TYPE_COPY_PASTE)) {}

void ClipboardHostImpl::Create(blink::mojom::ClipboardHostRequest request) {
  // Clipboard implementations do interesting things, like run nested message
  // loops. Since StrongBinding<T> synchronously destroys on failure, that can
  // result in some unfortunate use-after-frees after the nested message loops
  // exit.
  auto* host = new ClipboardHostImpl(std::move(request));
  host->binding_.set_connection_error_handler(base::BindOnce(
      [](ClipboardHostImpl* host) {
        base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, host);
      },
      host));
}

ClipboardHostImpl::~ClipboardHostImpl() {
  clipboard_writer_->Reset();
}

void ClipboardHostImpl::GetSequenceNumber(ui::ClipboardType clipboard_type,
                                          GetSequenceNumberCallback callback) {
  std::move(callback).Run(clipboard_->GetSequenceNumber(clipboard_type));
}

void ClipboardHostImpl::ReadAvailableTypes(
    ui::ClipboardType clipboard_type,
    ReadAvailableTypesCallback callback) {
  std::vector<base::string16> types;
  bool contains_filenames;
  clipboard_->ReadAvailableTypes(clipboard_type, &types, &contains_filenames);
  std::move(callback).Run(types, contains_filenames);
}

void ClipboardHostImpl::IsFormatAvailable(blink::mojom::ClipboardFormat format,
                                          ui::ClipboardType clipboard_type,
                                          IsFormatAvailableCallback callback) {
  bool result = false;
  switch (format) {
    case blink::mojom::ClipboardFormat::kPlaintext:
      result = clipboard_->IsFormatAvailable(
                   ui::Clipboard::GetPlainTextWFormatType(), clipboard_type) ||
               clipboard_->IsFormatAvailable(
                   ui::Clipboard::GetPlainTextFormatType(), clipboard_type);
      break;
    case blink::mojom::ClipboardFormat::kHtml:
      result = clipboard_->IsFormatAvailable(ui::Clipboard::GetHtmlFormatType(),
                                             clipboard_type);
      break;
    case blink::mojom::ClipboardFormat::kSmartPaste:
      result = clipboard_->IsFormatAvailable(
          ui::Clipboard::GetWebKitSmartPasteFormatType(), clipboard_type);
      break;
    case blink::mojom::ClipboardFormat::kBookmark:
#if defined(OS_WIN) || defined(OS_MACOSX)
      result = clipboard_->IsFormatAvailable(ui::Clipboard::GetUrlWFormatType(),
                                             clipboard_type);
#else
      result = false;
#endif
      break;
  }
  std::move(callback).Run(result);
}

void ClipboardHostImpl::ReadText(ui::ClipboardType clipboard_type,
                                 ReadTextCallback callback) {
  base::string16 result;
  if (clipboard_->IsFormatAvailable(ui::Clipboard::GetPlainTextWFormatType(),
                                    clipboard_type)) {
    clipboard_->ReadText(clipboard_type, &result);
  } else if (clipboard_->IsFormatAvailable(
                 ui::Clipboard::GetPlainTextFormatType(), clipboard_type)) {
    std::string ascii;
    clipboard_->ReadAsciiText(clipboard_type, &ascii);
    result = base::ASCIIToUTF16(ascii);
  }
  std::move(callback).Run(result);
}

void ClipboardHostImpl::ReadHtml(ui::ClipboardType clipboard_type,
                                 ReadHtmlCallback callback) {
  base::string16 markup;
  std::string src_url_str;
  uint32_t fragment_start = 0;
  uint32_t fragment_end = 0;
  clipboard_->ReadHTML(clipboard_type, &markup, &src_url_str, &fragment_start,
                       &fragment_end);
  std::move(callback).Run(std::move(markup), GURL(src_url_str), fragment_start,
                          fragment_end);
}

void ClipboardHostImpl::ReadRtf(ui::ClipboardType clipboard_type,
                                ReadRtfCallback callback) {
  std::string result;
  clipboard_->ReadRTF(clipboard_type, &result);
  std::move(callback).Run(result);
}

void ClipboardHostImpl::ReadImage(ui::ClipboardType clipboard_type,
                                  ReadImageCallback callback) {
  SkBitmap result = clipboard_->ReadImage(clipboard_type);
  std::move(callback).Run(result);
}

void ClipboardHostImpl::ReadCustomData(ui::ClipboardType clipboard_type,
                                       const base::string16& type,
                                       ReadCustomDataCallback callback) {
  base::string16 result;
  clipboard_->ReadCustomData(clipboard_type, type, &result);
  std::move(callback).Run(result);
}

void ClipboardHostImpl::WriteText(ui::ClipboardType,
                                  const base::string16& text) {
  clipboard_writer_->WriteText(text);
}

void ClipboardHostImpl::WriteHtml(ui::ClipboardType,
                                  const base::string16& markup,
                                  const GURL& url) {
  clipboard_writer_->WriteHTML(markup, url.spec());
}

void ClipboardHostImpl::WriteSmartPasteMarker(ui::ClipboardType) {
  clipboard_writer_->WriteWebSmartPaste();
}

void ClipboardHostImpl::WriteCustomData(
    ui::ClipboardType,
    const base::flat_map<base::string16, base::string16>& data) {
  base::Pickle pickle;
  ui::WriteCustomDataToPickle(data, &pickle);
  clipboard_writer_->WritePickledData(
      pickle, ui::Clipboard::GetWebCustomDataFormatType());
}

void ClipboardHostImpl::WriteBookmark(ui::ClipboardType,
                                      const std::string& url,
                                      const base::string16& title) {
  clipboard_writer_->WriteBookmark(title, url);
}

void ClipboardHostImpl::WriteImage(ui::ClipboardType, const SkBitmap& bitmap) {
  clipboard_writer_->WriteImage(bitmap);
}

void ClipboardHostImpl::CommitWrite(ui::ClipboardType) {
  clipboard_writer_.reset(
      new ui::ScopedClipboardWriter(ui::CLIPBOARD_TYPE_COPY_PASTE));
}

}  // namespace content
