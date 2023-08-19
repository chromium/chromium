// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/clipboard_host_impl.h"

#include <memory>
#include <set>
#include <utility>

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
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "third_party/blink/public/mojom/drag/drag.mojom-forward.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
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
struct LastWriterInfo {
  // A pointer to the last ClipboardHostImpl that committed data to the
  // clipboard.
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #global-scope
  RAW_PTR_EXCLUSION ClipboardHostImpl* writer = nullptr;

  // The sequence number of the last commit made by `writer`.
  ui::ClipboardSequenceNumberToken seqno;
};

LastWriterInfo& GetLastWriterInfo() {
  static LastWriterInfo info;
  return info;
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

// The amount of time that the result of a content allow request is cached
// and reused for the same clipboard `seqno`.
// TODO(b/294844565): Update this once multi-format pastes are handled
// correctly.
constexpr base::TimeDelta
    ClipboardHostImpl::kIsPasteContentAllowedRequestTooOld =
        base::Milliseconds(5000);

ClipboardHostImpl::IsPasteContentAllowedRequest::
    IsPasteContentAllowedRequest() = default;
ClipboardHostImpl::IsPasteContentAllowedRequest::
    ~IsPasteContentAllowedRequest() = default;

bool ClipboardHostImpl::IsPasteContentAllowedRequest::AddCallback(
    IsClipboardPasteContentAllowedCallback callback) {
  // If this request has already completed, invoke the callback immediately
  // and return.
  if (data_.has_value()) {
    std::move(callback).Run(data_.value());
    return false;
  }

  callbacks_.push_back(std::move(callback));

  // If this is the first callback registered tell the caller to start the scan.
  return callbacks_.size() == 1;
}

void ClipboardHostImpl::IsPasteContentAllowedRequest::Complete(
    IsClipboardPasteContentAllowedCallbackArgType data) {
  completed_time_ = base::Time::Now();
  data_ = std::move(data);
  InvokeCallbacks();
}

bool ClipboardHostImpl::IsPasteContentAllowedRequest::IsObsolete(
    base::Time now) {
  return (now - completed_time_) > kIsPasteContentAllowedRequestTooOld;
}

base::Time ClipboardHostImpl::IsPasteContentAllowedRequest::completed_time() {
  DCHECK(is_complete());
  return completed_time_;
}

void ClipboardHostImpl::IsPasteContentAllowedRequest::InvokeCallbacks() {
  DCHECK(data_);

  auto callbacks = std::move(callbacks_);
  for (auto& callback : callbacks) {
    if (!callback.is_null())
      std::move(callback).Run(data_.value());
  }
}

ClipboardHostImpl::ClipboardHostImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {
  clipboard_writer_ = std::make_unique<ui::ScopedClipboardWriter>(
      ui::ClipboardBuffer::kCopyPaste,
      render_frame_host.GetBrowserContext()->IsOffTheRecord()
          ? nullptr
          : std::make_unique<ui::DataTransferEndpoint>(
                render_frame_host.GetMainFrame()->GetLastCommittedURL()));
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
  GetLastWriterInfo() = {};
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

  std::u16string text = ExtractText(clipboard_buffer, CreateDataEndpoint());
  ClipboardPasteData clipboard_paste_data =
      ClipboardPasteData(base::UTF16ToUTF8(text), std::string(), {});
  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::PlainTextType(),
      std::move(clipboard_paste_data),
      base::BindOnce(
          [](std::u16string result, ReadTextCallback callback,
             absl::optional<ClipboardPasteData> clipboard_paste_data) {
            if (!clipboard_paste_data) {
              result.clear();
            }
            std::move(callback).Run(result);
          },
          std::move(text), std::move(callback)));
}

void ClipboardHostImpl::ReadHtml(ui::ClipboardBuffer clipboard_buffer,
                                 ReadHtmlCallback callback) {
  if (!IsRendererPasteAllowed(clipboard_buffer, render_frame_host())) {
    std::move(callback).Run(std::u16string(), GURL(), 0, 0);
    return;
  }
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string markup;
  std::string src_url_str;
  uint32_t fragment_start = 0;
  uint32_t fragment_end = 0;
  auto data_dst = CreateDataEndpoint();
  clipboard->ReadHTML(clipboard_buffer, data_dst.get(), &markup, &src_url_str,
                      &fragment_start, &fragment_end);

  ClipboardPasteData clipboard_paste_data =
      ClipboardPasteData(base::UTF16ToUTF8(markup), std::string(), {});
  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::HtmlType(),
      std::move(clipboard_paste_data),
      base::BindOnce(
          [](std::u16string markup, std::string src_url_str,
             uint32_t fragment_start, uint32_t fragment_end,
             ReadHtmlCallback callback,
             absl::optional<ClipboardPasteData> clipboard_paste_data) {
            if (!clipboard_paste_data) {
              markup.clear();
            }
            std::move(callback).Run(std::move(markup), GURL(src_url_str),
                                    fragment_start, fragment_end);
          },
          std::move(markup), std::move(src_url_str), fragment_start,
          fragment_end, std::move(callback)));
}

void ClipboardHostImpl::ReadSvg(ui::ClipboardBuffer clipboard_buffer,
                                ReadSvgCallback callback) {
  if (!IsRendererPasteAllowed(clipboard_buffer, render_frame_host())) {
    std::move(callback).Run(std::u16string());
    return;
  }
  std::u16string markup;
  ui::Clipboard::GetForCurrentThread()->ReadSvg(clipboard_buffer,
                                                /*data_dst=*/nullptr, &markup);

  ClipboardPasteData clipboard_paste_data =
      ClipboardPasteData(base::UTF16ToUTF8(markup), std::string(), {});
  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::SvgType(),
      std::move(clipboard_paste_data),
      base::BindOnce(
          [](std::u16string markup, ReadSvgCallback callback,
             absl::optional<ClipboardPasteData> clipboard_paste_data) {
            if (!clipboard_paste_data) {
              markup.clear();
            }
            std::move(callback).Run(std::move(markup));
          },
          std::move(markup), std::move(callback)));
}

void ClipboardHostImpl::ReadRtf(ui::ClipboardBuffer clipboard_buffer,
                                ReadRtfCallback callback) {
  if (!IsRendererPasteAllowed(clipboard_buffer, render_frame_host())) {
    std::move(callback).Run(std::string());
    return;
  }
  std::string result;
  auto data_dst = CreateDataEndpoint();
  ui::Clipboard::GetForCurrentThread()->ReadRTF(clipboard_buffer,
                                                data_dst.get(), &result);

  ClipboardPasteData clipboard_paste_data =
      ClipboardPasteData(result, std::string(), {});
  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::RtfType(),
      std::move(clipboard_paste_data),
      base::BindOnce(
          [](std::string result, ReadRtfCallback callback,
             absl::optional<ClipboardPasteData> clipboard_paste_data) {
            if (!clipboard_paste_data) {
              result.clear();
            }
            std::move(callback).Run(result);
          },
          std::move(result), std::move(callback)));
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
  ClipboardPasteData clipboard_paste_data = ClipboardPasteData(
      base::UTF16ToUTF8(ExtractText(clipboard_buffer, CreateDataEndpoint())),
      std::string(data.begin(), data.end()), {});
  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::PngType(),
      std::move(clipboard_paste_data),
      base::BindOnce(
          [](std::vector<uint8_t> data, ReadPngCallback callback,
             absl::optional<ClipboardPasteData> clipboard_paste_data) {
            if (!clipboard_paste_data.has_value()) {
              std::move(callback).Run(mojo_base::BigBuffer());
              return;
            }
            std::move(callback).Run(mojo_base::BigBuffer(data));
          },
          std::move(data), std::move(callback)));
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

  // Convert the vector of ui::FileInfo into a vector of std::string so that
  // it can be passed to PerformPasteIfContentAllowed() for analysis.  When
  // the latter is called with ui::ClipboardFormatType::FilenamesType() the
  // data to be analyzed is expected to be a newline-separated list of full
  // paths.
  std::vector<std::string> paths;
  paths.reserve(filenames.size());
  base::ranges::transform(
      filenames, std::back_inserter(paths),
      [](const ui::FileInfo& info) { return info.path.AsUTF8Unsafe(); });
  ClipboardPasteData clipboard_paste_data =
      ClipboardPasteData(std::string(), std::string(), std::move(paths));

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

  PerformPasteIfContentAllowed(
      clipboard->GetSequenceNumber(clipboard_buffer),
      ui::ClipboardFormatType::FilenamesType(), std::move(clipboard_paste_data),
      base::BindOnce(
          [](blink::mojom::ClipboardFilesPtr result, ReadFilesCallback callback,
             absl::optional<ClipboardPasteData> clipboard_paste_data) {
            if (!clipboard_paste_data) {
              result->files.clear();
              result->file_system_id->clear();
            } else {
              // A subset of the files can be copied.  Remove any files that
              // should be blocked.  First build a list of the files that are
              // allowed.
              std::vector<std::string> allowed_files_vector =
                  std::move(clipboard_paste_data->file_paths);
              std::set<std::string> allowed_files(
                  std::move_iterator(allowed_files_vector.begin()),
                  std::move_iterator(allowed_files_vector.end()));

              for (auto it = result->files.begin();
                   it != result->files.end();) {
                if (allowed_files.find(it->get()->path.AsUTF8Unsafe()) !=
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

void ClipboardHostImpl::ReadCustomData(ui::ClipboardBuffer clipboard_buffer,
                                       const std::u16string& type,
                                       ReadCustomDataCallback callback) {
  if (!IsRendererPasteAllowed(clipboard_buffer, render_frame_host())) {
    std::move(callback).Run(std::u16string());
    return;
  }
  std::u16string result;
  auto data_dst = CreateDataEndpoint();
  ui::Clipboard::GetForCurrentThread()->ReadCustomData(clipboard_buffer, type,
                                                       data_dst.get(), &result);

  ClipboardPasteData clipboard_paste_data =
      ClipboardPasteData(base::UTF16ToUTF8(result), std::string(), {});
  PasteIfPolicyAllowed(
      clipboard_buffer, ui::ClipboardFormatType::WebCustomDataType(),
      std::move(clipboard_paste_data),
      base::BindOnce(
          [](std::u16string result, ReadCustomDataCallback callback,
             absl::optional<ClipboardPasteData> clipboard_paste_data) {
            if (!clipboard_paste_data) {
              result.clear();
            }
            std::move(callback).Run(result);
          },
          std::move(result), std::move(callback)));
}

void ClipboardHostImpl::WriteText(const std::u16string& text) {
  CopyIfAllowed(
      text.size() * sizeof(std::u16string::value_type),
      base::BindOnce(&ui::ScopedClipboardWriter::WriteText,
                     base::Unretained(clipboard_writer_.get()), text));
}

void ClipboardHostImpl::WriteHtml(const std::u16string& markup,
                                  const GURL& url) {
  CopyIfAllowed(
      markup.size() * sizeof(std::u16string::value_type),
      base::BindOnce(&ui::ScopedClipboardWriter::WriteHTML,
                     base::Unretained(clipboard_writer_.get()), markup,
                     url.spec(), ui::ClipboardContentType::kSanitized));
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
      pickle, ui::ClipboardFormatType::WebCustomDataType());
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

  // Remember the ClipboardHostImpl and associated seqno of the last write
  // made to the clipboard by any ClipboardHostImpl.
  GetLastWriterInfo() = {
      .writer = this,
      .seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste)};
}

bool ClipboardHostImpl::IsRendererPasteAllowed(
    ui::ClipboardBuffer clipboard_buffer,
    RenderFrameHost& render_frame_host) {
  auto it = is_allowed_requests_.find(
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          clipboard_buffer));
  if (it != is_allowed_requests_.end() && it->second.is_complete() &&
      !it->second.IsObsolete(base::Time::Now())) {
    return true;
  }

  return GetContentClient()->browser()->IsClipboardPasteAllowed(
      &render_frame_host);
}

bool ClipboardHostImpl::IsUnsanitizedCustomFormatContentAllowed() {
  if (!base::FeatureList::IsEnabled(blink::features::kClipboardCustomFormats)) {
    mojo::ReportBadMessage("Custom format read/write is not enabled.");
    return false;
  }
  return true;
}

void ClipboardHostImpl::ReadAvailableCustomAndStandardFormats(
    ReadAvailableCustomAndStandardFormatsCallback callback) {
  if (!IsUnsanitizedCustomFormatContentAllowed())
    return;
  std::vector<std::u16string> format_types =
      ui::Clipboard::GetForCurrentThread()
          ->ReadAvailableStandardAndCustomFormatNames(
              ui::ClipboardBuffer::kCopyPaste, CreateDataEndpoint().get());
  std::move(callback).Run(format_types);
}

void ClipboardHostImpl::ReadUnsanitizedCustomFormat(
    const std::u16string& format,
    ReadUnsanitizedCustomFormatCallback callback) {
  if (!IsUnsanitizedCustomFormatContentAllowed())
    return;
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
  if (!IsUnsanitizedCustomFormatContentAllowed())
    return;
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
    IsClipboardPasteContentAllowedCallback callback) {
  if (clipboard_paste_data.isEmpty()) {
    std::move(callback).Run(std::move(clipboard_paste_data));
    return;
  }

  size_t data_size =
      clipboard_paste_data.text.size() + clipboard_paste_data.image.size();
  // When the paste data includes files, update data_size to be int_max to
  // enforce data transfer scanning.
  if (!clipboard_paste_data.file_paths.empty()) {
    data_size = INT_MAX;
  }
  auto policy_cb = base::BindOnce(
      &ClipboardHostImpl::PasteIfPolicyAllowedCallback,
      weak_ptr_factory_.GetWeakPtr(), clipboard_buffer, data_type,
      std::move(clipboard_paste_data), std::move(callback));

  if (ui::DataTransferPolicyController::HasInstance()) {
    ui::DataTransferPolicyController::Get()->PasteIfAllowed(
        ui::Clipboard::GetForCurrentThread()->GetSource(clipboard_buffer),
        CreateDataEndpoint().get(), data_size, &render_frame_host(),
        std::move(policy_cb));
    return;
  }
  std::move(policy_cb).Run(/*is_allowed=*/true);
}

void ClipboardHostImpl::PasteIfPolicyAllowedCallback(
    ui::ClipboardBuffer clipboard_buffer,
    const ui::ClipboardFormatType& data_type,
    ClipboardPasteData clipboard_paste_data,
    IsClipboardPasteContentAllowedCallback callback,
    bool is_allowed) {
  if (is_allowed) {
    PerformPasteIfContentAllowed(
        ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
            clipboard_buffer),
        data_type, std::move(clipboard_paste_data), std::move(callback));
  } else {
    // If not allowed, then don't proceed with content checks.
    std::move(callback).Run(absl::nullopt);
  }
}

void ClipboardHostImpl::PerformPasteIfContentAllowed(
    const ui::ClipboardSequenceNumberToken& seqno,
    const ui::ClipboardFormatType& data_type,
    ClipboardPasteData clipboard_paste_data,
    IsClipboardPasteContentAllowedCallback callback) {
  CleanupObsoleteRequests();

  // Always allow if the source of the last clipboard commit was this host.
  const LastWriterInfo& info = GetLastWriterInfo();
  if (info.writer == this && info.seqno == seqno) {
    std::move(callback).Run(std::move(clipboard_paste_data));
    return;
  }

  // Add |callback| to the callbacks associated to the sequence number, adding
  // an entry to the map if one does not exist.
  auto& request = is_allowed_requests_[seqno];
  if (request.AddCallback(std::move(callback))) {
    StartIsPasteContentAllowedRequest(seqno, data_type,
                                      std::move(clipboard_paste_data));
  }
}

void ClipboardHostImpl::StartIsPasteContentAllowedRequest(
    const ui::ClipboardSequenceNumberToken& seqno,
    const ui::ClipboardFormatType& data_type,
    ClipboardPasteData clipboard_paste_data) {
  static_cast<RenderFrameHostImpl&>(render_frame_host())
      .IsClipboardPasteContentAllowed(
          data_type, std::move(clipboard_paste_data),
          base::BindOnce(&ClipboardHostImpl::FinishPasteIfContentAllowed,
                         weak_ptr_factory_.GetWeakPtr(), seqno));
}

void ClipboardHostImpl::FinishPasteIfContentAllowed(
    const ui::ClipboardSequenceNumberToken& seqno,
    absl::optional<ClipboardPasteData> clipboard_paste_data) {
  if (is_allowed_requests_.count(seqno) == 0)
    return;

  auto& request = is_allowed_requests_[seqno];
  request.Complete(std::move(clipboard_paste_data));
}

void ClipboardHostImpl::CopyIfAllowed(size_t data_size_in_bytes,
                                      CopyAllowedCallback callback) {
  std::u16string replacement_data;
  if (GetContentClient()->browser()->IsClipboardCopyAllowed(
          render_frame_host().GetBrowserContext(),
          render_frame_host().GetLastCommittedURL(), data_size_in_bytes,
          replacement_data)) {
    // Set the source of the clipboard text/html
    clipboard_writer_->SetDataSourceURL(
        render_frame_host().GetMainFrame()->GetLastCommittedURL(),
        render_frame_host().GetLastCommittedURL());
    std::move(callback).Run();
  } else {
    clipboard_writer_->WriteText(replacement_data);
  }
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
  if (render_frame_host().GetBrowserContext()->IsOffTheRecord()) {
    return nullptr;
  }
  return std::make_unique<ui::DataTransferEndpoint>(
      render_frame_host().GetMainFrame()->GetLastCommittedURL(),
      render_frame_host().HasTransientUserActivation());
}
}  // namespace content
