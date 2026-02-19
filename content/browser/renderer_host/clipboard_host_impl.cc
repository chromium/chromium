// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/clipboard_host_impl.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/optional_util.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/data_transfer_util.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/drop_data.h"
#include "crypto/hmac.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "third_party/blink/public/mojom/drag/drag.mojom-forward.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "content/public/common/url_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace content {

namespace {

}  // namespace

ClipboardHostImpl::ClipboardHostImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {
  ResetClipboardWriter();
}

void ClipboardHostImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver) {
  CHECK(render_frame_host);
  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new ClipboardHostImpl(*render_frame_host, std::move(receiver));
}

ClipboardHostImpl::~ClipboardHostImpl() {
  clipboard_writer_->Reset();
  if (listening_to_clipboard_) {
    ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
    listening_to_clipboard_ = false;
  }
}

absl::uint128 ClipboardHostImpl::GetSequenceNumberImpl(
    ui::ClipboardBuffer clipboard_buffer) {
  const ui::ClipboardSequenceNumberToken seqno =
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(clipboard_buffer);
  const base::UnguessableToken& salt =
      static_cast<StoragePartitionImpl*>(
          render_frame_host().GetProcess()->GetStoragePartition())
          ->GetPartitionUUIDPerStorageKey(render_frame_host().GetStorageKey());

  // Generate a per-partition sequence number derived from the overall
  // clipboard sequence number, using HMAC-SHA256 with the domain-string
  // "clipboard-change-id-", that is:
  //   HMAC_SHA256(seqno, "clipboard-change-id-"||uuid)
  const std::array result = crypto::hmac::SignSha256(
      seqno.value().AsBytes(), base::as_byte_span(base::StrCat(
                                   {"clipboard-change-id-", salt.ToString()})));

  const base::span result_span(result);
  return absl::MakeUint128(
      base::U64FromLittleEndian(result_span.first<8>()),
      base::U64FromLittleEndian(result_span.subspan<8, 8>()));
}

void ClipboardHostImpl::GetSequenceNumber(ui::ClipboardBuffer clipboard_buffer,
                                          GetSequenceNumberCallback callback) {
  std::move(callback).Run(GetSequenceNumberImpl(clipboard_buffer));
}

void ClipboardHostImpl::ReadAvailableTypes(
    ui::ClipboardBuffer clipboard_buffer,
    ReadAvailableTypesCallback callback) {
  auto* clipboard = ui::Clipboard::GetForCurrentThread();
  auto data_endpoint = CreateDataEndpoint();

  // If an enterprise Data Controls rule modified the clipboard, get the last
  // replaced clipboard types instead.
  if (auto policy_types =
          static_cast<RenderFrameHostImpl&>(render_frame_host())
              .GetClipboardTypesIfPolicyApplied(
                  clipboard->GetSequenceNumber(clipboard_buffer))) {
    std::move(callback).Run(std::move(*policy_types));
    return;
  }

  // ReadAvailableTypes() returns 'text/uri-list' if either files are
  // provided, or if it was set as a custom web type. If it is set because
  // files are available, do not include other types such as text/plain which
  // contain the full path on some platforms (http://crbug.com/1214108). But
  // do not exclude other types when it is set as a custom web type
  // (http://crbug.com/1241671).
  bool file_type_only = clipboard->IsFormatAvailable(
      ui::ClipboardFormatType::FilenamesType(), clipboard_buffer,
      base::OptionalToPtr(data_endpoint));

#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS FilesApp must include the custom 'fs/sources', etc data for
  // paste that it put on the clipboard during copy (crbug.com/271078230).
  if (render_frame_host().GetMainFrame()->GetLastCommittedURL().SchemeIs(
          kChromeUIScheme)) {
    file_type_only = false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (file_type_only) {
    std::move(callback).Run({ui::kMimeTypeUriList16});
    return;
  }

  clipboard->ReadAvailableTypes(
      clipboard_buffer, data_endpoint,
      base::BindOnce(&ClipboardHostImpl::OnReadAvailableTypes,
                     weak_ptr_factory_.GetWeakPtr(), clipboard_buffer,
                     std::move(callback)));
}

void ClipboardHostImpl::OnReadAvailableTypes(
    ui::ClipboardBuffer clipboard_buffer,
    ReadAvailableTypesCallback callback,
    std::vector<std::u16string> types) {
  std::move(callback).Run(std::move(types));
}

void ClipboardHostImpl::IsFormatAvailable(blink::mojom::ClipboardFormat format,
                                          ui::ClipboardBuffer clipboard_buffer,
                                          IsFormatAvailableCallback callback) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  bool result = false;
  auto data_endpoint = CreateDataEndpoint();
  switch (format) {
    case blink::mojom::ClipboardFormat::kPlaintext:
      result = clipboard->IsFormatAvailable(
          ui::ClipboardFormatType::PlainTextType(), clipboard_buffer,
          base::OptionalToPtr(data_endpoint));
#if BUILDFLAG(IS_WIN)
      result |= clipboard->IsFormatAvailable(
          ui::ClipboardFormatType::PlainTextAType(), clipboard_buffer,
          base::OptionalToPtr(data_endpoint));
#endif
      break;
    case blink::mojom::ClipboardFormat::kHtml:
      result = clipboard->IsFormatAvailable(ui::ClipboardFormatType::HtmlType(),
                                            clipboard_buffer,
                                            base::OptionalToPtr(data_endpoint));
      break;
    case blink::mojom::ClipboardFormat::kSmartPaste:
      result = clipboard->IsFormatAvailable(
          ui::ClipboardFormatType::WebKitSmartPasteType(), clipboard_buffer,
          base::OptionalToPtr(data_endpoint));
      break;
    case blink::mojom::ClipboardFormat::kBookmark:
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
      result = clipboard->IsFormatAvailable(ui::ClipboardFormatType::UrlType(),
                                            clipboard_buffer,
                                            base::OptionalToPtr(data_endpoint));
#else
      result = false;
#endif
      break;
  }
  std::move(callback).Run(result);
}

void ClipboardHostImpl::ReadText(ui::ClipboardBuffer clipboard_buffer,
                                 ReadTextCallback callback) {
  if (!IsRendererPasteAllowed(clipboard_buffer, render_frame_host())) {
    std::move(callback).Run(std::u16string());
    return;
  }

  ExtractText(clipboard_buffer, CreateDataEndpoint(),
              base::BindOnce(&ClipboardHostImpl::OnReadText,
                             weak_ptr_factory_.GetWeakPtr(), clipboard_buffer,
                             std::move(callback)));
}

void ClipboardHostImpl::OnReadText(ui::ClipboardBuffer clipboard_buffer,
                                   ReadTextCallback callback,
                                   std::u16string text) {
  ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = std::move(text);

  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::PlainTextType(),
      std::move(clipboard_paste_data),
      base::BindOnce(
          [](ReadTextCallback callback,
             std::optional<ClipboardPasteData> clipboard_paste_data) {
            std::u16string result;
            if (clipboard_paste_data) {
              result = std::move(clipboard_paste_data->text);
            }
            std::move(callback).Run(std::move(result));
          },
          std::move(callback)));
}

void ClipboardHostImpl::ReadHtml(ui::ClipboardBuffer clipboard_buffer,
                                 ReadHtmlCallback callback) {
  if (!IsRendererPasteAllowed(clipboard_buffer, render_frame_host())) {
    std::move(callback).Run(std::u16string(), GURL(), 0, 0);
    return;
  }
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  auto data_dst = CreateDataEndpoint();
  clipboard->ReadHTML(clipboard_buffer, data_dst,
                      base::BindOnce(&ClipboardHostImpl::OnReadHtml,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     clipboard_buffer, std::move(callback)));
}

void ClipboardHostImpl::OnReadHtml(ui::ClipboardBuffer clipboard_buffer,
                                   ReadHtmlCallback callback,
                                   std::u16string markup,
                                   GURL src_url,
                                   uint32_t fragment_start,
                                   uint32_t fragment_end) {
  ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.html = std::move(markup);

  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::HtmlType(),
      std::move(clipboard_paste_data),
      base::BindOnce(
          [](GURL src_url, uint32_t fragment_start, uint32_t fragment_end,
             ReadHtmlCallback callback,
             std::optional<ClipboardPasteData> clipboard_paste_data) {
            std::u16string markup;
            if (clipboard_paste_data) {
              markup = std::move(clipboard_paste_data->html);
            }
            std::move(callback).Run(std::move(markup), src_url, fragment_start,
                                    fragment_end);
          },
          std::move(src_url), fragment_start, fragment_end,
          std::move(callback)));
}

void ClipboardHostImpl::ReadSvg(ui::ClipboardBuffer clipboard_buffer,
                                ReadSvgCallback callback) {
  if (!IsRendererPasteAllowed(clipboard_buffer, render_frame_host())) {
    std::move(callback).Run(std::u16string());
    return;
  }
  auto data_dst = CreateDataEndpoint();
  ui::Clipboard::GetForCurrentThread()->ReadSvg(
      clipboard_buffer, data_dst,
      base::BindOnce(&ClipboardHostImpl::OnReadSvg,
                     weak_ptr_factory_.GetWeakPtr(), clipboard_buffer,
                     std::move(callback)));
}

void ClipboardHostImpl::OnReadSvg(ui::ClipboardBuffer clipboard_buffer,
                                  ReadSvgCallback callback,
                                  std::u16string svg) {
  ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.svg = std::move(svg);

  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::SvgType(),
      std::move(clipboard_paste_data),
      base::BindOnce(
          [](ReadSvgCallback callback,
             std::optional<ClipboardPasteData> clipboard_paste_data) {
            std::u16string svg;
            if (clipboard_paste_data) {
              svg = std::move(clipboard_paste_data->svg);
            }
            std::move(callback).Run(std::move(svg));
          },
          std::move(callback)));
}

void ClipboardHostImpl::ReadRtf(ui::ClipboardBuffer clipboard_buffer,
                                ReadRtfCallback callback) {
  if (!IsRendererPasteAllowed(clipboard_buffer, render_frame_host())) {
    std::move(callback).Run(std::string());
    return;
  }

  auto data_dst = CreateDataEndpoint();
  ui::Clipboard::GetForCurrentThread()->ReadRTF(
      clipboard_buffer, data_dst,
      base::BindOnce(&ClipboardHostImpl::OnReadRtf,
                     weak_ptr_factory_.GetWeakPtr(), clipboard_buffer,
                     std::move(callback)));
}

void ClipboardHostImpl::OnReadRtf(ui::ClipboardBuffer clipboard_buffer,
                                  ReadRtfCallback callback,
                                  std::string rtf) {
  ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.rtf = std::move(rtf);

  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::RtfType(),
      std::move(clipboard_paste_data),
      base::BindOnce(
          [](ReadRtfCallback callback,
             std::optional<ClipboardPasteData> clipboard_paste_data) {
            std::string result;
            if (clipboard_paste_data) {
              result = std::move(clipboard_paste_data->rtf);
            }
            std::move(callback).Run(std::move(result));
          },
          std::move(callback)));
}

void ClipboardHostImpl::ReadPng(ui::ClipboardBuffer clipboard_buffer,
                                ReadPngCallback callback) {
  if (!IsRendererPasteAllowed(clipboard_buffer, render_frame_host())) {
    std::move(callback).Run(mojo_base::BigBuffer());
    return;
  }
  auto data_dst = CreateDataEndpoint();
  ui::Clipboard::GetForCurrentThread()->ReadPng(
      clipboard_buffer, data_dst,
      base::BindOnce(&ClipboardHostImpl::OnReadPng,
                     weak_ptr_factory_.GetWeakPtr(), clipboard_buffer,
                     std::move(callback)));
}

void ClipboardHostImpl::OnReadPng(ui::ClipboardBuffer clipboard_buffer,
                                  ReadPngCallback callback,
                                  const std::vector<uint8_t>& data) {
  // Pass both image and associated text for content analysis.
  ExtractText(clipboard_buffer, CreateDataEndpoint(),
              base::BindOnce(&ClipboardHostImpl::OnReadPngWithText,
                             weak_ptr_factory_.GetWeakPtr(), clipboard_buffer,
                             std::move(callback), std::move(data)));
}

void ClipboardHostImpl::OnReadPngWithText(ui::ClipboardBuffer clipboard_buffer,
                                          ReadPngCallback callback,
                                          std::vector<uint8_t> data,
                                          std::u16string text) {
  // Pass both image and associated text for content analysis.
  ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = std::move(text);
  clipboard_paste_data.png = std::move(data);

  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::PngType(),
      std::move(clipboard_paste_data),
      base::BindOnce(
          [](ReadPngCallback callback,
             std::optional<ClipboardPasteData> clipboard_paste_data) {
            if (!clipboard_paste_data.has_value()) {
              std::move(callback).Run(mojo_base::BigBuffer());
              return;
            }
            std::move(callback).Run(
                mojo_base::BigBuffer(std::move(clipboard_paste_data->png)));
          },
          std::move(callback)));
}

void ClipboardHostImpl::ReadFiles(ui::ClipboardBuffer clipboard_buffer,
                                  ReadFilesCallback callback) {
  if (!IsRendererPasteAllowed(clipboard_buffer, render_frame_host())) {
    std::move(callback).Run(blink::mojom::ClipboardFiles::New());
    return;
  }

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  auto data_dst = CreateDataEndpoint();
  clipboard->ReadFilenames(
      clipboard_buffer, data_dst,
      base::BindOnce(&ClipboardHostImpl::OnReadFiles,
                     weak_ptr_factory_.GetWeakPtr(), clipboard_buffer,
                     std::move(callback)));
}

void ClipboardHostImpl::OnReadFiles(ui::ClipboardBuffer clipboard_buffer,
                                    ReadFilesCallback callback,
                                    std::vector<ui::FileInfo> filenames) {
  blink::mojom::ClipboardFilesPtr result = blink::mojom::ClipboardFiles::New();

  // Convert the vector of ui::FileInfo into a vector of base::FilePath so that
  // it can be passed to PerformPasteIfContentAllowed() for analysis.  When
  // the latter is called with ui::ClipboardFormatType::FilenamesType() the
  // data to be analyzed is expected to be a newline-separated list of full
  // paths.
  std::vector<base::FilePath> paths;
  paths.reserve(filenames.size());
  std::ranges::transform(filenames, std::back_inserter(paths),
                         [](const ui::FileInfo& info) { return info.path; });
  ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.file_paths = std::move(paths);

  // This code matches the drag-and-drop DataTransfer code in
  // RenderWidgetHostImpl::DragTargetDrop().

  // Call PrepareDataTransferFilenamesForChildProcess() to register files so
  // they can be accessed by the renderer.
  RenderProcessHost* process = render_frame_host().GetProcess();
  result->file_system_id = PrepareDataTransferFilenamesForChildProcess(
      filenames, ChildProcessSecurityPolicyImpl::GetInstance(),
      process->GetID(), process->GetStoragePartition()->GetFileSystemContext());

  // Convert to DataTransferFiles which creates the access token for each file.
  StoragePartitionImpl* storage_partition = static_cast<StoragePartitionImpl*>(
      render_frame_host().GetProcess()->GetStoragePartition());
  std::vector<blink::mojom::DataTransferFilePtr> files =
      FileInfosToDataTransferFiles(
          filenames, storage_partition->GetFileSystemAccessManager(),
          process->GetDeprecatedID());
  std::move(files.begin(), files.end(), std::back_inserter(result->files));

  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::FilenamesType(),
      std::move(clipboard_paste_data),
      base::BindOnce(
          [](blink::mojom::ClipboardFilesPtr result, ReadFilesCallback callback,
             std::optional<ClipboardPasteData> clipboard_paste_data) {
            if (!clipboard_paste_data) {
              result->files.clear();
              result->file_system_id->clear();
            } else {
              // A subset of the files can be copied.  Remove any files that
              // should be blocked.  First build a list of the files that are
              // allowed.
              std::set<base::FilePath> allowed_files(
                  std::move_iterator(clipboard_paste_data->file_paths.begin()),
                  std::move_iterator(clipboard_paste_data->file_paths.end()));

              for (auto it = result->files.begin();
                   it != result->files.end();) {
                if (allowed_files.find(it->get()->path) !=
                    allowed_files.end()) {
                  it = std::next(it);
                } else {
                  it = result->files.erase(it);
                }
              }
            }
            std::move(callback).Run(std::move(result));
          },
          std::move(result), std::move(callback)));
}

void ClipboardHostImpl::ReadDataTransferCustomData(
    ui::ClipboardBuffer clipboard_buffer,
    const std::u16string& type,
    ReadDataTransferCustomDataCallback callback) {
  if (!IsRendererPasteAllowed(clipboard_buffer, render_frame_host())) {
    std::move(callback).Run(std::u16string());
    return;
  }

  auto data_dst = CreateDataEndpoint();
  ui::Clipboard::GetForCurrentThread()->ReadDataTransferCustomData(
      clipboard_buffer, type, data_dst,
      base::BindOnce(&ClipboardHostImpl::OnReadDataTransferCustomData,
                     weak_ptr_factory_.GetWeakPtr(), clipboard_buffer, type,
                     std::move(callback)));
}

void ClipboardHostImpl::OnReadDataTransferCustomData(
    ui::ClipboardBuffer clipboard_buffer,
    const std::u16string& type,
    ReadDataTransferCustomDataCallback callback,
    std::u16string data) {
  ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.custom_data[type] = std::move(data);

  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::DataTransferCustomType(),
      std::move(clipboard_paste_data),
      base::BindOnce(
          [](ReadDataTransferCustomDataCallback callback,
             const std::u16string& type,
             std::optional<ClipboardPasteData> clipboard_paste_data) {
            std::u16string result;
            if (clipboard_paste_data) {
              result = std::move(clipboard_paste_data->custom_data[type]);
            }
            std::move(callback).Run(std::move(result));
          },
          std::move(callback), type));
}

void ClipboardHostImpl::WriteText(const std::u16string& text) {
  ClipboardPasteData data;
  data.text = text;
  ++pending_writes_;
  GetContentClient()->browser()->IsClipboardCopyAllowedByPolicy(
      CreateClipboardEndpoint(),
      {
          .size = text.size() * sizeof(std::u16string::value_type),
          .format_type = ui::ClipboardFormatType::PlainTextType(),
      },
      data,
      base::BindOnce(&ClipboardHostImpl::OnCopyAllowedResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ClipboardHostImpl::WriteHtml(const std::u16string& markup,
                                  const GURL& url) {
  ClipboardPasteData data;
  data.html = markup;
  ++pending_writes_;
  GetContentClient()->browser()->IsClipboardCopyAllowedByPolicy(
      CreateClipboardEndpoint(),
      {
          .size = markup.size() * sizeof(std::u16string::value_type),
          .format_type = ui::ClipboardFormatType::HtmlType(),
      },
      data,
      base::BindOnce(&ClipboardHostImpl::OnCopyHtmlAllowedResult,
                     weak_ptr_factory_.GetWeakPtr(), url));
}

void ClipboardHostImpl::WriteSvg(const std::u16string& markup) {
  ClipboardPasteData data;
  data.svg = markup;
  ++pending_writes_;
  GetContentClient()->browser()->IsClipboardCopyAllowedByPolicy(
      CreateClipboardEndpoint(),
      {
          .size = markup.size() * sizeof(std::u16string::value_type),
          .format_type = ui::ClipboardFormatType::SvgType(),
      },
      data,
      base::BindOnce(&ClipboardHostImpl::OnCopyAllowedResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ClipboardHostImpl::WriteSmartPasteMarker() {
  clipboard_writer_->WriteWebSmartPaste();
}

void ClipboardHostImpl::WriteDataTransferCustomData(
    const base::flat_map<std::u16string, std::u16string>& data) {
  ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.custom_data = data;

  size_t total_size = 0;
  for (const auto& entry : clipboard_paste_data.custom_data) {
    total_size += entry.second.size();
  }

  ++pending_writes_;
  GetContentClient()->browser()->IsClipboardCopyAllowedByPolicy(
      CreateClipboardEndpoint(),
      {
          .size = total_size,
          .format_type = ui::ClipboardFormatType::DataTransferCustomType(),
      },
      clipboard_paste_data,
      base::BindOnce(&ClipboardHostImpl::OnCopyAllowedResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ClipboardHostImpl::WriteBookmark(const std::string& url,
                                      const std::u16string& title) {
  clipboard_writer_->WriteBookmark(title, url);
}

void ClipboardHostImpl::WriteImage(const SkBitmap& bitmap) {
  ClipboardPasteData data;
  data.bitmap = bitmap;
  ++pending_writes_;
  GetContentClient()->browser()->IsClipboardCopyAllowedByPolicy(
      CreateClipboardEndpoint(),
      {
          .size = bitmap.computeByteSize(),
          .format_type = ui::ClipboardFormatType::BitmapType(),
      },
      std::move(data),
      base::BindOnce(&ClipboardHostImpl::OnCopyAllowedResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ClipboardHostImpl::CommitWrite() {
  if (pending_writes_ != 0) {
    // This branch indicates that some callbacks passed to
    // `IsClipboardCopyAllowedByPolicy` still haven't been called, so committing
    // data to the clipboard should be delayed.
    pending_commit_write_ = true;
    return;
  }
  pending_commit_write_ = false;

  if (render_frame_host().GetBrowserContext()->IsOffTheRecord()) {
    clipboard_writer_->MarkAsOffTheRecord();
  }
  ResetClipboardWriter();
}

bool ClipboardHostImpl::IsRendererPasteAllowed(
    ui::ClipboardBuffer clipboard_buffer,
    RenderFrameHost& render_frame_host) {
  return GetContentClient()->browser()->IsClipboardPasteAllowed(
      &render_frame_host);
}

void ClipboardHostImpl::ReadAvailableCustomAndStandardFormats(
    ReadAvailableCustomAndStandardFormatsCallback callback) {
  if (!IsRendererPasteAllowed(ui::ClipboardBuffer::kCopyPaste,
                              render_frame_host())) {
    std::move(callback).Run(std::vector<std::u16string>());
    return;
  }

  auto data_endpoint = CreateDataEndpoint();
  ui::Clipboard::GetForCurrentThread()
      ->ReadAvailableStandardAndCustomFormatNames(
          ui::ClipboardBuffer::kCopyPaste, data_endpoint,
          base::BindOnce(
              [](std::optional<ui::DataTransferEndpoint> data_endpoint,
                 ReadAvailableCustomAndStandardFormatsCallback callback,
                 std::vector<std::u16string> format_types) {
                std::move(callback).Run(std::move(format_types));
              },
              data_endpoint, std::move(callback)));
}

void ClipboardHostImpl::ReadUnsanitizedCustomFormat(
    const std::u16string& format,
    ReadUnsanitizedCustomFormatCallback callback) {
  if (!IsRendererPasteAllowed(ui::ClipboardBuffer::kCopyPaste,
                              render_frame_host())) {
    std::move(callback).Run(mojo_base::BigBuffer());
    return;
  }

  // `kMaxFormatSize` includes the null terminator as well so we check if
  // the `format` size is strictly less than `kMaxFormatSize` or not.
  if (format.length() >= blink::mojom::ClipboardHost::kMaxFormatSize) {
    std::move(callback).Run(mojo_base::BigBuffer());
    return;
  }

  // Extract the custom format names and then query the web custom format
  // corresponding to the MIME type.
  std::string format_name = base::UTF16ToASCII(format);
  auto data_endpoint = CreateDataEndpoint();
  ui::Clipboard::GetForCurrentThread()->ExtractCustomPlatformNames(
      ui::ClipboardBuffer::kCopyPaste, data_endpoint,
      base::BindOnce(&ClipboardHostImpl::OnExtractCustomPlatformNames,
                     weak_ptr_factory_.GetWeakPtr(), format_name, data_endpoint,
                     std::move(callback)));
}

void ClipboardHostImpl::OnExtractCustomPlatformNames(
    const std::string& format_name,
    std::optional<ui::DataTransferEndpoint> data_endpoint,
    ReadUnsanitizedCustomFormatCallback callback,
    std::map<std::string, std::string> custom_format_names) {
  std::string web_custom_format_string;
  auto it = custom_format_names.find(format_name);
  if (it != custom_format_names.end()) {
    web_custom_format_string = it->second;
  }
  if (web_custom_format_string.empty()) {
    std::move(callback).Run(mojo_base::BigBuffer());
    return;
  }

  ui::Clipboard::GetForCurrentThread()->ReadData(
      ui::ClipboardFormatType::CustomPlatformType(web_custom_format_string),
      data_endpoint,
      base::BindOnce(&ClipboardHostImpl::OnReadUnsanitizedCustomFormat,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ClipboardHostImpl::OnReadUnsanitizedCustomFormat(
    ReadUnsanitizedCustomFormatCallback callback,
    std::string result) {
  if (result.size() >= blink::mojom::ClipboardHost::kMaxDataSize) {
    std::move(callback).Run(mojo_base::BigBuffer());
    return;
  }
  base::span<const uint8_t> span = base::as_byte_span(result);
  mojo_base::BigBuffer buffer = mojo_base::BigBuffer(span);
  std::move(callback).Run(std::move(buffer));
}

void ClipboardHostImpl::WriteUnsanitizedCustomFormat(
    const std::u16string& format,
    mojo_base::BigBuffer data) {
  // `kMaxFormatSize` & `kMaxDataSize` includes the null terminator.
  if (format.length() >= blink::mojom::ClipboardHost::kMaxFormatSize)
    return;
  if (data.size() >= blink::mojom::ClipboardHost::kMaxDataSize)
    return;

  // The `format` is mapped to user agent defined web custom format before
  // writing to the clipboard. This happens in
  // `ScopedClipboardWriter::WriteData`.
  clipboard_writer_->WriteData(format, std::move(data));
}

void ClipboardHostImpl::PasteIfPolicyAllowed(
    ui::ClipboardBuffer clipboard_buffer,
    const ui::ClipboardFormatType& data_type,
    ClipboardPasteData clipboard_paste_data,
    IsClipboardPasteAllowedCallback callback) {
  std::optional<size_t> data_size;
  if (clipboard_paste_data.file_paths.empty()) {
    data_size = clipboard_paste_data.size();
  }

  ui::ClipboardSequenceNumberToken seqno =
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(clipboard_buffer);

  auto data_dst = CreateClipboardEndpoint();
  const ui::DataTransferEndpoint* data_dst_endpoint =
      base::OptionalToPtr(data_dst.data_transfer_endpoint());
  static_cast<RenderFrameHostImpl&>(render_frame_host())
      .IsClipboardPasteAllowedByPolicy(
          GetSourceClipboardEndpoint(data_dst_endpoint, clipboard_buffer),
          std::move(data_dst),
          {
              .size = data_size,
              .format_type = data_type,
              .seqno = seqno,
          },
          std::move(clipboard_paste_data), std::move(callback));
}

void ClipboardHostImpl::OnCopyHtmlAllowedResult(
    const GURL& source_url,
    const ui::ClipboardFormatType& data_type,
    const ClipboardPasteData& data,
    std::optional<std::u16string> replacement_data) {
  DCHECK_GT(pending_writes_, 0);
  --pending_writes_;

  AddSourceDataToClipboardWriter(*clipboard_writer_, render_frame_host());

  if (replacement_data) {
    clipboard_writer_->WriteText(std::move(*replacement_data));
  } else {
    clipboard_writer_->WriteHTML(data.html, source_url.spec());
  }
  if (pending_commit_write_) {
    CommitWrite();
  }
}

void ClipboardHostImpl::OnCopyAllowedResult(
    const ui::ClipboardFormatType& data_type,
    const ClipboardPasteData& data,
    std::optional<std::u16string> replacement_data) {
  DCHECK_GT(pending_writes_, 0);
  --pending_writes_;

  AddSourceDataToClipboardWriter(*clipboard_writer_, render_frame_host());

  // If `replacement_data` is empty, only one of these fields should be
  // non-empty depending on which "Write" method was called by the renderer.
  if (replacement_data) {
    // `replacement_data` having a value implies the copy was not allowed and
    // that a warning message should instead be put into the clipboard.
    clipboard_writer_->WriteText(std::move(*replacement_data));
  } else if (data_type == ui::ClipboardFormatType::PlainTextType()) {
    // This branch should be reached only after `WriteText()` is called.
    clipboard_writer_->WriteText(data.text);
  } else if (data_type == ui::ClipboardFormatType::SvgType()) {
    // This branch should be reached only after `WriteSvg()` is called.
    clipboard_writer_->WriteSvg(data.svg);
  } else if (data_type == ui::ClipboardFormatType::BitmapType()) {
    // This branch should be reached only after `WriteImage()` is called.
    clipboard_writer_->WriteImage(data.bitmap);
  } else if (data_type == ui::ClipboardFormatType::DataTransferCustomType()) {
    // This branch should be reached only after `WriteCustomData()` is called.
    base::Pickle pickle;
    ui::WriteCustomDataToPickle(data.custom_data, &pickle);
    clipboard_writer_->WritePickledData(
        pickle, ui::ClipboardFormatType::DataTransferCustomType());
  } else {
    NOTREACHED();
  }

  if (pending_commit_write_) {
    CommitWrite();
  }

  // Notify the observer that the write was committed to the clipboard. We do
  // this only for text copies but it can be extended to other types if needed.
  if (!replacement_data &&
      data_type == ui::ClipboardFormatType::PlainTextType()) {
    static_cast<RenderFrameHostImpl&>(render_frame_host())
        .OnTextCopiedToClipboard(data.text);
  }
}

std::optional<ui::DataTransferEndpoint>
ClipboardHostImpl::CreateDataEndpoint() {
  auto* render_frame_host_main_frame = render_frame_host().GetMainFrame();
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
          .notify_if_restricted =
              render_frame_host().HasTransientUserActivation(),
          .off_the_record =
              render_frame_host().GetBrowserContext()->IsOffTheRecord(),
      });
}

ClipboardEndpoint ClipboardHostImpl::CreateClipboardEndpoint() {
  return ClipboardEndpoint(
      CreateDataEndpoint(),
      base::BindRepeating(
          [](GlobalRenderFrameHostId rfh_id) -> BrowserContext* {
            auto* rfh = RenderFrameHost::FromID(rfh_id);
            if (!rfh) {
              return nullptr;
            }
            return rfh->GetBrowserContext();
          },
          render_frame_host().GetGlobalId()),
      render_frame_host());
}

void ClipboardHostImpl::ResetClipboardWriter() {
  auto data_endpoint = CreateDataEndpoint();
  std::unique_ptr<ui::DataTransferEndpoint> data_endpoint_ptr;
  if (data_endpoint) {
    data_endpoint_ptr =
        std::make_unique<ui::DataTransferEndpoint>(std::move(*data_endpoint));
  }
  clipboard_writer_ = std::make_unique<ui::ScopedClipboardWriter>(
      ui::ClipboardBuffer::kCopyPaste, std::move(data_endpoint_ptr));
}

void ClipboardHostImpl::OnClipboardDataChanged() {
  if (!listening_to_clipboard_ || !clipboard_listener_) {
    return;
  }

  auto change_id = GetSequenceNumberImpl(ui::ClipboardBuffer::kCopyPaste);
  if (change_id == last_change_id_) {
    // We already sent an event with this change ID.
    return;
  }

  auto data_endpoint = CreateDataEndpoint();
  ui::Clipboard::GetForCurrentThread()->ReadAvailableTypes(
      ui::ClipboardBuffer::kCopyPaste, data_endpoint,
      base::BindOnce(&ClipboardHostImpl::OnReadAvailableTypesForUpdate,
                     weak_ptr_factory_.GetWeakPtr(), change_id));
}

void ClipboardHostImpl::OnReadAvailableTypesForUpdate(
    absl::uint128 change_id,
    std::vector<std::u16string> types) {
  if (change_id != GetSequenceNumberImpl(ui::ClipboardBuffer::kCopyPaste)) {
    // Clipboard changed meanwhile. There will be another notification anyway,
    // no need to retry here.
    return;
  }

  // Static list of allowed standard MIME types
  // https://w3c.github.io/clipboard-apis/#mandatory-data-types-x
  auto filtered_types = base::STLSetIntersection<std::vector<std::u16string>>(
      base::flat_set<std::u16string>{
          ui::kMimeTypePng16,
          ui::kMimeTypeHtml16,
          ui::kMimeTypePlainText16,
      },
      base::MakeFlatSet<std::u16string>(std::move(types)));

  clipboard_listener_->OnClipboardDataChanged(
      filtered_types, last_change_id_.emplace(change_id));
}

void ClipboardHostImpl::ExtractText(
    ui::ClipboardBuffer clipboard_buffer,
    std::optional<ui::DataTransferEndpoint> data_dst,
    base::OnceCallback<void(std::u16string)> callback) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  if (clipboard->IsFormatAvailable(ui::ClipboardFormatType::PlainTextType(),
                                   clipboard_buffer,
                                   base::OptionalToPtr(data_dst))) {
    clipboard->ReadText(clipboard_buffer, data_dst, std::move(callback));
    return;
  }
#if BUILDFLAG(IS_WIN)
  if (clipboard->IsFormatAvailable(ui::ClipboardFormatType::PlainTextAType(),
                                   clipboard_buffer,
                                   base::OptionalToPtr(data_dst))) {
    clipboard->ReadAsciiText(
        clipboard_buffer, data_dst,
        base::BindOnce(
            [](base::OnceCallback<void(std::u16string)> callback,
               std::string ascii) {
              std::move(callback).Run(base::ASCIIToUTF16(ascii));
            },
            std::move(callback)));
    return;
  }
#endif
  std::move(callback).Run(std::u16string());
}

void ClipboardHostImpl::RegisterClipboardListener(
    mojo::PendingRemote<blink::mojom::ClipboardListener> listener) {
  // Replace the current listener with the new one
  clipboard_listener_.reset();
  clipboard_listener_.Bind(std::move(listener));

  // Set up connection error handler to stop observing when connection is closed
  clipboard_listener_.set_disconnect_handler(
      base::BindOnce(&ClipboardHostImpl::StopObservingClipboard,
                     weak_ptr_factory_.GetWeakPtr()));

  // Start listening for clipboard changes if not already doing so
  if (!listening_to_clipboard_) {
    ui::ClipboardMonitor::GetInstance()->AddObserver(this);
    listening_to_clipboard_ = true;
  }
}

void ClipboardHostImpl::StopObservingClipboard() {
  if (listening_to_clipboard_) {
    ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
    listening_to_clipboard_ = false;
  }
  clipboard_listener_.reset();
}

}  // namespace content
