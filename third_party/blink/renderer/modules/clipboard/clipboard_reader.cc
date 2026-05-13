// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/clipboard/clipboard_reader.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace blink {

namespace {  // anonymous namespace for ClipboardReader's derived classes.

// Reads a PNG from the System Clipboard as a Blob with image/png content.
// Since the data returned from ReadPng() is already in the desired format, no
// encoding is required and the blob is created directly from Read().
class ClipboardPngReader final : public ClipboardReader {
 public:
  explicit ClipboardPngReader(SystemClipboard* system_clipboard,
                              ClipboardReaderResultHandler* result_handler)
      : ClipboardReader(system_clipboard, result_handler) {}
  ~ClipboardPngReader() override = default;

  ClipboardPngReader(const ClipboardPngReader&) = delete;
  ClipboardPngReader& operator=(const ClipboardPngReader&) = delete;

  void Read() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Async path (crbug.com/474131935): keep the renderer main thread
    // responsive while the OS clipboard read is in flight.
    system_clipboard()->ReadPng(
        mojom::blink::ClipboardBuffer::kStandard,
        BindOnce(&ClipboardPngReader::OnReadPng, WrapPersistent(this)));
  }

 private:
  void OnReadPng(mojo_base::BigBuffer data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Blob* blob = nullptr;
    if (RuntimeEnabledFeatures::
            ReadClipboardDataOnClipboardItemGetTypeEnabled() ||
        data.size()) {
      blob = Blob::Create(data, ui::kMimeTypePng);
    }
    base::UmaHistogramBoolean("Blink.Clipboard.Reader.ProcessedDataNull",
                              !data.size());
    result_handler_->OnRead(blob, ui::kMimeTypePng);
  }

  void NextRead(Vector<uint8_t> utf8_bytes) override { NOTREACHED(); }
};

// Reads an image from the System Clipboard as a Blob with text/plain content.
class ClipboardTextReader final : public ClipboardReader {
 public:
  explicit ClipboardTextReader(SystemClipboard* system_clipboard,
                               ClipboardReaderResultHandler* result_handler)
      : ClipboardReader(system_clipboard, result_handler) {}
  ~ClipboardTextReader() override = default;

  ClipboardTextReader(const ClipboardTextReader&) = delete;
  ClipboardTextReader& operator=(const ClipboardTextReader&) = delete;

  void Read() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    system_clipboard()->ReadPlainText(
        mojom::blink::ClipboardBuffer::kStandard,
        BindOnce(&ClipboardTextReader::OnRead, WrapPersistent(this)));
  }

 private:
  void OnRead(const String& plain_text) {
    base::UmaHistogramBoolean("Blink.Clipboard.Reader.ProcessedDataNull",
                              plain_text.empty());
    if (plain_text.empty()) {
      NextRead(Vector<uint8_t>());
      return;
    }

    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(&ClipboardTextReader::EncodeOnBackgroundThread,
                            std::move(plain_text), MakeCrossThreadHandle(this),
                            std::move(clipboard_task_runner_)));
  }

  static void EncodeOnBackgroundThread(
      String plain_text,
      CrossThreadHandle<ClipboardTextReader> reader,
      scoped_refptr<base::SingleThreadTaskRunner> clipboard_task_runner) {
    DCHECK(!IsMainThread());

    // Encode WTF String to UTF-8, the standard text format for Blobs.
    StringUtf8Adaptor utf8_text(plain_text);
    Vector<uint8_t> utf8_bytes;
    utf8_bytes.ReserveInitialCapacity(utf8_text.size());
    utf8_bytes.append_range(utf8_text);

    PostCrossThreadTask(
        *clipboard_task_runner, FROM_HERE,
        CrossThreadBindOnce(
            &ClipboardTextReader::NextRead,
            MakeUnwrappingCrossThreadHandle<ClipboardTextReader>(
                std::move(reader)),
            std::move(utf8_bytes)));
  }

  void NextRead(Vector<uint8_t> utf8_bytes) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Blob* blob = nullptr;
    if (RuntimeEnabledFeatures::
            ReadClipboardDataOnClipboardItemGetTypeEnabled() ||
        utf8_bytes.size()) {
      blob = Blob::Create(utf8_bytes, ui::kMimeTypePlainText);
    }
    result_handler_->OnRead(blob, ui::kMimeTypePlainText);
  }
};

// Reads HTML from the System Clipboard as a Blob with text/html content.
class ClipboardHtmlReader final : public ClipboardReader {
 public:
  explicit ClipboardHtmlReader(SystemClipboard* system_clipboard,
                               ClipboardReaderResultHandler* result_handler,
                               bool sanitize_html)
      : ClipboardReader(system_clipboard, result_handler),
        sanitize_html_(sanitize_html) {}
  ~ClipboardHtmlReader() override = default;

  void Read() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (ExecutionContext* context = result_handler_->GetExecutionContext()) {
      context->CountUse(sanitize_html_
                            ? WebFeature::kHtmlClipboardApiRead
                            : WebFeature::kHtmlClipboardApiUnsanitizedRead);
    }
    system_clipboard()->ReadHTML(
        BindOnce(&ClipboardHtmlReader::OnRead, WrapPersistent(this)));
  }

 private:
  void OnRead(const String& html_string,
              const KURL& url,
              unsigned fragment_start,
              unsigned fragment_end) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_GE(fragment_start, 0u);
    DCHECK_LE(fragment_end, html_string.length());
    DCHECK_LE(fragment_start, fragment_end);

    LocalFrame* frame = result_handler_->GetLocalFrame();
    if (!frame || html_string.empty()) {
      NextRead(Vector<uint8_t>());
      return;
    }

    // Process the HTML string and strip out certain security sensitive tags if
    // needed. `CreateStrictlyProcessedMarkupWithContext` must be called on the
    // main thread because HTML DOM nodes can only be used on the main thread.
    String final_html =
        sanitize_html_ ? CreateStrictlyProcessedMarkupWithContext(
                             *frame->GetDocument(), html_string, fragment_start,
                             fragment_end, url, kIncludeNode, kResolveAllURLs)
                       : html_string;
    base::UmaHistogramBoolean("Blink.Clipboard.Reader.ProcessedDataNull",
                              final_html.empty());
    if (final_html.empty()) {
      NextRead(Vector<uint8_t>());
      return;
    }
    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(&ClipboardHtmlReader::EncodeOnBackgroundThread,
                            std::move(final_html), MakeCrossThreadHandle(this),
                            std::move(clipboard_task_runner_)));
  }

  static void EncodeOnBackgroundThread(
      String plain_text,
      CrossThreadHandle<ClipboardHtmlReader> reader,
      scoped_refptr<base::SingleThreadTaskRunner> clipboard_task_runner) {
    DCHECK(!IsMainThread());

    // Encode WTF String to UTF-8, the standard text format for blobs.
    StringUtf8Adaptor utf8_text(plain_text);
    Vector<uint8_t> utf8_bytes;
    utf8_bytes.ReserveInitialCapacity(utf8_text.size());
    utf8_bytes.append_range(utf8_text);

    PostCrossThreadTask(
        *clipboard_task_runner, FROM_HERE,
        CrossThreadBindOnce(&ClipboardHtmlReader::NextRead,
                            MakeUnwrappingCrossThreadHandle(std::move(reader)),
                            std::move(utf8_bytes)));
  }

  void NextRead(Vector<uint8_t> utf8_bytes) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Blob* blob = nullptr;
    if (RuntimeEnabledFeatures::
            ReadClipboardDataOnClipboardItemGetTypeEnabled() ||
        utf8_bytes.size()) {
      blob = Blob::Create(utf8_bytes, ui::kMimeTypeHtml);
    }
    result_handler_->OnRead(blob, ui::kMimeTypeHtml);
  }

  bool sanitize_html_ = true;
};

// Reads SVG from the System Clipboard as a Blob with image/svg+xml content.
class ClipboardSvgReader final : public ClipboardReader {
 public:
  ClipboardSvgReader(SystemClipboard* system_clipboard,
                     ClipboardReaderResultHandler* result_handler)
      : ClipboardReader(system_clipboard, result_handler) {}
  ~ClipboardSvgReader() override = default;

  // This must be called on the main thread because XML DOM nodes can
  // only be used on the main thread.
  void Read() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (ExecutionContext* context = result_handler_->GetExecutionContext()) {
      context->CountUse(WebFeature::kClipboardSvgRead);
    }
    system_clipboard()->ReadSvg(
        BindOnce(&ClipboardSvgReader::OnRead, WrapPersistent(this)));
  }

 private:
  void OnRead(const String& svg_string) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    LocalFrame* frame = result_handler_->GetLocalFrame();
    if (!frame) {
      NextRead(Vector<uint8_t>());
      return;
    }

    // Now process the SVG string and strip out certain security sensitive tags.
    KURL url;
    unsigned fragment_start = 0;
    String strictly_processed_svg = CreateStrictlyProcessedMarkupWithContext(
        *frame->GetDocument(), svg_string, fragment_start, svg_string.length(),
        url, kIncludeNode, kResolveAllURLs);

    base::UmaHistogramBoolean("Blink.Clipboard.Reader.ProcessedDataNull",
                              strictly_processed_svg.empty());
    if (strictly_processed_svg.empty()) {
      NextRead(Vector<uint8_t>());
      return;
    }
    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(&ClipboardSvgReader::EncodeOnBackgroundThread,
                            std::move(strictly_processed_svg),
                            MakeCrossThreadHandle(this),
                            std::move(clipboard_task_runner_)));
  }

  static void EncodeOnBackgroundThread(
      String plain_text,
      CrossThreadHandle<ClipboardSvgReader> reader,
      scoped_refptr<base::SingleThreadTaskRunner> clipboard_task_runner) {
    DCHECK(!IsMainThread());

    // Encode WTF String to UTF-8, the standard text format for Blobs.
    StringUtf8Adaptor utf8_text(plain_text);
    Vector<uint8_t> utf8_bytes;
    utf8_bytes.ReserveInitialCapacity(utf8_text.size());
    utf8_bytes.append_range(utf8_text);

    PostCrossThreadTask(
        *clipboard_task_runner, FROM_HERE,
        CrossThreadBindOnce(&ClipboardSvgReader::NextRead,
                            MakeUnwrappingCrossThreadHandle(std::move(reader)),
                            std::move(utf8_bytes)));
  }

  void NextRead(Vector<uint8_t> utf8_bytes) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Blob* blob = nullptr;
    if (utf8_bytes.size()) {
      blob = Blob::Create(utf8_bytes, ui::kMimeTypeSvg);
    }
    result_handler_->OnRead(blob, ui::kMimeTypeSvg);
  }
};

// Reads unsanitized custom formats from the System Clipboard as a Blob with
// custom MIME type content.
class ClipboardCustomFormatReader final : public ClipboardReader {
 public:
  explicit ClipboardCustomFormatReader(
      SystemClipboard* system_clipboard,
      ClipboardReaderResultHandler* result_handler,
      const String& mime_type)
      : ClipboardReader(system_clipboard, result_handler),
        mime_type_(mime_type) {}
  ~ClipboardCustomFormatReader() override = default;

  void Read() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (ExecutionContext* context = result_handler_->GetExecutionContext()) {
      context->CountUse(WebFeature::kClipboardCustomFormatRead);
    }
    system_clipboard()->ReadUnsanitizedCustomFormat(
        mime_type_, BindOnce(&ClipboardCustomFormatReader::OnCustomFormatRead,
                             WrapPersistent(this)));
  }

  void OnCustomFormatRead(mojo_base::BigBuffer data) {
    Blob* blob = Blob::Create(data, mime_type_);
    result_handler_->OnRead(blob, mime_type_);
  }

 private:
  void NextRead(Vector<uint8_t> utf8_bytes) override {}

  String mime_type_;
};

}  // anonymous namespace

// ClipboardReader functions.

// static
ClipboardReader* ClipboardReader::Create(
    SystemClipboard* system_clipboard,
    const String& mime_type,
    ClipboardReaderResultHandler* result_handler,
    bool sanitize_html) {
  CHECK(ClipboardItem::supports(mime_type));
  // If this is a web custom format then read the unsanitized version.
  if (!Clipboard::ParseWebCustomFormat(mime_type).empty()) {
    // We read the custom MIME type that has the "web " prefix.
    // These MIME types are found in the web custom format map written by
    // native applications.
    return MakeGarbageCollected<ClipboardCustomFormatReader>(
        system_clipboard, result_handler, mime_type);
  }

  if (mime_type == ui::kMimeTypePng) {
    return MakeGarbageCollected<ClipboardPngReader>(system_clipboard,
                                                    result_handler);
  }

  if (mime_type == ui::kMimeTypePlainText) {
    return MakeGarbageCollected<ClipboardTextReader>(system_clipboard,
                                                     result_handler);
  }

  if (mime_type == ui::kMimeTypeHtml) {
    return MakeGarbageCollected<ClipboardHtmlReader>(
        system_clipboard, result_handler, sanitize_html);
  }

  if (mime_type == ui::kMimeTypeSvg) {
    return MakeGarbageCollected<ClipboardSvgReader>(system_clipboard,
                                                    result_handler);
  }

  NOTREACHED()
      << "IsValidType() and Create() have inconsistent implementations.";
}

ClipboardReader::ClipboardReader(SystemClipboard* system_clipboard,
                                 ClipboardReaderResultHandler* result_handler)
    : clipboard_task_runner_(
          result_handler->GetExecutionContext()->GetTaskRunner(
              TaskType::kUserInteraction)),
      result_handler_(result_handler),
      system_clipboard_(system_clipboard) {}

ClipboardReader::~ClipboardReader() = default;

void ClipboardReader::Trace(Visitor* visitor) const {
  visitor->Trace(system_clipboard_);
  visitor->Trace(result_handler_);
}

}  // namespace blink
