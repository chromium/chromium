// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/clipboard_host_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/pickle.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_data_endpoint.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "url/gurl.h"

namespace content {

// 5 mins is based on the timeout in BinaryUploadService. This scanning timeout
// of 5 mins means no paste will be held back longer before being allowed or
// blocked, so matching this timeout with the threshold for a paste being too
// old ensures scans that:
//  - Scans that timeout can be retried without waiting
//  - Scans that succeed will apply their verdicts without the risk that their
//    associated IsPasteAllowedRequest is already too old.
const base::TimeDelta ClipboardHostImpl::kIsPasteAllowedRequestTooOld =
    base::TimeDelta::FromMinutes(5);

ClipboardHostImpl::IsPasteAllowedRequest::IsPasteAllowedRequest() = default;
ClipboardHostImpl::IsPasteAllowedRequest::~IsPasteAllowedRequest() = default;

bool ClipboardHostImpl::IsPasteAllowedRequest::AddCallback(
    IsClipboardPasteAllowedCallback callback) {
  // If this request has already completed, invoke the callback immediately
  // and return.
  if (allowed_.has_value()) {
    std::move(callback).Run(allowed_.value());
    return false;
  }

  callbacks_.push_back(std::move(callback));

  // If this is the first callback registered tell the caller to start the scan.
  return callbacks_.size() == 1;
}

void ClipboardHostImpl::IsPasteAllowedRequest::Complete(
    ClipboardPasteAllowed allowed) {
  allowed_ = allowed;
  InvokeCallbacks();
}

bool ClipboardHostImpl::IsPasteAllowedRequest::IsObsolete(base::Time now) {
  // If the request is old and no longer has any registered callbacks it is
  // obsolete.
  return (now - time_) > kIsPasteAllowedRequestTooOld && callbacks_.empty();
}

void ClipboardHostImpl::IsPasteAllowedRequest::InvokeCallbacks() {
  DCHECK(allowed_);

  auto callbacks = std::move(callbacks_);
  for (auto& callback : callbacks) {
    if (!callback.is_null())
      std::move(callback).Run(allowed_.value());
  }
}

ClipboardHostImpl::ClipboardHostImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver)
    : receiver_(this, std::move(receiver)),
      clipboard_(ui::Clipboard::GetForCurrentThread()) {
  // |render_frame_host| may be null in unit tests.
  if (render_frame_host) {
    render_frame_routing_id_ = render_frame_host->GetRoutingID();
    render_process_id_ = render_frame_host->GetProcess()->GetID();
    clipboard_writer_ = std::make_unique<ui::ScopedClipboardWriter>(
        ui::ClipboardBuffer::kCopyPaste,
        std::make_unique<ui::ClipboardDataEndpoint>(
            render_frame_host->GetLastCommittedOrigin()));
  } else {
    clipboard_writer_ = std::make_unique<ui::ScopedClipboardWriter>(
        ui::ClipboardBuffer::kCopyPaste);
  }
}

void ClipboardHostImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver) {
  // Clipboard implementations do interesting things, like run nested message
  // loops. Use manual memory management instead of SelfOwnedReceiver<T> which
  // synchronously destroys on failure and can result in some unfortunate
  // use-after-frees after the nested message loops exit.
  auto* host = new ClipboardHostImpl(
      static_cast<RenderFrameHostImpl*>(render_frame_host),
      std::move(receiver));
  host->receiver_.set_disconnect_handler(base::BindOnce(
      [](ClipboardHostImpl* host) {
        base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, host);
      },
      host));
}

ClipboardHostImpl::~ClipboardHostImpl() {
  clipboard_writer_->Reset();
}

void ClipboardHostImpl::GetSequenceNumber(ui::ClipboardBuffer clipboard_buffer,
                                          GetSequenceNumberCallback callback) {
  std::move(callback).Run(clipboard_->GetSequenceNumber(clipboard_buffer));
}

void ClipboardHostImpl::ReadAvailableTypes(
    ui::ClipboardBuffer clipboard_buffer,
    ReadAvailableTypesCallback callback) {
  std::vector<base::string16> types;
  clipboard_->ReadAvailableTypes(clipboard_buffer, CreateDataEndpoint().get(),
                                 &types);
  std::move(callback).Run(types);
}

void ClipboardHostImpl::IsFormatAvailable(blink::mojom::ClipboardFormat format,
                                          ui::ClipboardBuffer clipboard_buffer,
                                          IsFormatAvailableCallback callback) {
  bool result = false;
  auto data_endpoint = CreateDataEndpoint();
  switch (format) {
    case blink::mojom::ClipboardFormat::kPlaintext:
      result = clipboard_->IsFormatAvailable(
          ui::ClipboardFormatType::GetPlainTextType(), clipboard_buffer,
          data_endpoint.get());
#if defined(OS_WIN)
      result |= clipboard_->IsFormatAvailable(
          ui::ClipboardFormatType::GetPlainTextAType(), clipboard_buffer,
          data_endpoint.get());
#endif
      break;
    case blink::mojom::ClipboardFormat::kHtml:
      result =
          clipboard_->IsFormatAvailable(ui::ClipboardFormatType::GetHtmlType(),
                                        clipboard_buffer, data_endpoint.get());
      break;
    case blink::mojom::ClipboardFormat::kSmartPaste:
      result = clipboard_->IsFormatAvailable(
          ui::ClipboardFormatType::GetWebKitSmartPasteType(), clipboard_buffer,
          data_endpoint.get());
      break;
    case blink::mojom::ClipboardFormat::kBookmark:
#if defined(OS_WIN) || defined(OS_MAC)
      result =
          clipboard_->IsFormatAvailable(ui::ClipboardFormatType::GetUrlType(),
                                        clipboard_buffer, data_endpoint.get());
#else
      result = false;
#endif
      break;
  }
  std::move(callback).Run(result);
}

void ClipboardHostImpl::ReadText(ui::ClipboardBuffer clipboard_buffer,
                                 ReadTextCallback callback) {
  base::string16 result;
  auto data_dst = CreateDataEndpoint();
  if (clipboard_->IsFormatAvailable(ui::ClipboardFormatType::GetPlainTextType(),
                                    clipboard_buffer, data_dst.get())) {
    clipboard_->ReadText(clipboard_buffer, data_dst.get(), &result);
  } else {
#if defined(OS_WIN)
    if (clipboard_->IsFormatAvailable(
            ui::ClipboardFormatType::GetPlainTextAType(), clipboard_buffer,
            data_dst.get())) {
      std::string ascii;
      clipboard_->ReadAsciiText(clipboard_buffer, data_dst.get(), &ascii);
      result = base::ASCIIToUTF16(ascii);
    }
#endif
  }

  std::string data = base::UTF16ToUTF8(result);
  PerformPasteIfAllowed(clipboard_->GetSequenceNumber(clipboard_buffer),
                        ui::ClipboardFormatType::GetPlainTextType(),
                        std::move(data),
                        base::BindOnce(
                            [](base::string16 result, ReadTextCallback callback,
                               ClipboardPasteAllowed allowed) {
                              if (!allowed)
                                result.clear();
                              std::move(callback).Run(result);
                            },
                            std::move(result), std::move(callback)));
}

void ClipboardHostImpl::ReadHtml(ui::ClipboardBuffer clipboard_buffer,
                                 ReadHtmlCallback callback) {
  base::string16 markup;
  std::string src_url_str;
  uint32_t fragment_start = 0;
  uint32_t fragment_end = 0;
  auto data_dst = CreateDataEndpoint();
  clipboard_->ReadHTML(clipboard_buffer, data_dst.get(), &markup, &src_url_str,
                       &fragment_start, &fragment_end);

  std::string data = base::UTF16ToUTF8(markup);
  PerformPasteIfAllowed(
      clipboard_->GetSequenceNumber(clipboard_buffer),
      ui::ClipboardFormatType::GetHtmlType(), std::move(data),
      base::BindOnce(
          [](base::string16 markup, std::string src_url_str,
             uint32_t fragment_start, uint32_t fragment_end,
             ReadHtmlCallback callback, ClipboardPasteAllowed allowed) {
            if (!allowed)
              markup.clear();
            std::move(callback).Run(std::move(markup), GURL(src_url_str),
                                    fragment_start, fragment_end);
          },
          std::move(markup), std::move(src_url_str), fragment_start,
          fragment_end, std::move(callback)));
}

void ClipboardHostImpl::ReadSvg(ui::ClipboardBuffer clipboard_buffer,
                                ReadSvgCallback callback) {
  base::string16 markup;
  clipboard_->ReadSvg(clipboard_buffer, /*data_dst=*/nullptr, &markup);

  std::string data = base::UTF16ToUTF8(markup);
  PerformPasteIfAllowed(clipboard_->GetSequenceNumber(clipboard_buffer),
                        ui::ClipboardFormatType::GetSvgType(), std::move(data),
                        base::BindOnce(
                            [](base::string16 markup, ReadSvgCallback callback,
                               ClipboardPasteAllowed allowed) {
                              if (!allowed)
                                markup.clear();
                              std::move(callback).Run(std::move(markup));
                            },
                            std::move(markup), std::move(callback)));
}

void ClipboardHostImpl::ReadRtf(ui::ClipboardBuffer clipboard_buffer,
                                ReadRtfCallback callback) {
  std::string result;
  auto data_dst = CreateDataEndpoint();
  clipboard_->ReadRTF(clipboard_buffer, data_dst.get(), &result);

  std::string data = result;
  PerformPasteIfAllowed(clipboard_->GetSequenceNumber(clipboard_buffer),
                        ui::ClipboardFormatType::GetRtfType(), std::move(data),
                        base::BindOnce(
                            [](std::string result, ReadRtfCallback callback,
                               ClipboardPasteAllowed allowed) {
                              if (!allowed)
                                result.clear();
                              std::move(callback).Run(result);
                            },
                            std::move(result), std::move(callback)));
}

void ClipboardHostImpl::ReadImage(ui::ClipboardBuffer clipboard_buffer,
                                  ReadImageCallback callback) {
  auto data_dst = CreateDataEndpoint();
  clipboard_->ReadImage(clipboard_buffer, data_dst.get(),
                        base::BindOnce(&ClipboardHostImpl::OnReadImage,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       clipboard_buffer, std::move(callback)));
}

void ClipboardHostImpl::OnReadImage(ui::ClipboardBuffer clipboard_buffer,
                                    ReadImageCallback callback,
                                    const SkBitmap& bitmap) {
  std::string data =
      std::string(reinterpret_cast<const char*>(bitmap.getPixels()),
                  bitmap.computeByteSize());
  PerformPasteIfAllowed(clipboard_->GetSequenceNumber(clipboard_buffer),
                        ui::ClipboardFormatType::GetBitmapType(),
                        std::move(data),
                        base::BindOnce(
                            [](SkBitmap bitmap, ReadImageCallback callback,
                               ClipboardPasteAllowed allowed) {
                              if (!allowed)
                                bitmap.reset();
                              std::move(callback).Run(bitmap);
                            },
                            std::move(bitmap), std::move(callback)));
}

void ClipboardHostImpl::ReadCustomData(ui::ClipboardBuffer clipboard_buffer,
                                       const base::string16& type,
                                       ReadCustomDataCallback callback) {
  base::string16 result;
  auto data_dst = CreateDataEndpoint();
  clipboard_->ReadCustomData(clipboard_buffer, type, data_dst.get(), &result);

  std::string data = base::UTF16ToUTF8(result);
  PerformPasteIfAllowed(
      clipboard_->GetSequenceNumber(clipboard_buffer),
      ui::ClipboardFormatType::GetWebCustomDataType(), std::move(data),
      base::BindOnce(
          [](base::string16 result, ReadCustomDataCallback callback,
             ClipboardPasteAllowed allowed) {
            if (!allowed)
              result.clear();
            std::move(callback).Run(result);
          },
          std::move(result), std::move(callback)));
}

void ClipboardHostImpl::WriteText(const base::string16& text) {
  clipboard_writer_->WriteText(text);
}

void ClipboardHostImpl::WriteHtml(const base::string16& markup,
                                  const GURL& url) {
  clipboard_writer_->WriteHTML(markup, url.spec());
}

void ClipboardHostImpl::WriteSvg(const base::string16& markup) {
  clipboard_writer_->WriteSvg(markup);
}

void ClipboardHostImpl::WriteSmartPasteMarker() {
  clipboard_writer_->WriteWebSmartPaste();
}

void ClipboardHostImpl::WriteCustomData(
    const base::flat_map<base::string16, base::string16>& data) {
  base::Pickle pickle;
  ui::WriteCustomDataToPickle(data, &pickle);
  clipboard_writer_->WritePickledData(
      pickle, ui::ClipboardFormatType::GetWebCustomDataType());
}

void ClipboardHostImpl::WriteBookmark(const std::string& url,
                                      const base::string16& title) {
  clipboard_writer_->WriteBookmark(title, url);
}

void ClipboardHostImpl::WriteImage(const SkBitmap& unsafe_bitmap) {
  SkBitmap bitmap;
  // On receipt of an arbitrary bitmap from the renderer, we convert to an N32
  // 32bpp bitmap. Other pixel sizes can lead to out-of-bounds mistakes when
  // transferring the pixels out of the bitmap into other buffers.
  if (!skia::SkBitmapToN32OpaqueOrPremul(unsafe_bitmap, &bitmap)) {
    NOTREACHED() << "Unable to convert bitmap for clipboard";
    return;
  }
  clipboard_writer_->WriteImage(bitmap);
}

void ClipboardHostImpl::CommitWrite() {
  clipboard_writer_ = std::make_unique<ui::ScopedClipboardWriter>(
      ui::ClipboardBuffer::kCopyPaste, CreateDataEndpoint());
}

void ClipboardHostImpl::PerformPasteIfAllowed(
    uint64_t seqno,
    const ui::ClipboardFormatType& data_type,
    std::string data,
    IsClipboardPasteAllowedCallback callback) {
  CleanupObsoleteRequests();

  if (data.empty()) {
    std::move(callback).Run(ClipboardPasteAllowed(true));
    return;
  }

  // Add |callback| to the callbacks associated to the sequence number, adding
  // an entry to the map if one does not exist.
  auto& request = is_allowed_requests_[seqno];
  if (request.AddCallback(std::move(callback)))
    StartIsPasteAllowedRequest(seqno, data_type, std::move(data));
}

void ClipboardHostImpl::StartIsPasteAllowedRequest(
    uint64_t seqno,
    const ui::ClipboardFormatType& data_type,
    std::string data) {
  // May not have a RenderFrameHost in tests.
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id_, render_frame_routing_id_);
  if (render_frame_host) {
    render_frame_host->IsClipboardPasteAllowed(
        data_type, data,
        base::BindOnce(&ClipboardHostImpl::FinishPasteIfAllowed,
                       weak_ptr_factory_.GetWeakPtr(), seqno));
  } else {
    FinishPasteIfAllowed(seqno, ClipboardPasteAllowed(true));
  }
}

void ClipboardHostImpl::FinishPasteIfAllowed(uint64_t seqno,
                                             ClipboardPasteAllowed allowed) {
  if (is_allowed_requests_.count(seqno) == 0)
    return;

  auto& request = is_allowed_requests_[seqno];
  request.Complete(allowed);
}

void ClipboardHostImpl::CleanupObsoleteRequests() {
  for (auto it = is_allowed_requests_.begin();
       it != is_allowed_requests_.end();) {
    it = it->second.IsObsolete(base::Time::Now())
             ? is_allowed_requests_.erase(it)
             : std::next(it);
  }
}

std::unique_ptr<ui::ClipboardDataEndpoint>
ClipboardHostImpl::CreateDataEndpoint() {
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id_, render_frame_routing_id_);
  if (render_frame_host) {
    return std::make_unique<ui::ClipboardDataEndpoint>(
        render_frame_host->GetLastCommittedOrigin());
  }
  return nullptr;
}
}  // namespace content
