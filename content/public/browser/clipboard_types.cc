// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/clipboard_types.h"

#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/types/optional_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "ui/base/clipboard/clipboard.h"

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

const ui::ClipboardFormatType& SourceRFHTokenType() {
#if BUILDFLAG(IS_APPLE)
  constexpr char kTypeName[] = "org.chromium.internal.source-rfh-token";
#elif BUILDFLAG(IS_WIN)
  constexpr char kTypeName[] = "Chromium internal source RFH token";
#else
  constexpr char kTypeName[] = "chromium/x-internal-source-rfh-token";
#endif
  static base::NoDestructor<ui::ClipboardFormatType> type(
      ui::ClipboardFormatType::CustomPlatformType(kTypeName));
  return *type;
}

void OnReadSourceRFHToken(ui::ClipboardBuffer clipboard_buffer,
                          base::OnceCallback<void(ClipboardEndpoint)> callback,
                          std::string result) {
  auto rfh_token = GlobalRenderFrameHostToken::FromPickle(
      base::Pickle::WithData(base::as_byte_span(result)));

  ui::Clipboard::GetForCurrentThread()->GetSource(
      clipboard_buffer,
      base::BindOnce(
          [](std::optional<GlobalRenderFrameHostToken> rfh_token,
             base::OnceCallback<void(ClipboardEndpoint)> callback,
             std::optional<ui::DataTransferEndpoint> clipboard_source_dte) {
            RenderFrameHost* rfh = nullptr;
            if (rfh_token) {
              rfh = RenderFrameHost::FromFrameToken(*rfh_token);
            }

            if (!rfh) {
              std::move(callback).Run(ClipboardEndpoint(clipboard_source_dte));
              return;
            }

            std::optional<ui::DataTransferEndpoint> source_dte;
            if (clipboard_source_dte) {
              if (clipboard_source_dte->IsUrlType()) {
                source_dte = std::make_optional<ui::DataTransferEndpoint>(
                    *clipboard_source_dte->GetURL(),
                    ui::DataTransferEndpointOptions{
                        .off_the_record =
                            rfh->GetBrowserContext()->IsOffTheRecord()});
              } else {
                source_dte = std::move(clipboard_source_dte);
              }
            }

            std::move(callback).Run(ClipboardEndpoint(
                std::move(source_dte),
                base::BindRepeating(
                    [](GlobalRenderFrameHostToken rfh_token)
                        -> BrowserContext* {
                      auto* rfh = RenderFrameHost::FromFrameToken(rfh_token);
                      if (!rfh) {
                        return nullptr;
                      }
                      return rfh->GetBrowserContext();
                    },
                    rfh->GetGlobalFrameToken()),
                *rfh));
          },
          rfh_token, std::move(callback)));
}

void GetSourceClipboardEndpoint(
    const ui::DataTransferEndpoint* data_dst,
    ui::ClipboardBuffer clipboard_buffer,
    base::OnceCallback<void(ClipboardEndpoint)> callback) {
  ui::Clipboard::GetForCurrentThread()->ReadData(
      SourceRFHTokenType(), base::OptionalFromPtr(data_dst),
      base::BindOnce(&OnReadSourceRFHToken, clipboard_buffer,
                     std::move(callback)));
}

void AddSourceDataToClipboardWriter(ui::ScopedClipboardWriter& clipboard_writer,
                                    content::RenderFrameHost& rfh) {
  clipboard_writer.SetDataSourceURL(rfh.GetMainFrame()->GetLastCommittedURL(),
                                    rfh.GetLastCommittedURL());
  clipboard_writer.WritePickledData(rfh.GetGlobalFrameToken().ToPickle(),
                                    SourceRFHTokenType());
}

std::optional<ui::DataTransferEndpoint> CreateDataEndpoint(
    content::RenderFrameHost& rfh) {
  auto* render_frame_host_main_frame = rfh.GetMainFrame();
  auto source_url = render_frame_host_main_frame->GetLastCommittedURL();
  if (!source_url.is_valid()) {
    return std::nullopt;
  }

  if (auto maybe_url = GetContentClient()
                           ->browser()
                           ->MaybeOverrideSourceURLForClipboardAccess(
                               render_frame_host_main_frame, source_url)) {
    source_url = *maybe_url;
  }

  return ui::DataTransferEndpoint(
      source_url,
      ui::DataTransferEndpointOptions{
          .notify_if_restricted = rfh.HasTransientUserActivation(),
          .off_the_record = rfh.GetBrowserContext()->IsOffTheRecord(),
      });
}

ClipboardEndpoint CreateClipboardEndpoint(content::RenderFrameHost& rfh) {
  return ClipboardEndpoint(
      CreateDataEndpoint(rfh),
      base::BindRepeating(
          [](GlobalRenderFrameHostId rfh_id) -> BrowserContext* {
            auto* rfh = RenderFrameHost::FromID(rfh_id);
            if (!rfh) {
              return nullptr;
            }
            return rfh->GetBrowserContext();
          },
          rfh.GetGlobalId()),
      rfh);
}

}  // namespace content
