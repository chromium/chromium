// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_reader.h"

#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {  // anonymous namespace for ClipboardReader's derived classes.

// Reads an image from the System Clipboard as a blob with image/png content.
class ClipboardImageReader final : public ClipboardReader {
 public:
  ClipboardImageReader() = default;
  ~ClipboardImageReader() override = default;

  Blob* ReadFromSystem() override {
    SkBitmap bitmap = SystemClipboard::GetInstance().ReadImage(
        mojom::ClipboardBuffer::kStandard);

    // Encode bitmap to Vector<uint8_t> on the main thread.
    SkPixmap pixmap;
    bitmap.peekPixels(&pixmap);

    // Set encoding options to favor speed over size.
    SkPngEncoder::Options options;
    options.fZLibLevel = 1;
    options.fFilterFlags = SkPngEncoder::FilterFlag::kNone;

    Vector<uint8_t> png_data;
    if (!ImageEncoder::Encode(&png_data, pixmap, options))
      return nullptr;

    return Blob::Create(png_data.data(), png_data.size(), kMimeTypeImagePng);
  }
};

// Reads an image from the System Clipboard as a blob with text/plain content.
class ClipboardTextReader final : public ClipboardReader {
 public:
  ClipboardTextReader() = default;
  ~ClipboardTextReader() override = default;

  Blob* ReadFromSystem() override {
    String plain_text = SystemClipboard::GetInstance().ReadPlainText(
        mojom::ClipboardBuffer::kStandard);

    // Encode WTF String to UTF-8, the standard text format for blobs.
    StringUTF8Adaptor utf_text(plain_text);
    return Blob::Create(reinterpret_cast<const uint8_t*>(utf_text.data()),
                        utf_text.size(), kMimeTypeTextPlain);
  }
};

}  // anonymous namespace

// ClipboardReader functions.

// static
std::unique_ptr<ClipboardReader> ClipboardReader::Create(
    const String& mime_type) {
  if (mime_type == kMimeTypeImagePng)
    return std::make_unique<ClipboardImageReader>();
  if (mime_type == kMimeTypeTextPlain)
    return std::make_unique<ClipboardTextReader>();

  // The MIME type is not supported.
  return nullptr;
}

ClipboardReader::ClipboardReader() = default;
ClipboardReader::~ClipboardReader() = default;

}  // namespace blink
