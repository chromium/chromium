// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_writer.h"

#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {  // anonymous namespace for ClipboardWriter's derived classes.

// Writes a blob with image/png content to the System Clipboard.
class ClipboardImageWriter final : public ClipboardWriter {
 public:
  ClipboardImageWriter(ClipboardPromise* promise) : ClipboardWriter(promise) {}
  ~ClipboardImageWriter() override = default;

 private:
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
        CrossThreadBindOnce(
            &ClipboardImageWriter::Write,
            /* This unretained is safe because the ClipboardImageWriter must
              remain alive when returning to its main thread. */
            CrossThreadUnretained(this), std::move(image)));
  }
  void Write(sk_sp<SkImage> image) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!image) {
      promise_->RejectFromReadOrDecodeFailure();
      return;
    }
    SkBitmap bitmap;
    image->asLegacyBitmap(&bitmap);
    SystemClipboard::GetInstance().WriteImage(std::move(bitmap));
    promise_->CompleteWriteRepresentation();
  }
};

// Writes a blob with text/plain content to the System Clipboard.
class ClipboardTextWriter final : public ClipboardWriter {
 public:
  ClipboardTextWriter(ClipboardPromise* promise) : ClipboardWriter(promise) {}
  ~ClipboardTextWriter() override = default;

 private:
  void DecodeOnBackgroundThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      DOMArrayBuffer* raw_data) override {
    DCHECK(!IsMainThread());

    String wtf_string =
        String::FromUTF8(reinterpret_cast<const LChar*>(raw_data->Data()),
                         raw_data->ByteLengthAsSizeT());
    DCHECK(wtf_string.IsSafeToSendToAnotherThread());
    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBindOnce(
            &ClipboardTextWriter::Write,
            /* This unretained is safe because the ClipboardTextWriter
              must remain alive when returning to its main thread. */
            CrossThreadUnretained(this), std::move(wtf_string)));
  }
  void Write(const String& text) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    SystemClipboard::GetInstance().WritePlainText(text);

    promise_->CompleteWriteRepresentation();
  }
};

}  // anonymous namespace

// ClipboardWriter functions.

// static
std::unique_ptr<ClipboardWriter> ClipboardWriter::Create(
    const String& mime_type,
    ClipboardPromise* promise) {
  if (mime_type == kMimeTypeImagePng)
    return std::make_unique<ClipboardImageWriter>(promise);
  if (mime_type == kMimeTypeTextPlain)
    return std::make_unique<ClipboardTextWriter>(promise);

  NOTREACHED() << "Type " << mime_type << " was not implemented";
  return nullptr;
}

ClipboardWriter::ClipboardWriter(ClipboardPromise* promise)
    : promise_(promise),
      clipboard_task_runner_(promise->GetExecutionContext()->GetTaskRunner(
          TaskType::kUserInteraction)),
      file_reading_task_runner_(promise->GetExecutionContext()->GetTaskRunner(
          TaskType::kFileReading)) {}

ClipboardWriter::~ClipboardWriter() = default;

// static
bool ClipboardWriter::IsValidType(const String& type) {
  // TODO(https://crbug.com/931839): Add support for text/html and other types.
  return type == kMimeTypeImagePng || type == kMimeTypeTextPlain;
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

  worker_pool::PostTask(
      FROM_HERE,
      CrossThreadBindOnce(&ClipboardWriter::DecodeOnBackgroundThread,
                          /* This unretained is safe because the ClipboardWriter
                             will wait for Decode to finish and return back to
                             this thread before deallocating. */
                          CrossThreadUnretained(this), clipboard_task_runner_,
                          WrapCrossThreadPersistent(array_buffer)));
}

void ClipboardWriter::DidFail(FileErrorCode error_code) {
  promise_->RejectFromReadOrDecodeFailure();
}

}  // namespace blink
