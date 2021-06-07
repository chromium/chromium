// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/clipboard_host_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/containers/contains.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/data_transfer_util.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/drop_data.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "third_party/blink/public/mojom/page/drag.mojom-forward.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

namespace content {

namespace {
bool IsRendererPasteAllowed(
    const GlobalFrameRoutingId& render_frame_routing_id_) {
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_frame_routing_id_);
  if (!render_frame_host)
    return false;
  ContentBrowserClient* browser_client = GetContentClient()->browser();
  return browser_client->IsClipboardPasteAllowed(render_frame_host);
}
}  // namespace

// 5 mins is based on the timeout in BinaryUploadService. This scanning timeout
// of 5 mins means no paste will be held back longer before being allowed or
// blocked, so matching this timeout with the threshold for a paste being too
// old ensures scans that:
//  - Scans that timeout can be retried without waiting
//  - Scans that succeed will apply their verdicts without the risk that their
//    associated IsPasteContentAllowedRequest is already too old.
const base::TimeDelta ClipboardHostImpl::kIsPasteContentAllowedRequestTooOld =
    base::TimeDelta::FromMinutes(5);

ClipboardHostImpl::IsPasteContentAllowedRequest::
    IsPasteContentAllowedRequest() = default;
ClipboardHostImpl::IsPasteContentAllowedRequest::
    ~IsPasteContentAllowedRequest() = default;

bool ClipboardHostImpl::IsPasteContentAllowedRequest::AddCallback(
    IsClipboardPasteContentAllowedCallback callback) {
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

void ClipboardHostImpl::IsPasteContentAllowedRequest::Complete(
    ClipboardPasteContentAllowed allowed) {
  allowed_ = allowed;
  InvokeCallbacks();
}

bool ClipboardHostImpl::IsPasteContentAllowedRequest::IsObsolete(
    base::Time now) {
  // If the request is old and no longer has any registered callbacks it is
  // obsolete.
  return (now - time_) > kIsPasteContentAllowedRequestTooOld &&
         callbacks_.empty();
}

void ClipboardHostImpl::IsPasteContentAllowedRequest::InvokeCallbacks() {
  DCHECK(allowed_);

  auto callbacks = std::move(callbacks_);
  for (auto& callback : callbacks) {
    if (!callback.is_null())
      std::move(callback).Run(allowed_.value());
  }
}

ClipboardHostImpl::ClipboardHostImpl(RenderFrameHost* render_frame_host)
    : clipboard_(ui::Clipboard::GetForCurrentThread()) {
  // |render_frame_host| may be null in unit tests.
  if (render_frame_host) {
    render_frame_routing_id_ =
        GlobalFrameRoutingId(render_frame_host->GetProcess()->GetID(),
                             render_frame_host->GetRoutingID());
    clipboard_writer_ = std::make_unique<ui::ScopedClipboardWriter>(
        ui::ClipboardBuffer::kCopyPaste,
        std::make_unique<ui::DataTransferEndpoint>(
            render_frame_host->GetLastCommittedOrigin()));
  } else {
    render_frame_routing_id_ = GlobalFrameRoutingId(
        ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE);
    clipboard_writer_ = std::make_unique<ui::ScopedClipboardWriter>(
        ui::ClipboardBuffer::kCopyPaste);
  }
}

void ClipboardHostImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new ClipboardHostImpl(
          static_cast<RenderFrameHostImpl*>(render_frame_host))),
      std::move(receiver));
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
  std::vector<std::u16string> types;
  clipboard_->ReadAvailableTypes(clipboard_buffer, CreateDataEndpoint().get(),
                                 &types);
  // If files are available, do not include other types such as text/plain
  // which contain the full path on some platforms (http://crbug.com/1214108).
  std::u16string filenames_type = base::UTF8ToUTF16(ui::kMimeTypeURIList);
  if (base::Contains(types, filenames_type)) {
    types = {filenames_type};
  }
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
    case blink::mojom::ClipboardFormat::kRtf:
      result =
          clipboard_->IsFormatAvailable(ui::ClipboardFormatType::GetRtfType(),
                                        clipboard_buffer, data_endpoint.get());
      break;
  }
  std::move(callback).Run(result);
}

void ClipboardHostImpl::ReadText(ui::ClipboardBuffer clipboard_buffer,
                                 ReadTextCallback callback) {
  if (!IsRendererPasteAllowed(render_frame_routing_id_)) {
    std::move(callback).Run(std::u16string());
    return;
  }
  std::u16string result;
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
  PasteIfPolicyAllowed(clipboard_buffer,
                       ui::ClipboardFormatType::GetPlainTextType(),
                       std::move(data),
                       base::BindOnce(
                           [](std::u16string result, ReadTextCallback callback,
                              ClipboardPasteContentAllowed allowed) {
                             if (!allowed)
                               result.clear();
                             std::move(callback).Run(result);
                           },
                           std::move(result), std::move(callback)));
}

void ClipboardHostImpl::ReadHtml(ui::ClipboardBuffer clipboard_buffer,
                                 ReadHtmlCallback callback) {
  if (!IsRendererPasteAllowed(render_frame_routing_id_)) {
    std::move(callback).Run(std::u16string(), GURL(), 0, 0);
    return;
  }
  std::u16string markup;
  std::string src_url_str;
  uint32_t fragment_start = 0;
  uint32_t fragment_end = 0;
  auto data_dst = CreateDataEndpoint();
  clipboard_->ReadHTML(clipboard_buffer, data_dst.get(), &markup, &src_url_str,
                       &fragment_start, &fragment_end);

  std::string data = base::UTF16ToUTF8(markup);
  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::GetHtmlType(), std::move(data),
      base::BindOnce(
          [](std::u16string markup, std::string src_url_str,
             uint32_t fragment_start, uint32_t fragment_end,
             ReadHtmlCallback callback, ClipboardPasteContentAllowed allowed) {
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
  if (!IsRendererPasteAllowed(render_frame_routing_id_)) {
    std::move(callback).Run(std::u16string());
    return;
  }
  std::u16string markup;
  clipboard_->ReadSvg(clipboard_buffer, /*data_dst=*/nullptr, &markup);

  std::string data = base::UTF16ToUTF8(markup);
  PasteIfPolicyAllowed(clipboard_buffer, ui::ClipboardFormatType::GetSvgType(),
                       std::move(data),
                       base::BindOnce(
                           [](std::u16string markup, ReadSvgCallback callback,
                              ClipboardPasteContentAllowed allowed) {
                             if (!allowed)
                               markup.clear();
                             std::move(callback).Run(std::move(markup));
                           },
                           std::move(markup), std::move(callback)));
}

void ClipboardHostImpl::ReadRtf(ui::ClipboardBuffer clipboard_buffer,
                                ReadRtfCallback callback) {
  if (!IsRendererPasteAllowed(render_frame_routing_id_)) {
    std::move(callback).Run(std::string());
    return;
  }
  std::string result;
  auto data_dst = CreateDataEndpoint();
  clipboard_->ReadRTF(clipboard_buffer, data_dst.get(), &result);

  std::string data = result;
  PasteIfPolicyAllowed(clipboard_buffer, ui::ClipboardFormatType::GetRtfType(),
                       std::move(data),
                       base::BindOnce(
                           [](std::string result, ReadRtfCallback callback,
                              ClipboardPasteContentAllowed allowed) {
                             if (!allowed)
                               result.clear();
                             std::move(callback).Run(result);
                           },
                           std::move(result), std::move(callback)));
}

void ClipboardHostImpl::ReadPng(ui::ClipboardBuffer clipboard_buffer,
                                ReadPngCallback callback) {
  if (!IsRendererPasteAllowed(render_frame_routing_id_)) {
    std::move(callback).Run(mojo_base::BigBuffer());
    return;
  }
  auto data_dst = CreateDataEndpoint();
  clipboard_->ReadPng(clipboard_buffer, data_dst.get(),
                      base::BindOnce(&ClipboardHostImpl::OnReadPng,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     clipboard_buffer, std::move(callback)));
}

void ClipboardHostImpl::OnReadPng(ui::ClipboardBuffer clipboard_buffer,
                                  ReadPngCallback callback,
                                  const std::vector<uint8_t>& data) {
  std::string string_data(
      reinterpret_cast<const char*>(data.data(), data.data() + data.size()));
  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::GetPngType(),
      std::move(string_data),
      base::BindOnce(
          [](std::vector<uint8_t> data, ReadPngCallback callback,
             ClipboardPasteContentAllowed allowed) {
            if (!allowed) {
              std::move(callback).Run(mojo_base::BigBuffer());
              return;
            }
            std::move(callback).Run(mojo_base::BigBuffer(data));
          },
          std::move(data), std::move(callback)));
}
void ClipboardHostImpl::ReadImage(ui::ClipboardBuffer clipboard_buffer,
                                  ReadImageCallback callback) {
  if (!IsRendererPasteAllowed(render_frame_routing_id_)) {
    std::move(callback).Run(SkBitmap());
    return;
  }
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
  PasteIfPolicyAllowed(clipboard_buffer,
                       ui::ClipboardFormatType::GetBitmapType(),
                       std::move(data),
                       base::BindOnce(
                           [](SkBitmap bitmap, ReadImageCallback callback,
                              ClipboardPasteContentAllowed allowed) {
                             if (!allowed)
                               bitmap.reset();
                             std::move(callback).Run(bitmap);
                           },
                           std::move(bitmap), std::move(callback)));
}

void ClipboardHostImpl::ReadFiles(ui::ClipboardBuffer clipboard_buffer,
                                  ReadFilesCallback callback) {
  blink::mojom::ClipboardFilesPtr result = blink::mojom::ClipboardFiles::New();
  if (!IsRendererPasteAllowed(render_frame_routing_id_)) {
    std::move(callback).Run(std::move(result));
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kClipboardFilenames)) {
    std::move(callback).Run(std::move(result));
    return;
  }

  std::vector<ui::FileInfo> filenames;
  auto data_dst = CreateDataEndpoint();
  clipboard_->ReadFilenames(clipboard_buffer, data_dst.get(), &filenames);
  std::string data = ui::FileInfosToURIList(filenames);

  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_frame_routing_id_);
  DCHECK(render_frame_host);
  // This code matches the drag-and-drop DataTransfer code in
  // RenderWidgetHostImpl::DragTargetDrop().

  // Call PrepareDataTransferFilenamesForChildProcess() to register files so
  // they can be accessed by the renderer.
  RenderProcessHost* process = render_frame_host->GetProcess();
  result->file_system_id = PrepareDataTransferFilenamesForChildProcess(
      filenames, ChildProcessSecurityPolicyImpl::GetInstance(),
      process->GetID(), process->GetStoragePartition()->GetFileSystemContext());

  // Convert to DataTransferFiles which creates the access token for each file.
  StoragePartitionImpl* storage_partition = static_cast<StoragePartitionImpl*>(
      render_frame_host->GetProcess()->GetStoragePartition());
  std::vector<blink::mojom::DataTransferFilePtr> files =
      FileInfosToDataTransferFiles(
          filenames, storage_partition->GetFileSystemAccessManager(),
          process->GetID());
  std::move(files.begin(), files.end(), std::back_inserter(result->files));

  PerformPasteIfContentAllowed(
      clipboard_->GetSequenceNumber(clipboard_buffer),
      ui::ClipboardFormatType::GetFilenamesType(), std::move(data),
      base::BindOnce(
          [](blink::mojom::ClipboardFilesPtr result, ReadFilesCallback callback,
             ClipboardPasteContentAllowed allowed) {
            if (!allowed) {
              result->files.clear();
              result->file_system_id->clear();
            }
            std::move(callback).Run(std::move(result));
          },
          std::move(result), std::move(callback)));
}

void ClipboardHostImpl::ReadCustomData(ui::ClipboardBuffer clipboard_buffer,
                                       const std::u16string& type,
                                       ReadCustomDataCallback callback) {
  if (!IsRendererPasteAllowed(render_frame_routing_id_)) {
    std::move(callback).Run(std::u16string());
    return;
  }
  std::u16string result;
  auto data_dst = CreateDataEndpoint();
  clipboard_->ReadCustomData(clipboard_buffer, type, data_dst.get(), &result);

  std::string data = base::UTF16ToUTF8(result);
  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::GetWebCustomDataType(),
      std::move(data),
      base::BindOnce(
          [](std::u16string result, ReadCustomDataCallback callback,
             ClipboardPasteContentAllowed allowed) {
            if (!allowed)
              result.clear();
            std::move(callback).Run(result);
          },
          std::move(result), std::move(callback)));
}

void ClipboardHostImpl::WriteText(const std::u16string& text) {
  clipboard_writer_->WriteText(text);
}

void ClipboardHostImpl::WriteHtml(const std::u16string& markup,
                                  const GURL& url) {
  clipboard_writer_->WriteHTML(markup, url.spec());
}

void ClipboardHostImpl::WriteSvg(const std::u16string& markup) {
  clipboard_writer_->WriteSvg(markup);
}

void ClipboardHostImpl::WriteSmartPasteMarker() {
  clipboard_writer_->WriteWebSmartPaste();
}

void ClipboardHostImpl::WriteCustomData(
    const base::flat_map<std::u16string, std::u16string>& data) {
  base::Pickle pickle;
  ui::WriteCustomDataToPickle(data, &pickle);
  clipboard_writer_->WritePickledData(
      pickle, ui::ClipboardFormatType::GetWebCustomDataType());
}

void ClipboardHostImpl::WriteBookmark(const std::string& url,
                                      const std::u16string& title) {
  clipboard_writer_->WriteBookmark(title, url);
}

void ClipboardHostImpl::WriteImage(const SkBitmap& bitmap) {
  clipboard_writer_->WriteImage(bitmap);
}

void ClipboardHostImpl::CommitWrite() {
  clipboard_writer_ = std::make_unique<ui::ScopedClipboardWriter>(
      ui::ClipboardBuffer::kCopyPaste, CreateDataEndpoint());
}

void ClipboardHostImpl::PasteIfPolicyAllowed(
    ui::ClipboardBuffer clipboard_buffer,
    const ui::ClipboardFormatType& data_type,
    std::string data,
    IsClipboardPasteContentAllowedCallback callback) {
  if (data.empty()) {
    std::move(callback).Run(ClipboardPasteContentAllowed(true));
    return;
  }

  auto policy_cb =
      base::BindOnce(&ClipboardHostImpl::PasteIfPolicyAllowedCallback,
                     weak_ptr_factory_.GetWeakPtr(), clipboard_buffer,
                     data_type, std::move(data), std::move(callback));

  if (ui::DataTransferPolicyController::HasInstance()) {
    WebContents* web_contents = nullptr;
    RenderFrameHostImpl* render_frame_host =
        RenderFrameHostImpl::FromID(render_frame_routing_id_);
    if (render_frame_host) {
      auto* delegate = render_frame_host->delegate();
      web_contents = delegate ? delegate->GetAsWebContents() : nullptr;
    }

    ui::DataTransferPolicyController::Get()->PasteIfAllowed(
        clipboard_->GetSource(clipboard_buffer), CreateDataEndpoint().get(),
        web_contents, std::move(policy_cb));
    return;
  }
  std::move(policy_cb).Run(/*is_allowed=*/true);
}

void ClipboardHostImpl::PasteIfPolicyAllowedCallback(
    ui::ClipboardBuffer clipboard_buffer,
    const ui::ClipboardFormatType& data_type,
    std::string data,
    IsClipboardPasteContentAllowedCallback callback,
    bool is_allowed) {
  if (is_allowed) {
    PerformPasteIfContentAllowed(
        clipboard_->GetSequenceNumber(clipboard_buffer), data_type,
        std::move(data), std::move(callback));
  } else {
    // If not allowed, then don't proceed with content checks.
    std::move(callback).Run(ClipboardPasteContentAllowed(false));
  }
}

void ClipboardHostImpl::PerformPasteIfContentAllowed(
    uint64_t seqno,
    const ui::ClipboardFormatType& data_type,
    std::string data,
    IsClipboardPasteContentAllowedCallback callback) {
  CleanupObsoleteRequests();
  // Add |callback| to the callbacks associated to the sequence number, adding
  // an entry to the map if one does not exist.
  auto& request = is_allowed_requests_[seqno];
  if (request.AddCallback(std::move(callback)))
    StartIsPasteContentAllowedRequest(seqno, data_type, std::move(data));
}

void ClipboardHostImpl::StartIsPasteContentAllowedRequest(
    uint64_t seqno,
    const ui::ClipboardFormatType& data_type,
    std::string data) {
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_frame_routing_id_);
  if (!render_frame_host) {
    FinishPasteIfContentAllowed(seqno, ClipboardPasteContentAllowed(false));
    return;
  }

  render_frame_host->IsClipboardPasteContentAllowed(
      data_type, data,
      base::BindOnce(&ClipboardHostImpl::FinishPasteIfContentAllowed,
                     weak_ptr_factory_.GetWeakPtr(), seqno));
}

void ClipboardHostImpl::FinishPasteIfContentAllowed(
    uint64_t seqno,
    ClipboardPasteContentAllowed allowed) {
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

std::unique_ptr<ui::DataTransferEndpoint>
ClipboardHostImpl::CreateDataEndpoint() {
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_frame_routing_id_);
  if (render_frame_host) {
    return std::make_unique<ui::DataTransferEndpoint>(
        render_frame_host->GetLastCommittedOrigin(),
        render_frame_host->HasTransientUserActivation());
  }
  return nullptr;
}
}  // namespace content
