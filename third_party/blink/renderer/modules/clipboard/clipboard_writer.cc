// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/clipboard/clipboard_writer.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_supported_type.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
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

// Writes a Blob with image/png content to the System Clipboard.
class ClipboardImageWriter final : public ClipboardWriter {
 public:
  ClipboardImageWriter(SystemClipboard* system_clipboard,
                       ClipboardPromise* promise)
      : ClipboardWriter(system_clipboard, promise) {}
  ~ClipboardImageWriter() override = default;

 private:
  void StartWrite(
      DOMArrayBuffer* raw_data,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // ArrayBufferContents is a thread-safe smart pointer around the backing
    // store.
    ArrayBufferContents contents = *raw_data->Content();
    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(&ClipboardImageWriter::DecodeOnBackgroundThread,
                            std::move(contents), MakeCrossThreadHandle(this),
                            task_runner));
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
    if (!promise_->GetLocalFrame()) {
      return;
    }
    SkBitmap bitmap;
    image->asLegacyBitmap(&bitmap);
    system_clipboard()->WriteImage(std::move(bitmap));
    promise_->CompleteWriteRepresentation();
  }
};

// Writes a Blob with text/plain content to the System Clipboard.
class ClipboardTextWriter final : public ClipboardWriter {
 public:
  ClipboardTextWriter(SystemClipboard* system_clipboard,
                      ClipboardPromise* promise)
      : ClipboardWriter(system_clipboard, promise) {}
  ~ClipboardTextWriter() override = default;

 private:
  void StartWrite(
      DOMArrayBuffer* raw_data,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // ArrayBufferContents is a thread-safe smart pointer around the backing
    // store.
    ArrayBufferContents contents = *raw_data->Content();
    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(&ClipboardTextWriter::DecodeOnBackgroundThread,
                            std::move(contents), MakeCrossThreadHandle(this),
                            task_runner));
  }
  static void DecodeOnBackgroundThread(
      ArrayBufferContents raw_data,
      CrossThreadHandle<ClipboardTextWriter> writer,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    DCHECK(!IsMainThread());

    String wtf_string = String::FromUTF8(
        reinterpret_cast<const LChar*>(raw_data.Data()), raw_data.DataLength());
    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBindOnce(&ClipboardTextWriter::Write,
                            MakeUnwrappingCrossThreadHandle(std::move(writer)),
                            std::move(wtf_string)));
  }
  void Write(const String& text) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!promise_->GetLocalFrame()) {
      return;
    }
    system_clipboard()->WritePlainText(text);

    promise_->CompleteWriteRepresentation();
  }
};

// Writes a blob with text/html content to the System Clipboard.
class ClipboardHtmlWriter final : public ClipboardWriter {
 public:
  ClipboardHtmlWriter(SystemClipboard* system_clipboard,
                      ClipboardPromise* promise)
      : ClipboardWriter(system_clipboard, promise) {}
  ~ClipboardHtmlWriter() override = default;

 private:
  void StartWrite(
      DOMArrayBuffer* html_data,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    LocalFrame* local_frame = promise_->GetLocalFrame();
    auto* execution_context = promise_->GetExecutionContext();
    if (!local_frame || !execution_context) {
      return;
    }
    String html_string =
        String::FromUTF8(reinterpret_cast<const LChar*>(html_data->Data()),
                         html_data->ByteLength());
    const KURL& url = local_frame->GetDocument()->Url();
    DOMParser* dom_parser = DOMParser::Create(promise_->GetScriptState());
    const Document* doc = dom_parser->parseFromString(
        html_string, V8SupportedType(V8SupportedType::Enum::kTextHtml));
    DCHECK(doc);
    String serialized_html = CreateMarkup(doc, kIncludeNode, kResolveAllURLs);
    Write(serialized_html, url);
  }

  void Write(const String& serialized_html, const KURL& url) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    system_clipboard()->WriteHTML(serialized_html, url);
    promise_->CompleteWriteRepresentation();
  }
};

// Writes a blob with image/svg+xml content to the System Clipboard.
class ClipboardSvgWriter final : public ClipboardWriter {
 public:
  ClipboardSvgWriter(SystemClipboard* system_clipboard,
                     ClipboardPromise* promise)
      : ClipboardWriter(system_clipboard, promise) {}
  ~ClipboardSvgWriter() override = default;

 private:
  void StartWrite(
      DOMArrayBuffer* svg_data,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    String svg_string =
        String::FromUTF8(reinterpret_cast<const LChar*>(svg_data->Data()),
                         svg_data->ByteLength());

    LocalFrame* local_frame = promise_->GetLocalFrame();
    if (!local_frame) {
      return;
    }

    DOMParser* dom_parser = DOMParser::Create(promise_->GetScriptState());
    const Document* doc = dom_parser->parseFromString(
        svg_string, V8SupportedType(V8SupportedType::Enum::kImageSvgXml));
    Write(CreateMarkup(doc, kIncludeNode, kResolveAllURLs));
  }

  void Write(const String& svg_html) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    system_clipboard()->WriteSvg(svg_html);
    promise_->CompleteWriteRepresentation();
  }
};

// Writes a Blob with arbitrary, unsanitized content to the System Clipboard.
class ClipboardCustomFormatWriter final : public ClipboardWriter {
 public:
  ClipboardCustomFormatWriter(SystemClipboard* system_clipboard,
                              ClipboardPromise* promise,
                              const String& mime_type)
      : ClipboardWriter(system_clipboard, promise), mime_type_(mime_type) {}
  ~ClipboardCustomFormatWriter() override = default;

 private:
  void StartWrite(
      DOMArrayBuffer* custom_format_data,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Write(custom_format_data);
  }

  void Write(DOMArrayBuffer* custom_format_data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!promise_->GetLocalFrame()) {
      return;
    }
    if (custom_format_data->ByteLength() >=
        mojom::blink::ClipboardHost::kMaxDataSize) {
      promise_->RejectFromReadOrDecodeFailure();
      return;
    }
    mojo_base::BigBuffer buffer(
        base::make_span(static_cast<uint8_t*>(custom_format_data->Data()),
                        custom_format_data->ByteLength()));
    system_clipboard()->WriteUnsanitizedCustomFormat(mime_type_,
                                                     std::move(buffer));
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

  if (mime_type == kMimeTypeImagePng) {
    return MakeGarbageCollected<ClipboardImageWriter>(system_clipboard,
                                                      promise);
  }

  if (mime_type == kMimeTypeTextPlain) {
    return MakeGarbageCollected<ClipboardTextWriter>(system_clipboard, promise);
  }

  if (mime_type == kMimeTypeTextHTML) {
    return MakeGarbageCollected<ClipboardHtmlWriter>(system_clipboard, promise);
  }

  if (mime_type == kMimeTypeImageSvg) {
    return MakeGarbageCollected<ClipboardSvgWriter>(system_clipboard, promise);
  }

  NOTREACHED_IN_MIGRATION()
      << "IsValidType() and Create() have inconsistent implementations.";
  return nullptr;
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

void ClipboardWriter::WriteToSystem(Blob* blob) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!file_reader_);
  file_reader_ = MakeGarbageCollected<FileReaderLoader>(
      this, std::move(file_reading_task_runner_));
  file_reader_->Start(blob->GetBlobDataHandle());
}

// FileReaderClient implementation.
void ClipboardWriter::DidFinishLoading(FileReaderData contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DOMArrayBuffer* array_buffer = std::move(contents).AsDOMArrayBuffer();
  DCHECK(array_buffer);

  self_keep_alive_.Clear();
  file_reader_ = nullptr;

  StartWrite(array_buffer, clipboard_task_runner_);
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
