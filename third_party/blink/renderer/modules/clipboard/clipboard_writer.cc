// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_writer.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_supported_type.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/xml/dom_parser.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_skia.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace blink {

namespace {  // anonymous namespace for ClipboardWriter's derived classes.

// Base class for string-based clipboard writers
class ClipboardStringWriter : public ClipboardWriter {
 public:
  ClipboardStringWriter(SystemClipboard* system_clipboard,
                        ClipboardPromise* promise)
      : ClipboardWriter(system_clipboard, promise) {}
  ~ClipboardStringWriter() override = default;

 protected:
  // FileReaderClient implementation for string-based writers
  void DidFinishLoading(FileReaderData contents) override {
    if (!CleanupAfterFileReaderFinishedAndCheckIfCanProceed()) {
      return;
    }

    WriteString(std::move(contents).AsText("UTF-8"));
  }
};

// Writes image/png content to the System Clipboard.
class ClipboardImageWriter final : public ClipboardWriter {
 public:
  ClipboardImageWriter(SystemClipboard* system_clipboard,
                       ClipboardPromise* promise)
      : ClipboardWriter(system_clipboard, promise) {}
  ~ClipboardImageWriter() override = default;

  // FileReaderClient implementation for binary data
  void DidFinishLoading(FileReaderData contents) override {
    if (!CleanupAfterFileReaderFinishedAndCheckIfCanProceed()) {
      return;
    }

    StartBinaryWrite(std::move(contents).AsArrayBufferContents());
  }

 private:
  void StartBinaryWrite(ArrayBufferContents raw_data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // ArrayBufferContents is a thread-safe smart pointer around the backing
    // store
    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(&ClipboardImageWriter::DecodeOnBackgroundThread,
                            std::move(raw_data), MakeCrossThreadHandle(this),
                            clipboard_task_runner()));
  }

  static void DecodeOnBackgroundThread(
      ArrayBufferContents png_data,
      CrossThreadHandle<ClipboardImageWriter> writer,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    DCHECK(!IsMainThread());
    std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
        SegmentReader::CreateFromSkData(
            SkData::MakeWithoutCopy(png_data.Data(), png_data.DataLength())),
        /*data_complete=*/true, ImageDecoder::kAlphaPremultiplied,
        ImageDecoder::kDefaultBitDepth, ColorBehavior::kTag,
        cc::AuxImage::kDefault, Platform::GetMaxDecodedImageBytes());
    sk_sp<SkImage> image = nullptr;
    // `decoder` is nullptr if `png_data` doesn't begin with the PNG signature.
    if (decoder) {
      image = ImageBitmap::GetSkImageFromDecoder(std::move(decoder));
    }

    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBindOnce(&ClipboardImageWriter::Write,
                            MakeUnwrappingCrossThreadHandle(std::move(writer)),
                            std::move(image)));
  }

  void Write(sk_sp<SkImage> image) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!image) {
      promise_->RejectFromReadOrDecodeFailure();
      return;
    }
    SkBitmap bitmap;
    image->asLegacyBitmap(&bitmap);
    system_clipboard()->WriteImage(std::move(bitmap));
    promise_->CompleteWriteRepresentation();
  }
};

// Writes text/plain content to the System Clipboard.
class ClipboardTextWriter final : public ClipboardStringWriter {
 public:
  ClipboardTextWriter(SystemClipboard* system_clipboard,
                      ClipboardPromise* promise)
      : ClipboardStringWriter(system_clipboard, promise) {}
  ~ClipboardTextWriter() override = default;

 private:
  void WriteString(String text) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    system_clipboard()->WritePlainText(std::move(text));
    promise_->CompleteWriteRepresentation();
  }
};

// Writes text/html content to the System Clipboard.
class ClipboardHtmlWriter final : public ClipboardStringWriter {
 public:
  ClipboardHtmlWriter(SystemClipboard* system_clipboard,
                      ClipboardPromise* promise)
      : ClipboardStringWriter(system_clipboard, promise) {}
  ~ClipboardHtmlWriter() override = default;

 private:
  void WriteString(String html_string) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    LocalFrame* local_frame = promise_->GetLocalFrame();
    auto* execution_context = promise_->GetExecutionContext();
    if (!execution_context) {
      return;
    }
    const KURL& url = local_frame->GetDocument()->Url();
    DOMParser* dom_parser = DOMParser::Create(promise_->GetScriptState());

    const Document* doc = dom_parser->ParseFromStringWithoutTrustedTypes(
        std::move(html_string),
        V8SupportedType(V8SupportedType::Enum::kTextHtml));
    DCHECK(doc);

    system_clipboard()->WriteHTML(
        CreateMarkup(doc, kIncludeNode, kResolveAllURLs), url);
    promise_->CompleteWriteRepresentation();
  }
};

// Write image/svg+xml content to the System Clipboard.
class ClipboardSvgWriter final : public ClipboardStringWriter {
 public:
  ClipboardSvgWriter(SystemClipboard* system_clipboard,
                     ClipboardPromise* promise)
      : ClipboardStringWriter(system_clipboard, promise) {}
  ~ClipboardSvgWriter() override = default;

 private:
  void WriteString(String svg_string) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DOMParser* dom_parser = DOMParser::Create(promise_->GetScriptState());
    const Document* doc = dom_parser->ParseFromStringWithoutTrustedTypes(
        std::move(svg_string),
        V8SupportedType(V8SupportedType::Enum::kImageSvgXml));
    promise_->GetExecutionContext()->CountUse(WebFeature::kClipboardSvgWrite);
    system_clipboard()->WriteSvg(
        CreateMarkup(doc, kIncludeNode, kResolveAllURLs));
    promise_->CompleteWriteRepresentation();
  }
};

// Writes arbitrary, unsanitized content to the System Clipboard.
class ClipboardCustomFormatWriter final : public ClipboardWriter {
 public:
  ClipboardCustomFormatWriter(SystemClipboard* system_clipboard,
                              ClipboardPromise* promise,
                              const String& mime_type)
      : ClipboardWriter(system_clipboard, promise), mime_type_(mime_type) {}
  ~ClipboardCustomFormatWriter() override = default;

  // FileReaderClient implementation for binary data
  void DidFinishLoading(FileReaderData contents) override {
    if (!CleanupAfterFileReaderFinishedAndCheckIfCanProceed()) {
      return;
    }

    ArrayBufferContents array_buffer =
        std::move(contents).AsArrayBufferContents();
    Write(array_buffer.ByteSpan());
  }

 private:
  // Handle DOM string data
  void WriteString(String text) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Write(base::as_byte_span(text.Utf8()));
  }

  // Common write method for both string and binary data
  void Write(base::span<const uint8_t> data) {
    promise_->GetExecutionContext()->CountUse(
        WebFeature::kClipboardCustomFormatWrite);
    if (data.size() >= mojom::blink::ClipboardHost::kMaxDataSize) {
      promise_->RejectFromReadOrDecodeFailure();
      return;
    }
    system_clipboard()->WriteUnsanitizedCustomFormat(mime_type_, data);
    promise_->CompleteWriteRepresentation();
  }

  String mime_type_;
};

}  // anonymous namespace

// ClipboardWriter functions.

// static
ClipboardWriter* ClipboardWriter::Create(SystemClipboard* system_clipboard,
                                         const String& mime_type,
                                         ClipboardPromise* promise) {
  CHECK(ClipboardItem::supports(mime_type));
  String web_custom_format = Clipboard::ParseWebCustomFormat(mime_type);
  if (!web_custom_format.empty()) {
    // We write the custom MIME type without the "web " prefix into the web
    // custom format map so native applications don't have to add any string
    // parsing logic to read format from clipboard.
    return MakeGarbageCollected<ClipboardCustomFormatWriter>(
        system_clipboard, promise, web_custom_format);
  }

  if (mime_type == ui::kMimeTypePng) {
    return MakeGarbageCollected<ClipboardImageWriter>(system_clipboard,
                                                      promise);
  }

  if (mime_type == ui::kMimeTypePlainText) {
    return MakeGarbageCollected<ClipboardTextWriter>(system_clipboard, promise);
  }

  if (mime_type == ui::kMimeTypeHtml) {
    return MakeGarbageCollected<ClipboardHtmlWriter>(system_clipboard, promise);
  }

  if (mime_type == ui::kMimeTypeSvg) {
    return MakeGarbageCollected<ClipboardSvgWriter>(system_clipboard, promise);
  }

  NOTREACHED()
      << "IsValidType() and Create() have inconsistent implementations.";
}

ClipboardWriter::ClipboardWriter(SystemClipboard* system_clipboard,
                                 ClipboardPromise* promise)
    : promise_(promise),
      clipboard_task_runner_(promise->GetExecutionContext()->GetTaskRunner(
          TaskType::kUserInteraction)),
      file_reading_task_runner_(promise->GetExecutionContext()->GetTaskRunner(
          TaskType::kFileReading)),
      system_clipboard_(system_clipboard) {}

ClipboardWriter::~ClipboardWriter() = default;

bool ClipboardWriter::CleanupAfterFileReaderFinishedAndCheckIfCanProceed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  self_keep_alive_.Clear();
  file_reader_ = nullptr;
  return promise_->GetLocalFrame();
}

void ClipboardWriter::WriteString(String text) {
  promise_->RejectFromReadOrDecodeFailure();
}

void ClipboardWriter::WriteToSystem(V8UnionBlobOrString* clipboard_item_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(promise_->GetLocalFrame());
  if (clipboard_item_data->IsBlob()) {
    DCHECK(!file_reader_);
    file_reader_ = MakeGarbageCollected<FileReaderLoader>(
        this, std::move(file_reading_task_runner_));
    file_reader_->Start(clipboard_item_data->GetAsBlob()->GetBlobDataHandle());
  } else if (clipboard_item_data->IsString()) {
    DCHECK(RuntimeEnabledFeatures::ClipboardItemWithDOMStringSupportEnabled());
    WriteString(clipboard_item_data->GetAsString());
  } else {
    NOTREACHED();
  }
}

void ClipboardWriter::DidFail(FileErrorCode error_code) {
  FileReaderAccumulator::DidFail(error_code);
  self_keep_alive_.Clear();
  file_reader_ = nullptr;
  promise_->RejectFromReadOrDecodeFailure();
}

void ClipboardWriter::Trace(Visitor* visitor) const {
  FileReaderAccumulator::Trace(visitor);
  visitor->Trace(promise_);
  visitor->Trace(system_clipboard_);
  visitor->Trace(file_reader_);
}

}  // namespace blink
