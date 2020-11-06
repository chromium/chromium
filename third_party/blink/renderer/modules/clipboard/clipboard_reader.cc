// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/clipboard/clipboard_reader.h"

#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {  // anonymous namespace for ClipboardReader's derived classes.

// Reads an image from the System Clipboard as a Blob with image/png content.
class ClipboardImageReader final : public ClipboardReader {
 public:
  explicit ClipboardImageReader(SystemClipboard* system_clipboard,
                                ClipboardPromise* promise)
      : ClipboardReader(system_clipboard, promise) {}
  ~ClipboardImageReader() override = default;

  ClipboardImageReader(const ClipboardImageReader&) = delete;
  ClipboardImageReader& operator=(const ClipboardImageReader&) = delete;

  void Read() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    SkBitmap bitmap =
        system_clipboard()->ReadImage(mojom::ClipboardBuffer::kStandard);
    sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
    if (!image) {
      NextRead(Vector<uint8_t>());
      return;
    }

    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(&ClipboardImageReader::EncodeOnBackgroundThread,
                            std::move(image), WrapCrossThreadPersistent(this),
                            std::move(clipboard_task_runner_)));
  }

 private:
  static void EncodeOnBackgroundThread(
      sk_sp<SkImage> image,
      ClipboardImageReader* reader,
      scoped_refptr<base::SingleThreadTaskRunner> clipboard_task_runner) {
    DCHECK(!IsMainThread());

    SkPixmap pixmap;
    image->peekPixels(&pixmap);

    // Set encoding options to favor speed over size.
    SkPngEncoder::Options options;
    options.fZLibLevel = 1;
    options.fFilterFlags = SkPngEncoder::FilterFlag::kNone;

    Vector<uint8_t> png_data;
    if (!ImageEncoder::Encode(&png_data, pixmap, options))
      png_data.clear();

    // Now return to the kUserInteraction thread.
    PostCrossThreadTask(*clipboard_task_runner, FROM_HERE,
                        CrossThreadBindOnce(&ClipboardImageReader::NextRead,
                                            WrapCrossThreadPersistent(reader),
                                            std::move(png_data)));
  }

  // An empty vector indicates that the encoding step failed.
  void NextRead(Vector<uint8_t> png_data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Blob* blob = nullptr;
    if (png_data.size())
      blob = Blob::Create(png_data.data(), png_data.size(), kMimeTypeImagePng);

    promise_->OnRead(blob);
  }
};

// Reads an image from the System Clipboard as a Blob with text/plain content.
class ClipboardTextReader final : public ClipboardReader {
 public:
  explicit ClipboardTextReader(SystemClipboard* system_clipboard,
                               ClipboardPromise* promise)
      : ClipboardReader(system_clipboard, promise) {}
  ~ClipboardTextReader() override = default;

  ClipboardTextReader(const ClipboardTextReader&) = delete;
  ClipboardTextReader& operator=(const ClipboardTextReader&) = delete;

  void Read() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    String plain_text =
        system_clipboard()->ReadPlainText(mojom::ClipboardBuffer::kStandard);
    if (plain_text.IsEmpty()) {
      NextRead(Vector<uint8_t>());
      return;
    }

    worker_pool::PostTask(
        FROM_HERE, CrossThreadBindOnce(
                       &ClipboardTextReader::EncodeOnBackgroundThread,
                       std::move(plain_text), WrapCrossThreadPersistent(this),
                       std::move(clipboard_task_runner_)));
  }

 private:
  static void EncodeOnBackgroundThread(
      String plain_text,
      ClipboardTextReader* reader,
      scoped_refptr<base::SingleThreadTaskRunner> clipboard_task_runner) {
    DCHECK(!IsMainThread());

    // Encode WTF String to UTF-8, the standard text format for Blobs.
    StringUTF8Adaptor utf8_text(plain_text);
    Vector<uint8_t> utf8_bytes;
    utf8_bytes.ReserveInitialCapacity(utf8_text.size());
    utf8_bytes.Append(utf8_text.data(), utf8_text.size());

    PostCrossThreadTask(*clipboard_task_runner, FROM_HERE,
                        CrossThreadBindOnce(&ClipboardTextReader::NextRead,
                                            WrapCrossThreadPersistent(reader),
                                            std::move(utf8_bytes)));
  }

  void NextRead(Vector<uint8_t> utf8_bytes) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Blob* blob = nullptr;
    if (utf8_bytes.size()) {
      blob = Blob::Create(utf8_bytes.data(), utf8_bytes.size(),
                          kMimeTypeTextPlain);
    }
    promise_->OnRead(blob);
  }
};

// Reads HTML from the System Clipboard as a Blob with text/html content.
class ClipboardHtmlReader final : public ClipboardReader {
 public:
  explicit ClipboardHtmlReader(SystemClipboard* system_clipboard,
                               ClipboardPromise* promise)
      : ClipboardReader(system_clipboard, promise) {}
  ~ClipboardHtmlReader() override = default;

  // This must be called on the main thread because HTML DOM nodes can
  // only be used on the main thread.
  void Read() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    promise_->GetExecutionContext()->CountUse(
        WebFeature::kHtmlClipboardApiRead);

    KURL url;
    unsigned fragment_start = 0;
    unsigned fragment_end = 0;

    String html_string =
        system_clipboard()->ReadHTML(url, fragment_start, fragment_end);

    // Now sanitize the HTML string.
    LocalFrame* frame = promise_->GetLocalFrame();
    DocumentFragment* fragment = CreateSanitizedFragmentFromMarkupWithContext(
        *frame->GetDocument(), html_string, fragment_start,
        html_string.length(), url);
    String sanitized_html =
        CreateMarkup(fragment, kIncludeNode, kResolveAllURLs);

    if (sanitized_html.IsEmpty()) {
      NextRead(Vector<uint8_t>());
      return;
    }
    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(&ClipboardHtmlReader::EncodeOnBackgroundThread,
                            std::move(sanitized_html),
                            WrapCrossThreadPersistent(this),
                            std::move(clipboard_task_runner_)));
  }

 private:
  static void EncodeOnBackgroundThread(
      String plain_text,
      ClipboardHtmlReader* reader,
      scoped_refptr<base::SingleThreadTaskRunner> clipboard_task_runner) {
    DCHECK(!IsMainThread());

    // Encode WTF String to UTF-8, the standard text format for blobs.
    StringUTF8Adaptor utf8_text(plain_text);
    Vector<uint8_t> utf8_bytes;
    utf8_bytes.ReserveInitialCapacity(utf8_text.size());
    utf8_bytes.Append(utf8_text.data(), utf8_text.size());

    PostCrossThreadTask(*clipboard_task_runner, FROM_HERE,
                        CrossThreadBindOnce(&ClipboardHtmlReader::NextRead,
                                            WrapCrossThreadPersistent(reader),
                                            std::move(utf8_bytes)));
  }

  void NextRead(Vector<uint8_t> utf8_bytes) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Blob* blob = nullptr;
    if (utf8_bytes.size()) {
      blob =
          Blob::Create(utf8_bytes.data(), utf8_bytes.size(), kMimeTypeTextHTML);
    }
    promise_->OnRead(blob);
  }
};

// Reads SVG from the System Clipboard as a Blob with image/svg content.
class ClipboardSvgReader final : public ClipboardReader {
 public:
  ClipboardSvgReader(SystemClipboard* system_clipboard,
                              ClipboardPromise* promise)
      : ClipboardReader(system_clipboard, promise) {}
  ~ClipboardSvgReader() override = default;

  // This must be called on the main thread because XML DOM nodes can
  // only be used on the main thread.
  void Read() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    system_clipboard()->ReadSvg(
        WTF::Bind(&ClipboardSvgReader::OnRead, WrapPersistent(this)));
  }

 private:
  void OnRead(const String& svg_string) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    LocalFrame* frame = promise_->GetLocalFrame();
    if (!frame) {
      NextRead(Vector<uint8_t>());
      return;
    }

    // Now sanitize the SVG string.
    KURL url;
    unsigned fragment_start = 0;
    DocumentFragment* fragment = CreateSanitizedFragmentFromMarkupWithContext(
        *frame->GetDocument(), svg_string, fragment_start, svg_string.length(),
        url);
    String sanitized_svg =
        CreateMarkup(fragment, kIncludeNode, kResolveAllURLs);

    if (sanitized_svg.IsEmpty()) {
      NextRead(Vector<uint8_t>());
      return;
    }
    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(&ClipboardSvgReader::EncodeOnBackgroundThread,
                            std::move(sanitized_svg),
                            WrapCrossThreadPersistent(this),
                            std::move(clipboard_task_runner_)));
  }

  static void EncodeOnBackgroundThread(
      String plain_text,
      ClipboardSvgReader* reader,
      scoped_refptr<base::SingleThreadTaskRunner> clipboard_task_runner) {
    DCHECK(!IsMainThread());

    // Encode WTF String to UTF-8, the standard text format for Blobs.
    StringUTF8Adaptor utf8_text(plain_text);
    Vector<uint8_t> utf8_bytes;
    utf8_bytes.ReserveInitialCapacity(utf8_text.size());
    utf8_bytes.Append(utf8_text.data(), utf8_text.size());

    PostCrossThreadTask(*clipboard_task_runner, FROM_HERE,
                        CrossThreadBindOnce(&ClipboardSvgReader::NextRead,
                                            WrapCrossThreadPersistent(reader),
                                            std::move(utf8_bytes)));
  }

  void NextRead(Vector<uint8_t> utf8_bytes) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Blob* blob = nullptr;
    if (utf8_bytes.size()) {
      blob =
          Blob::Create(utf8_bytes.data(), utf8_bytes.size(), kMimeTypeImageSvg);
    }
    promise_->OnRead(blob);
  }
};
}  // anonymous namespace
// ClipboardReader functions.

ClipboardReader* ClipboardReader::Create(SystemClipboard* system_clipboard,
                                         const String& mime_type,
                                         ClipboardPromise* promise) {
  if (mime_type == kMimeTypeImagePng)
    return MakeGarbageCollected<ClipboardImageReader>(system_clipboard,
                                                      promise);
  if (mime_type == kMimeTypeTextPlain)
    return MakeGarbageCollected<ClipboardTextReader>(system_clipboard, promise);

  if (mime_type == kMimeTypeTextHTML)
    return MakeGarbageCollected<ClipboardHtmlReader>(system_clipboard, promise);

  if (mime_type == kMimeTypeImageSvg &&
      RuntimeEnabledFeatures::ClipboardSvgEnabled())
    return MakeGarbageCollected<ClipboardSvgReader>(system_clipboard, promise);
  // The MIME type is not supported.
  return nullptr;
}

ClipboardReader::ClipboardReader(SystemClipboard* system_clipboard,
                                 ClipboardPromise* promise)
    : clipboard_task_runner_(promise->GetExecutionContext()->GetTaskRunner(
          TaskType::kUserInteraction)),
      promise_(promise),
      system_clipboard_(system_clipboard) {}

ClipboardReader::~ClipboardReader() = default;

void ClipboardReader::Trace(Visitor* visitor) const {
  visitor->Trace(system_clipboard_);
  visitor->Trace(promise_);
}

}  // namespace blink
