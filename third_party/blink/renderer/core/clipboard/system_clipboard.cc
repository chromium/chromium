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
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
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

// This function is needed to clone a PendingRemote because normally they are
// not clonable.  The input PendingRemote is "cloned" twice, so one of those
// copies is intended to replace the original PendingRemote passed in by the
// caller.
std::pair<mojo::PendingRemote<mojom::blink::FileSystemAccessDataTransferToken>,
          mojo::PendingRemote<mojom::blink::FileSystemAccessDataTransferToken>>
CloneFsaToken(
    mojo::PendingRemote<mojom::blink::FileSystemAccessDataTransferToken> in) {
  if (!in.is_valid()) {
    return {mojo::NullRemote(), mojo::NullRemote()};
  }
  mojo::Remote remote(std::move(in));
  mojo::PendingRemote<mojom::blink::FileSystemAccessDataTransferToken> copy;
  remote->Clone(copy.InitWithNewPipeAndPassReceiver());
  return {remote.Unbind(), std::move(copy)};
}

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
  return buffer_ == mojom::blink::ClipboardBuffer::kSelection;
}

void SystemClipboard::SetSelectionMode(bool selection_mode) {
  buffer_ = selection_mode ? mojom::blink::ClipboardBuffer::kSelection
                           : mojom::blink::ClipboardBuffer::kStandard;
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

  if (snapshot_ && snapshot_->HasPlainText(buffer)) {
    return snapshot_->PlainText(buffer);
  }

  String text;
  clipboard_->ReadText(buffer, &text);
  if (snapshot_) {
    snapshot_->SetPlainText(buffer, text);
  }

  return text;
}

void SystemClipboard::ReadPlainText(
    mojom::blink::ClipboardBuffer buffer,
    mojom::blink::ClipboardHost::ReadTextCallback callback) {
  if (!IsValidBufferType(buffer) || !clipboard_.is_bound()) {
    std::move(callback).Run(String());
    return;
  }
  clipboard_->ReadText(buffer, std::move(callback));
}

void SystemClipboard::WritePlainText(const String& plain_text,
                                     SmartReplaceOption) {
  DCHECK(!snapshot_);

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
  if (!IsValidBufferType(buffer_) || !clipboard_.is_bound()) {
    url = KURL();
    fragment_start = 0;
    fragment_end = 0;
    return String();
  }

  if (snapshot_ && snapshot_->HasHtml(buffer_)) {
    url = snapshot_->Url(buffer_);
    fragment_start = snapshot_->FragmentStart(buffer_);
    fragment_end = snapshot_->FragmentEnd(buffer_);
    return snapshot_->Html(buffer_);
  }

  // NOTE: `fragment_start` and `fragment_end` can be the same reference, so
  // use local variables here to make sure the snapshot is set correctly.
  String html;
  uint32_t local_fragment_start;
  uint32_t local_fragment_end;
  clipboard_->ReadHtml(buffer_, &html, &url, &local_fragment_start,
                       &local_fragment_end);
  if (html.empty()) {
    url = KURL();
    local_fragment_start = 0;
    local_fragment_end = 0;
  }

  if (snapshot_) {
    snapshot_->SetHtml(buffer_, html, url, local_fragment_start,
                       local_fragment_end);
  }

  fragment_start = local_fragment_start;
  fragment_end = local_fragment_end;
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
  DCHECK(!snapshot_);

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
  DCHECK(!snapshot_);

  if (!clipboard_.is_bound())
    return;
  clipboard_->WriteSvg(NonNullString(markup));
}

String SystemClipboard::ReadRTF() {
  if (!IsValidBufferType(buffer_) || !clipboard_.is_bound())
    return String();

  if (snapshot_ && snapshot_->HasRtf(buffer_)) {
    return snapshot_->Rtf(buffer_);
  }

  String rtf;
  clipboard_->ReadRtf(buffer_, &rtf);
  if (snapshot_) {
    snapshot_->SetRtf(buffer_, rtf);
  }

  return rtf;
}

mojo_base::BigBuffer SystemClipboard::ReadPng(
    mojom::blink::ClipboardBuffer buffer) {
  if (!IsValidBufferType(buffer) || !clipboard_.is_bound())
    return mojo_base::BigBuffer();

  if (snapshot_ && snapshot_->HasPng(buffer)) {
    return snapshot_->Png(buffer);
  }

  mojo_base::BigBuffer png;
  clipboard_->ReadPng(buffer, &png);
  if (snapshot_) {
    snapshot_->SetPng(buffer, png);
  }

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
  DCHECK(!snapshot_);
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
  DCHECK(!snapshot_);

  if (!clipboard_.is_bound())
    return;
  clipboard_->WriteImage(bitmap);
}

mojom::blink::ClipboardFilesPtr SystemClipboard::ReadFiles() {
  mojom::blink::ClipboardFilesPtr files;
  if (!IsValidBufferType(buffer_) || !clipboard_.is_bound())
    return files;

  if (snapshot_ && snapshot_->HasFiles(buffer_)) {
    return snapshot_->Files(buffer_);
  }

  clipboard_->ReadFiles(buffer_, &files);
  if (snapshot_) {
    snapshot_->SetFiles(buffer_, files);
  }

  return files;
}

String SystemClipboard::ReadDataTransferCustomData(const String& type) {
  if (!IsValidBufferType(buffer_) || !clipboard_.is_bound())
    return String();

  if (snapshot_ && snapshot_->HasCustomData(buffer_, type)) {
    return snapshot_->CustomData(buffer_, type);
  }

  String data;
  clipboard_->ReadDataTransferCustomData(buffer_, NonNullString(type), &data);
  if (snapshot_) {
    snapshot_->SetCustomData(buffer_, type, data);
  }

  return data;
}

void SystemClipboard::WriteDataObject(DataObject* data_object) {
  DCHECK(!snapshot_);
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
  // TODO(crbug.com/332555471): Use a mojo struct to send web_drag_data and
  // allow receiving side to extract the data required.
  // TODO(crbug.com/332571415): Properly support text/uri-list here.
  HashMap<String, String> custom_data;
  WebDragData data = data_object->ToWebDragData();
  for (const WebDragData::Item& item : data.Items()) {
    if (const auto* string_item =
            absl::get_if<WebDragData::StringItem>(&item)) {
      if (string_item->type == kMimeTypeTextPlain) {
        clipboard_->WriteText(NonNullString(string_item->data));
      } else if (string_item->type == kMimeTypeTextHTML) {
        clipboard_->WriteHtml(NonNullString(string_item->data), KURL());
      } else if (string_item->type != kMimeTypeDownloadURL) {
        custom_data.insert(string_item->type, NonNullString(string_item->data));
      }
    }
  }
  if (!custom_data.empty()) {
    clipboard_->WriteDataTransferCustomData(std::move(custom_data));
  }
}

void SystemClipboard::CommitWrite() {
  DCHECK(!snapshot_);
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
  // TODO(crbug.com/332555472): Add test coverage for all functions with this
  //  check in `SystemClipboard` and consider if it's appropriate to throw
  // exceptions or reject promises if the context is detached.
  if (!clipboard_.is_bound())
    return;
  // The format size restriction is added in `ClipboardItem::supports`.
  DCHECK_LT(type.length(), mojom::blink::ClipboardHost::kMaxFormatSize);
  clipboard_->ReadUnsanitizedCustomFormat(type, std::move(callback));
}

void SystemClipboard::WriteUnsanitizedCustomFormat(const String& type,
                                                   mojo_base::BigBuffer data) {
  DCHECK(!snapshot_);

  if (!clipboard_.is_bound() ||
      data.size() >= mojom::blink::ClipboardHost::kMaxDataSize) {
    return;
  }
  // The format size restriction is added in `ClipboardItem::supports`.
  DCHECK_LT(type.length(), mojom::blink::ClipboardHost::kMaxFormatSize);
  clipboard_->WriteUnsanitizedCustomFormat(type, std::move(data));
}

void SystemClipboard::Trace(Visitor* visitor) const {
  visitor->Trace(clipboard_);
}

bool SystemClipboard::IsValidBufferType(mojom::blink::ClipboardBuffer buffer) {
  switch (buffer) {
    case mojom::blink::ClipboardBuffer::kStandard:
      return true;
    case mojom::blink::ClipboardBuffer::kSelection:
      return is_selection_buffer_available_;
  }
  return true;
}

void SystemClipboard::TakeSnapshot() {
  ++snapshot_count_;
  if (snapshot_count_ == 1) {
    DCHECK(!snapshot_);
    snapshot_ = std::make_unique<Snapshot>();
  }
}

void SystemClipboard::DropSnapshot() {
  DCHECK_GT(snapshot_count_, 0u);
  --snapshot_count_;
  if (snapshot_count_ == 0) {
    snapshot_.reset();
  }
}

SystemClipboard::Snapshot::Snapshot() = default;

SystemClipboard::Snapshot::~Snapshot() = default;

bool SystemClipboard::Snapshot::HasPlainText(
    mojom::blink::ClipboardBuffer buffer) const {
  return buffer_.has_value() && plain_text_.has_value();
}

const String& SystemClipboard::Snapshot::PlainText(
    mojom::blink::ClipboardBuffer buffer) const {
  DCHECK(HasPlainText(buffer));
  return plain_text_.value();
}

void SystemClipboard::Snapshot::SetPlainText(
    mojom::blink::ClipboardBuffer buffer,
    const String& text) {
  BindToBuffer(buffer);
  plain_text_ = text;
}

bool SystemClipboard::Snapshot::HasHtml(
    mojom::blink::ClipboardBuffer buffer) const {
  return buffer_.has_value() && html_.has_value();
}

const KURL& SystemClipboard::Snapshot::Url(
    mojom::blink::ClipboardBuffer buffer) const {
  DCHECK(HasHtml(buffer));
  return url_;
}

unsigned SystemClipboard::Snapshot::FragmentStart(
    mojom::blink::ClipboardBuffer buffer) const {
  DCHECK(HasHtml(buffer));
  return fragment_start_;
}

unsigned SystemClipboard::Snapshot::FragmentEnd(
    mojom::blink::ClipboardBuffer buffer) const {
  DCHECK(HasHtml(buffer));
  return fragment_end_;
}

const String& SystemClipboard::Snapshot::Html(
    mojom::blink::ClipboardBuffer buffer) const {
  DCHECK(HasHtml(buffer));
  return html_.value();
}

void SystemClipboard::Snapshot::SetHtml(mojom::blink::ClipboardBuffer buffer,
                                        const String& html,
                                        const KURL& url,
                                        unsigned fragment_start,
                                        unsigned fragment_end) {
  BindToBuffer(buffer);
  html_ = html;
  url_ = url;
  fragment_start_ = fragment_start;
  fragment_end_ = fragment_end;
}

bool SystemClipboard::Snapshot::HasRtf(
    mojom::blink::ClipboardBuffer buffer) const {
  return buffer_.has_value() && rtf_.has_value();
}

const String& SystemClipboard::Snapshot::Rtf(
    mojom::blink::ClipboardBuffer buffer) const {
  DCHECK(HasRtf(buffer));
  return rtf_.value();
}

void SystemClipboard::Snapshot::SetRtf(mojom::blink::ClipboardBuffer buffer,
                                       const String& rtf) {
  BindToBuffer(buffer);
  rtf_ = rtf;
}

bool SystemClipboard::Snapshot::HasPng(
    mojom::blink::ClipboardBuffer buffer) const {
  return buffer_.has_value() && png_.has_value();
}

mojo_base::BigBuffer SystemClipboard::Snapshot::Png(
    mojom::blink::ClipboardBuffer buffer) const {
  DCHECK(HasPng(buffer));
  // Make an owning copy of the png to return to user.
  base::span<const uint8_t> span = base::make_span(png_.value());
  return mojo_base::BigBuffer(span);
}

// TODO(https://crbug.com/1412180): Reduce data copies.
void SystemClipboard::Snapshot::SetPng(mojom::blink::ClipboardBuffer buffer,
                                       const mojo_base::BigBuffer& png) {
  BindToBuffer(buffer);
  // Make an owning copy of the png to save locally.
  base::span<const uint8_t> span = base::make_span(png);
  png_ = mojo_base::BigBuffer(span);
}

bool SystemClipboard::Snapshot::HasFiles(
    mojom::blink::ClipboardBuffer buffer) const {
  return buffer_.has_value() && files_.has_value();
}

mojom::blink::ClipboardFilesPtr SystemClipboard::Snapshot::Files(
    mojom::blink::ClipboardBuffer buffer) const {
  DCHECK(HasFiles(buffer));
  return CloneFiles(files_.value());
}

void SystemClipboard::Snapshot::SetFiles(
    mojom::blink::ClipboardBuffer buffer,
    mojom::blink::ClipboardFilesPtr& files) {
  BindToBuffer(buffer);
  files_ = CloneFiles(files);
}

bool SystemClipboard::Snapshot::HasCustomData(
    mojom::blink::ClipboardBuffer buffer,
    const String& type) const {
  return buffer_.has_value() && custom_data_.Contains(type);
}

String SystemClipboard::Snapshot::CustomData(
    mojom::blink::ClipboardBuffer buffer,
    const String& type) const {
  DCHECK(HasCustomData(buffer, type));
  return custom_data_.at(type);
}

void SystemClipboard::Snapshot::SetCustomData(
    mojom::blink::ClipboardBuffer buffer,
    const String& type,
    const String& data) {
  BindToBuffer(buffer);
  custom_data_.Set(type, data);
}

// static
mojom::blink::ClipboardFilesPtr SystemClipboard::Snapshot::CloneFiles(
    mojom::blink::ClipboardFilesPtr& files) {
  if (!files) {
    return {};
  }

  WTF::Vector<mojom::blink::DataTransferFilePtr> vec;
  for (auto& dtf : files->files) {
    auto clones = CloneFsaToken(std::move(dtf->file_system_access_token));
    dtf->file_system_access_token = std::move(clones.first);
    vec.emplace_back(mojom::blink::DataTransferFile::New(
        dtf->path, dtf->display_name, std::move(clones.second)));
  }

  return mojom::blink::ClipboardFiles::New(std::move(vec),
                                           files->file_system_id);
}

void SystemClipboard::Snapshot::BindToBuffer(
    mojom::blink::ClipboardBuffer buffer) {
  if (!buffer_) {
    buffer_ = buffer;
  } else {
    DCHECK_EQ(*buffer_, buffer);
  }
}

ScopedSystemClipboardSnapshot::ScopedSystemClipboardSnapshot(
    SystemClipboard& clipboard)
    : clipboard_(clipboard) {
  clipboard.TakeSnapshot();
}

ScopedSystemClipboardSnapshot::~ScopedSystemClipboardSnapshot() {
  clipboard_.DropSnapshot();
}

}  // namespace blink
