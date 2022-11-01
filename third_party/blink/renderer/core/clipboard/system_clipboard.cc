// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_utilities.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

namespace {

String NonNullString(const String& string) {
  return string.IsNull() ? g_empty_string16_bit : string;
}

// This enum is used in UMA. Do not delete or re-order entries. New entries
// should only be added at the end. Please keep in sync with
// "ClipboardPastedImageUrls" in //tools/metrics/histograms/enums.xml.
enum class ClipboardPastedImageUrls {
  kUnknown = 0,
  kLocalFileUrls = 1,
  kHttpUrls = 2,
  kCidUrls = 3,
  kOtherUrls = 4,
  kBase64EncodedImage = 5,
  kLocalFileUrlWithRtf = 6,
  kImageLoadError = 7,
  kMaxValue = kImageLoadError,
};

}  // namespace

SystemClipboard::SystemClipboard(LocalFrame* frame)
    : clipboard_(frame->DomWindow()) {
  frame->GetBrowserInterfaceBroker().GetInterface(
      clipboard_.BindNewPipeAndPassReceiver(
          frame->GetTaskRunner(TaskType::kUserInteraction)));
#if BUILDFLAG(IS_OZONE)
  is_selection_buffer_available_ =
      frame->GetSettings()->GetSelectionClipboardBufferAvailable();
#endif  // BUILDFLAG(IS_OZONE)
}

bool SystemClipboard::IsSelectionMode() const {
  return buffer_ == mojom::ClipboardBuffer::kSelection;
}

void SystemClipboard::SetSelectionMode(bool selection_mode) {
  buffer_ = selection_mode ? mojom::ClipboardBuffer::kSelection
                           : mojom::ClipboardBuffer::kStandard;
}

bool SystemClipboard::IsFormatAvailable(blink::mojom::ClipboardFormat format) {
  if (!IsValidBufferType(buffer_) || !clipboard_.is_bound())
    return false;
  bool result = false;
  clipboard_->IsFormatAvailable(format, buffer_, &result);
  return result;
}

ClipboardSequenceNumberToken SystemClipboard::SequenceNumber() {
  if (!IsValidBufferType(buffer_) || !clipboard_.is_bound())
    return ClipboardSequenceNumberToken();
  ClipboardSequenceNumberToken result;
  clipboard_->GetSequenceNumber(buffer_, &result);
  return result;
}

Vector<String> SystemClipboard::ReadAvailableTypes() {
  if (!IsValidBufferType(buffer_) || !clipboard_.is_bound())
    return {};
  Vector<String> types;
  clipboard_->ReadAvailableTypes(buffer_, &types);
  return types;
}

String SystemClipboard::ReadPlainText() {
  return ReadPlainText(buffer_);
}

String SystemClipboard::ReadPlainText(mojom::blink::ClipboardBuffer buffer) {
  if (!IsValidBufferType(buffer) || !clipboard_.is_bound())
    return String();
  String text;
  clipboard_->ReadText(buffer, &text);
  return text;
}

void SystemClipboard::ReadPlainText(
    mojom::blink::ClipboardBuffer buffer,
    mojom::blink::ClipboardHost::ReadTextCallback callback) {
  if (!IsValidBufferType(buffer) || !clipboard_.is_bound()) {
    std::move(callback).Run(String());
    return;
  }
  clipboard_->ReadText(buffer_, std::move(callback));
}

void SystemClipboard::WritePlainText(const String& plain_text,
                                     SmartReplaceOption) {
  if (!clipboard_.is_bound())
    return;
  // TODO(https://crbug.com/106449): add support for smart replace, which is
  // currently under-specified.
  String text = plain_text;
#if BUILDFLAG(IS_WIN)
  ReplaceNewlinesWithWindowsStyleNewlines(text);
#endif
  clipboard_->WriteText(NonNullString(text));
}

String SystemClipboard::ReadHTML(KURL& url,
                                 unsigned& fragment_start,
                                 unsigned& fragment_end) {
  String html;
  if (IsValidBufferType(buffer_) && clipboard_.is_bound()) {
    clipboard_->ReadHtml(buffer_, &html, &url,
                         static_cast<uint32_t*>(&fragment_start),
                         static_cast<uint32_t*>(&fragment_end));
  }
  if (html.empty()) {
    url = KURL();
    fragment_start = 0;
    fragment_end = 0;
  }
  return html;
}

void SystemClipboard::ReadHTML(
    mojom::blink::ClipboardHost::ReadHtmlCallback callback) {
  if (!IsValidBufferType(buffer_) || !clipboard_.is_bound()) {
    std::move(callback).Run(String(), KURL(), 0, 0);
    return;
  }
  clipboard_->ReadHtml(buffer_, std::move(callback));
}

void SystemClipboard::WriteHTML(const String& markup,
                                const KURL& document_url,
                                SmartReplaceOption smart_replace_option) {
  if (!clipboard_.is_bound())
    return;
  clipboard_->WriteHtml(NonNullString(markup), document_url);
  if (smart_replace_option == kCanSmartReplace)
    clipboard_->WriteSmartPasteMarker();
}

void SystemClipboard::ReadSvg(
    mojom::blink::ClipboardHost::ReadSvgCallback callback) {
  if (!IsValidBufferType(buffer_) || !clipboard_.is_bound()) {
    std::move(callback).Run(String());
    return;
  }
  clipboard_->ReadSvg(buffer_, std::move(callback));
}

void SystemClipboard::WriteSvg(const String& markup) {
  if (!clipboard_.is_bound())
    return;
  clipboard_->WriteSvg(NonNullString(markup));
}

String SystemClipboard::ReadRTF() {
  if (!IsValidBufferType(buffer_) || !clipboard_.is_bound())
    return String();
  String rtf;
  clipboard_->ReadRtf(buffer_, &rtf);
  return rtf;
}

mojo_base::BigBuffer SystemClipboard::ReadPng(
    mojom::blink::ClipboardBuffer buffer) {
  if (!IsValidBufferType(buffer) || !clipboard_.is_bound())
    return mojo_base::BigBuffer();
  mojo_base::BigBuffer png;
  clipboard_->ReadPng(buffer, &png);
  return png;
}

String SystemClipboard::ReadImageAsImageMarkup(
    mojom::blink::ClipboardBuffer buffer) {
  mojo_base::BigBuffer png_data = ReadPng(buffer);
  return PNGToImageMarkup(png_data);
}

void SystemClipboard::WriteImageWithTag(Image* image,
                                        const KURL& url,
                                        const String& title) {
  DCHECK(image);

  if (!clipboard_.is_bound())
    return;

  PaintImage paint_image = image->PaintImageForCurrentFrame();
  // Orient the data.
  if (!image->HasDefaultOrientation()) {
    paint_image = Image::ResizeAndOrientImage(
        paint_image, image->CurrentFrameOrientation(), gfx::Vector2dF(1, 1), 1,
        kInterpolationNone);
  }
  SkBitmap bitmap;
  if (sk_sp<SkImage> sk_image = paint_image.GetSwSkImage())
    sk_image->asLegacyBitmap(&bitmap);

  // The bitmap backing a canvas can be in non-native skia pixel order (aka
  // RGBA when kN32_SkColorType is BGRA-ordered, or higher bit-depth color-types
  // like F16. The IPC to the browser requires the bitmap to be in N32 format
  // so we convert it here if needed.
  SkBitmap n32_bitmap;
  if (skia::SkBitmapToN32OpaqueOrPremul(bitmap, &n32_bitmap) &&
      !n32_bitmap.isNull()) {
    clipboard_->WriteImage(n32_bitmap);
  }

  if (url.IsValid() && !url.IsEmpty()) {
#if !BUILDFLAG(IS_MAC)
    // See http://crbug.com/838808: Not writing text/plain on Mac for
    // consistency between platforms, and to help fix errors in applications
    // which prefer text/plain content over image content for compatibility with
    // Microsoft Word.
    clipboard_->WriteBookmark(url.GetString(), NonNullString(title));
#endif

    // When writing the image, we also write the image markup so that pasting
    // into rich text editors, such as Gmail, reveals the image. We also don't
    // want to call writeText(), since some applications (WordPad) don't pick
    // the image if there is also a text format on the clipboard.
    clipboard_->WriteHtml(URLToImageMarkup(url, title), KURL());
  }
}

void SystemClipboard::WriteImage(const SkBitmap& bitmap) {
  if (!clipboard_.is_bound())
    return;
  clipboard_->WriteImage(bitmap);
}

mojom::blink::ClipboardFilesPtr SystemClipboard::ReadFiles() {
  mojom::blink::ClipboardFilesPtr files;
  if (!IsValidBufferType(buffer_) || !clipboard_.is_bound())
    return files;
  clipboard_->ReadFiles(buffer_, &files);
  return files;
}

String SystemClipboard::ReadCustomData(const String& type) {
  if (!IsValidBufferType(buffer_) || !clipboard_.is_bound())
    return String();
  String data;
  clipboard_->ReadCustomData(buffer_, NonNullString(type), &data);
  return data;
}

void SystemClipboard::WriteDataObject(DataObject* data_object) {
  DCHECK(data_object);
  if (!clipboard_.is_bound())
    return;
  // This plagiarizes the logic in DropDataBuilder::Build, but only extracts the
  // data needed for the implementation of WriteDataObject.
  //
  // We avoid calling the WriteFoo functions if there is no data associated with
  // a type. This prevents stomping on clipboard contents that might have been
  // written by extension functions such as chrome.bookmarkManagerPrivate.copy.
  //
  // TODO(slangley): Use a mojo struct to send web_drag_data and allow receiving
  // side to extract the data required.
  // TODO(dcheng): Properly support text/uri-list here.
  HashMap<String, String> custom_data;
  WebDragData data = data_object->ToWebDragData();
  for (const WebDragData::Item& item : data.Items()) {
    if (item.storage_type == WebDragData::Item::kStorageTypeString) {
      if (item.string_type == kMimeTypeTextPlain) {
        clipboard_->WriteText(NonNullString(item.string_data));
      } else if (item.string_type == kMimeTypeTextHTML) {
        clipboard_->WriteHtml(NonNullString(item.string_data), KURL());
      } else if (item.string_type != kMimeTypeDownloadURL) {
        custom_data.insert(item.string_type, NonNullString(item.string_data));
      }
    }
  }
  if (!custom_data.empty()) {
    clipboard_->WriteCustomData(std::move(custom_data));
  }
}

void SystemClipboard::CommitWrite() {
  if (!clipboard_.is_bound())
    return;
  clipboard_->CommitWrite();
}

void SystemClipboard::CopyToFindPboard(const String& text) {
#if BUILDFLAG(IS_MAC)
  if (!clipboard_.is_bound())
    return;
  clipboard_->WriteStringToFindPboard(text);
#endif
}

void SystemClipboard::ReadAvailableCustomAndStandardFormats(
    mojom::blink::ClipboardHost::ReadAvailableCustomAndStandardFormatsCallback
        callback) {
  if (!clipboard_.is_bound())
    return;
  clipboard_->ReadAvailableCustomAndStandardFormats(std::move(callback));
}

void SystemClipboard::ReadUnsanitizedCustomFormat(
    const String& type,
    mojom::blink::ClipboardHost::ReadUnsanitizedCustomFormatCallback callback) {
  // TODO(ansollan): Add test coverage for all functions with this check in
  // |SystemClipboard| and consider if it's appropriate to throw exceptions or
  // reject promises if the context is detached.
  if (!clipboard_.is_bound())
    return;
  // The format size restriction is added in `ClipboardWriter::IsValidType`.
  DCHECK_LT(type.length(), mojom::blink::ClipboardHost::kMaxFormatSize);
  clipboard_->ReadUnsanitizedCustomFormat(type, std::move(callback));
}

void SystemClipboard::WriteUnsanitizedCustomFormat(const String& type,
                                                   mojo_base::BigBuffer data) {
  if (!clipboard_.is_bound() ||
      data.size() >= mojom::blink::ClipboardHost::kMaxDataSize) {
    return;
  }
  // The format size restriction is added in `ClipboardWriter::IsValidType`.
  DCHECK_LT(type.length(), mojom::blink::ClipboardHost::kMaxFormatSize);
  clipboard_->WriteUnsanitizedCustomFormat(type, std::move(data));
}

void SystemClipboard::Trace(Visitor* visitor) const {
  visitor->Trace(clipboard_);
}

bool SystemClipboard::IsValidBufferType(mojom::ClipboardBuffer buffer) {
  switch (buffer) {
    case mojom::ClipboardBuffer::kStandard:
      return true;
    case mojom::ClipboardBuffer::kSelection:
      return is_selection_buffer_available_;
  }
  return true;
}

}  // namespace blink
