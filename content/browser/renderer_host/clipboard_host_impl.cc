// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/clipboard_host_impl.h"

#include <memory>
#include <set>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/data_transfer_util.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/drop_data.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "content/public/common/url_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace content {

namespace {

// Used to skip content analysis checks when the source and destination of the
// clipboard data are the same.
struct LastClipboardWriterInfo {
  // RFH ID of the last RFH to have committed data to the clipboard.
  GlobalRenderFrameHostId rfh_id;

  // The sequence number of the last commit made to the clipboard by the
  // browser.
  ui::ClipboardSequenceNumberToken seqno;
};

LastClipboardWriterInfo& LastClipboardWriterInfoStorage() {
  static LastClipboardWriterInfo info;
  return info;
}

// Helper observer used to keep track of the last copy operation made from
// the browser. This is used to ensure that `ui::Clipboard::GetSequenceNumber`
// is always used with a updated value, which is not always synchronous
// depending on platform.
class CopyObserver : public ui::ClipboardObserver {
 public:
  CopyObserver() { ui::ClipboardMonitor::GetInstance()->AddObserver(this); }

 private:
  void OnClipboardDataChanged() override {
    LastClipboardWriterInfoStorage().seqno =
        ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
            ui::ClipboardBuffer::kCopyPaste);
  }
};

void ClearIfLastClipboardWriterIs(const RenderFrameHost& rfh) {
  if (LastClipboardWriterInfoStorage().rfh_id == rfh.GetGlobalId()) {
    LastClipboardWriterInfoStorage() = {};
  }
}

std::u16string ExtractText(ui::ClipboardBuffer clipboard_buffer,
                           std::unique_ptr<ui::DataTransferEndpoint> data_dst) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string result;
  if (clipboard->IsFormatAvailable(ui::ClipboardFormatType::PlainTextType(),
                                   clipboard_buffer, data_dst.get())) {
    clipboard->ReadText(clipboard_buffer, data_dst.get(), &result);
  } else {
#if BUILDFLAG(IS_WIN)
    if (clipboard->IsFormatAvailable(ui::ClipboardFormatType::PlainTextAType(),
                                     clipboard_buffer, data_dst.get())) {
      std::string ascii;
      clipboard->ReadAsciiText(clipboard_buffer, data_dst.get(), &ascii);
      result = base::ASCIIToUTF16(ascii);
    }
#endif
  }
  return result;
}

}  // namespace

bool IsLastClipboardWrite(const RenderFrameHost& rfh,
                          ui::ClipboardSequenceNumberToken seqno) {
  const auto& info = LastClipboardWriterInfoStorage();
  return info.rfh_id == rfh.GetGlobalId() && info.seqno == seqno;
}

void SetLastClipboardWrite(const RenderFrameHost& rfh,
                           ui::ClipboardSequenceNumberToken seqno) {
  LastClipboardWriterInfoStorage() = {
      .rfh_id = rfh.GetGlobalId(),
      .seqno = seqno,
  };
}

ClipboardEndpoint GetSourceClipboardEndpoint(
    ui::ClipboardSequenceNumberToken seqno,
    ui::ClipboardBuffer clipboard_buffer) {
  const auto& info = LastClipboardWriterInfoStorage();
  auto* rfh = RenderFrameHost::FromID(info.rfh_id);

  if (info.seqno != seqno || !rfh) {
    // Fall back to the clipboard source if there is no `seqno` match or RFH, as
    // `ui::DataTransferEndpoint` can be populated differently based on
    // platform.
    return ClipboardEndpoint(
        ui::Clipboard::GetForCurrentThread()->GetSource(clipboard_buffer));
  }

  std::optional<ui::DataTransferEndpoint> source_dte;
  auto clipboard_source_dte =
      ui::Clipboard::GetForCurrentThread()->GetSource(clipboard_buffer);
  if (clipboard_source_dte) {
    if (clipboard_source_dte->IsUrlType()) {
      source_dte = std::make_optional<ui::DataTransferEndpoint>(
          *clipboard_source_dte->GetURL(),
          ui::DataTransferEndpointOptions{
              .off_the_record = rfh->GetBrowserContext()->IsOffTheRecord()});
    } else {
      source_dte = std::move(clipboard_source_dte);
    }
  }

  return ClipboardEndpoint(
      std::move(source_dte),
      base::BindRepeating(
          [](GlobalRenderFrameHostId rfh_id) -> BrowserContext* {
            auto* rfh = RenderFrameHost::FromID(rfh_id);
            if (!rfh) {
              return nullptr;
            }
            return rfh->GetBrowserContext();
          },
          info.rfh_id),
      *rfh);
}

ClipboardHostImpl::ClipboardHostImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {
  static base::NoDestructor<CopyObserver> observer;
  clipboard_writer_ = std::make_unique<ui::ScopedClipboardWriter>(
      ui::ClipboardBuffer::kCopyPaste, CreateDataEndpoint());
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
  ClearIfLastClipboardWriterIs(render_frame_host());
  clipboard_writer_->Reset();
}

void ClipboardHostImpl::GetSequenceNumber(ui::ClipboardBuffer clipboard_buffer,
                                          GetSequenceNumberCallback callback) {
  std::move(callback).Run(
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          clipboard_buffer));
}

void ClipboardHostImpl::ReadAvailableTypes(
    ui::ClipboardBuffer clipboard_buffer,
    ReadAvailableTypesCallback callback) {
  std::vector<std::u16string> types;
  auto* clipboard = ui::Clipboard::GetForCurrentThread();
  auto data_endpoint = CreateDataEndpoint();

  // ReadAvailableTypes() returns 'text/uri-list' if either files are provided,
  // or if it was set as a custom web type. If it is set because files are
  // available, do not include other types such as text/plain which contain the
  // full path on some platforms (http://crbug.com/1214108). But do not exclude
  // other types when it is set as a custom web type (http://crbug.com/1241671).
  bool file_type_only =
      clipboard->IsFormatAvailable(ui::ClipboardFormatType::FilenamesType(),
                                   clipboard_buffer, data_endpoint.get());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS FilesApp must include the custom 'fs/sources', etc data for
  // paste that it put on the clipboard during copy (b/271078230). This can be
  // removed when ash is fully replaced by lacros.
  if (render_frame_host().GetMainFrame()->GetLastCommittedURL().SchemeIs(
          kChromeUIScheme)) {
    file_type_only = false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (file_type_only) {
    types = {base::UTF8ToUTF16(ui::kMimeTypeURIList)};
  } else {
    clipboard->ReadAvailableTypes(clipboard_buffer, data_endpoint.get(),
                                  &types);
  }
  std::move(callback).Run(types);
}

void ClipboardHostImpl::IsFormatAvailable(blink::mojom::ClipboardFormat format,
                                          ui::ClipboardBuffer clipboard_buffer,
                                          IsFormatAvailableCallback callback) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  bool result = false;
  auto data_endpoint = CreateDataEndpoint();
  switch (format) {
    case blink::mojom::ClipboardFormat::kPlaintext:
      result =
          clipboard->IsFormatAvailable(ui::ClipboardFormatType::PlainTextType(),
                                       clipboard_buffer, data_endpoint.get());
#if BUILDFLAG(IS_WIN)
      result |= clipboard->IsFormatAvailable(
          ui::ClipboardFormatType::PlainTextAType(), clipboard_buffer,
          data_endpoint.get());
#endif
      break;
    case blink::mojom::ClipboardFormat::kHtml:
      result =
          clipboard->IsFormatAvailable(ui::ClipboardFormatType::HtmlType(),
                                       clipboard_buffer, data_endpoint.get());
      break;
    case blink::mojom::ClipboardFormat::kSmartPaste:
      result = clipboard->IsFormatAvailable(
          ui::ClipboardFormatType::WebKitSmartPasteType(), clipboard_buffer,
          data_endpoint.get());
      break;
    case blink::mojom::ClipboardFormat::kBookmark:
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
      result =
          clipboard->IsFormatAvailable(ui::ClipboardFormatType::UrlType(),
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
  if (!IsRendererPasteAllowed(clipboard_buffer, render_frame_host())) {
    std::move(callback).Run(std::u16string());
    return;
  }

  ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text =
      ExtractText(clipboard_buffer, CreateDataEndpoint());

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
  ClipboardPasteData clipboard_paste_data;
  std::string src_url_str;
  uint32_t fragment_start = 0;
  uint32_t fragment_end = 0;
  auto data_dst = CreateDataEndpoint();
  clipboard->ReadHTML(clipboard_buffer, data_dst.get(),
                      &clipboard_paste_data.html, &src_url_str, &fragment_start,
                      &fragment_end);

  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::HtmlType(),
      std::move(clipboard_paste_data),
      base::BindOnce(
          [](std::string src_url_str, uint32_t fragment_start,
             uint32_t fragment_end, ReadHtmlCallback callback,
             std::optional<ClipboardPasteData> clipboard_paste_data) {
            std::u16string markup;
            if (clipboard_paste_data) {
              markup = std::move(clipboard_paste_data->html);
            }
            std::move(callback).Run(std::move(markup), GURL(src_url_str),
                                    fragment_start, fragment_end);
          },
          std::move(src_url_str), fragment_start, fragment_end,
          std::move(callback)));
}

void ClipboardHostImpl::ReadSvg(ui::ClipboardBuffer clipboard_buffer,
                                ReadSvgCallback callback) {
  if (!IsRendererPasteAllowed(clipboard_buffer, render_frame_host())) {
    std::move(callback).Run(std::u16string());
    return;
  }
  ClipboardPasteData clipboard_paste_data;
  ui::Clipboard::GetForCurrentThread()->ReadSvg(clipboard_buffer,
                                                /*data_dst=*/nullptr,
                                                &clipboard_paste_data.svg);

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

  ClipboardPasteData clipboard_paste_data;
  auto data_dst = CreateDataEndpoint();
  ui::Clipboard::GetForCurrentThread()->ReadRTF(
      clipboard_buffer, data_dst.get(), &clipboard_paste_data.rtf);

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
      clipboard_buffer, data_dst.get(),
      base::BindOnce(&ClipboardHostImpl::OnReadPng,
                     weak_ptr_factory_.GetWeakPtr(), clipboard_buffer,
                     std::move(callback)));
}

void ClipboardHostImpl::OnReadPng(ui::ClipboardBuffer clipboard_buffer,
                                  ReadPngCallback callback,
                                  const std::vector<uint8_t>& data) {
  // Pass both image and associated text for content analysis.
  ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text =
      ExtractText(clipboard_buffer, CreateDataEndpoint());
  clipboard_paste_data.png = data;

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
  blink::mojom::ClipboardFilesPtr result = blink::mojom::ClipboardFiles::New();
  if (!IsRendererPasteAllowed(clipboard_buffer, render_frame_host())) {
    std::move(callback).Run(std::move(result));
    return;
  }

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::vector<ui::FileInfo> filenames;
  auto data_dst = CreateDataEndpoint();
  clipboard->ReadFilenames(clipboard_buffer, data_dst.get(), &filenames);

  // Convert the vector of ui::FileInfo into a vector of base::FilePath so that
  // it can be passed to PerformPasteIfContentAllowed() for analysis.  When
  // the latter is called with ui::ClipboardFormatType::FilenamesType() the
  // data to be analyzed is expected to be a newline-separated list of full
  // paths.
  std::vector<base::FilePath> paths;
  paths.reserve(filenames.size());
  base::ranges::transform(filenames, std::back_inserter(paths),
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
          process->GetID());
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

  ClipboardPasteData clipboard_paste_data;
  auto data_dst = CreateDataEndpoint();
  ui::Clipboard::GetForCurrentThread()->ReadDataTransferCustomData(
      clipboard_buffer, type, data_dst.get(),
      &clipboard_paste_data.custom_data[type]);

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
  clipboard_writer_ = std::make_unique<ui::ScopedClipboardWriter>(
      ui::ClipboardBuffer::kCopyPaste, CreateDataEndpoint());

  LastClipboardWriterInfoStorage().rfh_id = render_frame_host().GetGlobalId();
}

bool ClipboardHostImpl::IsRendererPasteAllowed(
    ui::ClipboardBuffer clipboard_buffer,
    RenderFrameHost& render_frame_host) {
  return GetContentClient()->browser()->IsClipboardPasteAllowed(
      &render_frame_host);
}

void ClipboardHostImpl::ReadAvailableCustomAndStandardFormats(
    ReadAvailableCustomAndStandardFormatsCallback callback) {
  std::vector<std::u16string> format_types =
      ui::Clipboard::GetForCurrentThread()
          ->ReadAvailableStandardAndCustomFormatNames(
              ui::ClipboardBuffer::kCopyPaste, CreateDataEndpoint().get());
  std::move(callback).Run(format_types);
}

void ClipboardHostImpl::ReadUnsanitizedCustomFormat(
    const std::u16string& format,
    ReadUnsanitizedCustomFormatCallback callback) {
  // `kMaxFormatSize` includes the null terminator as well so we check if
  // the `format` size is strictly less than `kMaxFormatSize` or not.
  if (format.length() >= blink::mojom::ClipboardHost::kMaxFormatSize)
    return;

  // Extract the custom format names and then query the web custom format
  // corresponding to the MIME type.
  std::string format_name = base::UTF16ToASCII(format);
  auto data_endpoint = CreateDataEndpoint();
  std::map<std::string, std::string> custom_format_names =
      ui::Clipboard::GetForCurrentThread()->ExtractCustomPlatformNames(
          ui::ClipboardBuffer::kCopyPaste, data_endpoint.get());
  std::string web_custom_format_string;
  if (custom_format_names.find(format_name) != custom_format_names.end())
    web_custom_format_string = custom_format_names[format_name];
  if (web_custom_format_string.empty())
    return;

  std::string result;
  ui::Clipboard::GetForCurrentThread()->ReadData(
      ui::ClipboardFormatType::GetType(web_custom_format_string),
      data_endpoint.get(), &result);
  if (result.size() >= blink::mojom::ClipboardHost::kMaxDataSize)
    return;
  base::span<const uint8_t> span = base::as_bytes(base::make_span(result));
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

  static_cast<RenderFrameHostImpl&>(render_frame_host())
      .IsClipboardPasteAllowedByPolicy(
          GetSourceClipboardEndpoint(seqno, clipboard_buffer),
          CreateClipboardEndpoint(),
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

  clipboard_writer_->SetDataSourceURL(
      render_frame_host().GetMainFrame()->GetLastCommittedURL(),
      render_frame_host().GetLastCommittedURL());

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

  clipboard_writer_->SetDataSourceURL(
      render_frame_host().GetMainFrame()->GetLastCommittedURL(),
      render_frame_host().GetLastCommittedURL());

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
    NOTREACHED_IN_MIGRATION();
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

std::unique_ptr<ui::DataTransferEndpoint>
ClipboardHostImpl::CreateDataEndpoint() {
  if (!render_frame_host().GetMainFrame()->GetLastCommittedURL().is_valid()) {
    return nullptr;
  }

  return std::make_unique<ui::DataTransferEndpoint>(
      render_frame_host().GetMainFrame()->GetLastCommittedURL(),
      ui::DataTransferEndpointOptions{
          .notify_if_restricted =
              render_frame_host().HasTransientUserActivation(),
          .off_the_record =
              render_frame_host().GetBrowserContext()->IsOffTheRecord(),
      });
}

ClipboardEndpoint ClipboardHostImpl::CreateClipboardEndpoint() {
  return ClipboardEndpoint(
      CreateDataEndpoint().get(),
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

}  // namespace content
