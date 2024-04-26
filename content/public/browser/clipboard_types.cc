// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/clipboard_types.h"

#include "base/functional/callback_helpers.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

ClipboardPasteData::ClipboardPasteData() = default;
ClipboardPasteData::ClipboardPasteData(const ClipboardPasteData&) = default;
ClipboardPasteData& ClipboardPasteData::operator=(const ClipboardPasteData&) =
    default;
ClipboardPasteData::ClipboardPasteData(ClipboardPasteData&&) = default;
ClipboardPasteData& ClipboardPasteData::operator=(ClipboardPasteData&&) =
    default;

bool ClipboardPasteData::empty() const {
  return text.empty() && html.empty() && svg.empty() && rtf.empty() &&
         png.empty() && bitmap.empty() && file_paths.empty() &&
         custom_data.empty();
}

size_t ClipboardPasteData::size() const {
  size_t size = text.size() + html.size() + svg.size() + rtf.size() +
                png.size() + bitmap.computeByteSize();
  for (const auto& entry : custom_data) {
    size += entry.second.size();
  }
  return size;
}

void ClipboardPasteData::Merge(ClipboardPasteData other) {
  if (!other.text.empty()) {
    text = std::move(other.text);
  }

  if (!other.html.empty()) {
    html = std::move(other.html);
  }

  if (!other.svg.empty()) {
    svg = std::move(other.svg);
  }

  if (!other.rtf.empty()) {
    rtf = std::move(other.rtf);
  }

  if (!other.png.empty()) {
    png = std::move(other.png);
  }

  if (!other.bitmap.empty()) {
    bitmap = std::move(other.bitmap);
  }

  if (!other.file_paths.empty()) {
    file_paths = std::move(other.file_paths);
  }

  if (!other.custom_data.empty()) {
    for (auto& entry : other.custom_data) {
      custom_data[entry.first] = std::move(entry.second);
    }
  }
}

ClipboardPasteData::~ClipboardPasteData() = default;

ClipboardEndpoint::ClipboardEndpoint(
    base::optional_ref<const ui::DataTransferEndpoint> data_transfer_endpoint)
    : data_transfer_endpoint_(data_transfer_endpoint.CopyAsOptional()) {}

ClipboardEndpoint::ClipboardEndpoint(
    base::optional_ref<const ui::DataTransferEndpoint> data_transfer_endpoint,
    base::RepeatingCallback<BrowserContext*()> browser_context_fetcher,
    RenderFrameHost& rfh)
    : data_transfer_endpoint_(data_transfer_endpoint.CopyAsOptional()),
      browser_context_fetcher_(std::move(browser_context_fetcher)),
      web_contents_(WebContents::FromRenderFrameHost(&rfh)->GetWeakPtr()) {}

ClipboardEndpoint::ClipboardEndpoint(const ClipboardEndpoint&) = default;
ClipboardEndpoint& ClipboardEndpoint::operator=(const ClipboardEndpoint&) =
    default;
ClipboardEndpoint::~ClipboardEndpoint() = default;

BrowserContext* ClipboardEndpoint::browser_context() const {
  if (browser_context_fetcher_) {
    return browser_context_fetcher_.Run();
  }
  return nullptr;
}

WebContents* ClipboardEndpoint::web_contents() const {
  return web_contents_.get();
}

}  // namespace content
