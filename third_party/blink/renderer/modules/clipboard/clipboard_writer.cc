// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_writer.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/public/mojom/clipboard/raw_clipboard.mojom-blink.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/raw_system_clipboard.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {  // anonymous namespace for ClipboardWriter's derived classes.

// Writes a blob with image/png content to the System Clipboard.
class ClipboardImageWriter final : public ClipboardWriter {
 public:
  ClipboardImageWriter(SystemClipboard* system_clipboard,
                       ClipboardPromise* promise)
      : ClipboardWriter(system_clipboard, promise) {}
  ~ClipboardImageWriter() override = default;

 private:
  void StartWrite(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                  DOMArrayBuffer* raw_data) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(&ClipboardImageWriter::DecodeOnBackgroundThread,
                            WrapCrossThreadPersistent(this), task_runner,
                            WrapCrossThreadPersistent(raw_data)));
  }
  void DecodeOnBackgroundThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      DOMArrayBuffer* png_data) override {
    DCHECK(!IsMainThread());
    std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
        SegmentReader::CreateFromSkData(SkData::MakeWithoutCopy(
            png_data->Data(), png_data->ByteLengthAsSizeT())),
        true, ImageDecoder::kAlphaPremultiplied, ImageDecoder::kDefaultBitDepth,
        ColorBehavior::Tag());
    sk_sp<SkImage> image = nullptr;
    // |decoder| is nullptr if |png_data| doesn't begin with the PNG signature.
    if (decoder)
      image = ImageBitmap::GetSkImageFromDecoder(std::move(decoder));

    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBindOnce(&ClipboardImageWriter::Write,
                            WrapCrossThreadPersistent(this), std::move(image)));
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

// Writes a blob with text/plain content to the System Clipboard.
class ClipboardTextWriter final : public ClipboardWriter {
 public:
  ClipboardTextWriter(SystemClipboard* system_clipboard,
                      ClipboardPromise* promise)
      : ClipboardWriter(system_clipboard, promise) {}
  ~ClipboardTextWriter() override = default;

 private:
  void StartWrite(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                  DOMArrayBuffer* raw_data) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(&ClipboardTextWriter::DecodeOnBackgroundThread,
                            WrapCrossThreadPersistent(this), task_runner,
                            WrapCrossThreadPersistent(raw_data)));
  }
  void DecodeOnBackgroundThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      DOMArrayBuffer* raw_data) override {
    DCHECK(!IsMainThread());

    String wtf_string =
        String::FromUTF8(reinterpret_cast<const LChar*>(raw_data->Data()),
                         raw_data->ByteLengthAsSizeT());
    DCHECK(wtf_string.IsSafeToSendToAnotherThread());
    PostCrossThreadTask(*task_runner, FROM_HERE,
                        CrossThreadBindOnce(&ClipboardTextWriter::Write,
                                            WrapCrossThreadPersistent(this),
                                            std::move(wtf_string)));
  }
  void Write(const String& text) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    system_clipboard()->WritePlainText(text);

    promise_->CompleteWriteRepresentation();
  }
};

class ClipboardHtmlWriter final : public ClipboardWriter {
 public:
  ClipboardHtmlWriter(SystemClipboard* system_clipboard,
                      ClipboardPromise* promise)
      : ClipboardWriter(system_clipboard, promise) {}
  ~ClipboardHtmlWriter() override = default;

 private:
  void StartWrite(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                  DOMArrayBuffer* html_data) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    promise_->GetExecutionContext()->CountUse(
        WebFeature::kHtmlClipboardApiWrite);

    String html_string =
        String::FromUTF8(reinterpret_cast<const LChar*>(html_data->Data()),
                         html_data->ByteLengthAsSizeT());

    // Sanitizing on the main thread because HTML DOM nodes can only be used
    // on the main thread.
    KURL url;
    unsigned fragment_start = 0;
    unsigned fragment_end = html_string.length();

    Document* document = promise_->GetLocalFrame()->GetDocument();
    DocumentFragment* fragment = CreateSanitizedFragmentFromMarkupWithContext(
        *document, html_string, fragment_start, fragment_end, url);
    String sanitized_html =
        CreateMarkup(fragment, kIncludeNode, kResolveAllURLs);
    Write(sanitized_html, url);
  }

  void DecodeOnBackgroundThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      DOMArrayBuffer* html_data) override {
    NOTREACHED() << "HTML's serializers cannot be used on background threads.";
  }

  void Write(const String& sanitized_html, KURL url) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    system_clipboard()->WriteHTML(sanitized_html, url);
    promise_->CompleteWriteRepresentation();
  }
};

class ClipboardSvgWriter final : public ClipboardWriter {
 public:
  ClipboardSvgWriter(SystemClipboard* system_clipboard,
                     ClipboardPromise* promise)
      : ClipboardWriter(system_clipboard, promise) {}
  ~ClipboardSvgWriter() override = default;

 private:
  // This must be called on the main thread because XML DOM nodes can
  // only be used on the main thread
  void StartWrite(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                  DOMArrayBuffer* svg_data) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    String svg_string =
        String::FromUTF8(reinterpret_cast<const LChar*>(svg_data->Data()),
                         svg_data->ByteLengthAsSizeT());

    // Now sanitize the SVG string.
    KURL url;
    unsigned fragment_start = 0;
    unsigned fragment_end = svg_string.length();

    Document* document = promise_->GetLocalFrame()->GetDocument();
    DocumentFragment* fragment = CreateSanitizedFragmentFromMarkupWithContext(
        *document, svg_string, fragment_start, fragment_end, url);
    String sanitized_svg =
        CreateMarkup(fragment, kIncludeNode, kResolveAllURLs);
    Write(sanitized_svg);
  }

  void DecodeOnBackgroundThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      DOMArrayBuffer* html_data) override {
    NOTREACHED() << "SVG's serializers cannot be used on background threads.";
  }

  void Write(const String& svg_html) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    system_clipboard()->WriteSvg(svg_html);
    promise_->CompleteWriteRepresentation();
  }
};
// Writes a blob with arbitrary, unsanitized content to the System Clipboard.
class ClipboardRawDataWriter final : public ClipboardWriter {
 public:
  ClipboardRawDataWriter(RawSystemClipboard* raw_system_clipboard,
                         ClipboardPromise* promise,
                         String mime_type)
      : ClipboardWriter(raw_system_clipboard, promise), mime_type_(mime_type) {}
  ~ClipboardRawDataWriter() override = default;

 private:
  void StartWrite(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                  DOMArrayBuffer* raw_data) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(&ClipboardRawDataWriter::DecodeOnBackgroundThread,
                            WrapCrossThreadPersistent(this), task_runner,
                            WrapCrossThreadPersistent(raw_data)));
  }

  // Unfortunately, in order to use the same ClipboardWriter base,
  // ClipboardRawDataWriter does need to have these extra 2 thread hops.
  void DecodeOnBackgroundThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      DOMArrayBuffer* raw_data) override {
    DCHECK(!IsMainThread());

    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBindOnce(&ClipboardRawDataWriter::Write,
                            WrapCrossThreadPersistent(this),
                            WrapCrossThreadPersistent(raw_data)));
  }

  void Write(DOMArrayBuffer* raw_data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (raw_data->ByteLengthAsSizeT() >=
        mojom::blink::RawClipboardHost::kMaxDataSize) {
      promise_->RejectFromReadOrDecodeFailure();
      return;
    }

    uint8_t* raw_data_pointer = static_cast<uint8_t*>(raw_data->Data());
    mojo_base::BigBuffer buffer(std::vector<uint8_t>(
        raw_data_pointer, raw_data_pointer + raw_data->ByteLengthAsSizeT()));

    raw_system_clipboard()->Write(mime_type_, std::move(buffer));

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
  if (mime_type == kMimeTypeImagePng) {
    return MakeGarbageCollected<ClipboardImageWriter>(system_clipboard,
                                                      promise);
  }
  if (mime_type == kMimeTypeTextPlain)
    return MakeGarbageCollected<ClipboardTextWriter>(system_clipboard, promise);

  if (mime_type == kMimeTypeTextHTML)
    return MakeGarbageCollected<ClipboardHtmlWriter>(system_clipboard, promise);

  if (mime_type == kMimeTypeImageSvg &&
      RuntimeEnabledFeatures::ClipboardSvgEnabled())
    return MakeGarbageCollected<ClipboardSvgWriter>(system_clipboard, promise);

  NOTREACHED() << "Type " << mime_type << " was not implemented";
  return nullptr;
}

// static
ClipboardWriter* ClipboardWriter::Create(
    RawSystemClipboard* raw_system_clipboard,
    const String& mime_type,
    ClipboardPromise* promise) {
  DCHECK(base::FeatureList::IsEnabled(features::kRawClipboard));

  return MakeGarbageCollected<ClipboardRawDataWriter>(raw_system_clipboard,
                                                      promise, mime_type);
}

ClipboardWriter::ClipboardWriter(SystemClipboard* system_clipboard,
                                 ClipboardPromise* promise)
    : ClipboardWriter(system_clipboard, nullptr, promise) {}

ClipboardWriter::ClipboardWriter(RawSystemClipboard* raw_system_clipboard,
                                 ClipboardPromise* promise)
    : ClipboardWriter(nullptr, raw_system_clipboard, promise) {}

ClipboardWriter::ClipboardWriter(SystemClipboard* system_clipboard,
                                 RawSystemClipboard* raw_system_clipboard,
                                 ClipboardPromise* promise)
    : promise_(promise),
      clipboard_task_runner_(promise->GetExecutionContext()->GetTaskRunner(
          TaskType::kUserInteraction)),
      file_reading_task_runner_(promise->GetExecutionContext()->GetTaskRunner(
          TaskType::kFileReading)),
      system_clipboard_(system_clipboard),
      raw_system_clipboard_(raw_system_clipboard) {}

ClipboardWriter::~ClipboardWriter() = default;

// static
bool ClipboardWriter::IsValidType(const String& type, bool is_raw) {
  if (is_raw)
    return type.length() < mojom::blink::RawClipboardHost::kMaxFormatSize;

  // TODO(https://crbug.com/1029857): Add support for other types.
  return type == kMimeTypeImagePng || type == kMimeTypeTextPlain ||
         type == kMimeTypeTextHTML || type == kMimeTypeImageSvg;
}

void ClipboardWriter::WriteToSystem(Blob* blob) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!file_reader_);
  file_reader_ = std::make_unique<FileReaderLoader>(
      FileReaderLoader::kReadAsArrayBuffer, this,
      std::move(file_reading_task_runner_));
  file_reader_->Start(blob->GetBlobDataHandle());
}

// FileReaderLoaderClient implementation.

void ClipboardWriter::DidStartLoading() {}
void ClipboardWriter::DidReceiveData() {}

void ClipboardWriter::DidFinishLoading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DOMArrayBuffer* array_buffer = file_reader_->ArrayBufferResult();
  DCHECK(array_buffer);
  file_reader_.reset();

  StartWrite(clipboard_task_runner_, array_buffer);
}

void ClipboardWriter::DidFail(FileErrorCode error_code) {
  promise_->RejectFromReadOrDecodeFailure();
}

void ClipboardWriter::Trace(Visitor* visitor) const {
  visitor->Trace(promise_);
  visitor->Trace(system_clipboard_);
  visitor->Trace(raw_system_clipboard_);
}

}  // namespace blink
