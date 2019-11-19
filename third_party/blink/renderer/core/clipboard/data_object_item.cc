/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/clipboard/data_object_item.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"

namespace blink {

DataObjectItem* DataObjectItem::CreateFromString(const String& type,
                                                 const String& data) {
  DataObjectItem* item =
      MakeGarbageCollected<DataObjectItem>(kStringKind, type);
  item->data_ = data;
  return item;
}

DataObjectItem* DataObjectItem::CreateFromFile(File* file) {
  DataObjectItem* item =
      MakeGarbageCollected<DataObjectItem>(kFileKind, file->type());
  item->file_ = file;
  return item;
}

DataObjectItem* DataObjectItem::CreateFromFileWithFileSystemId(
    File* file,
    const String& file_system_id) {
  DataObjectItem* item =
      MakeGarbageCollected<DataObjectItem>(kFileKind, file->type());
  item->file_ = file;
  item->file_system_id_ = file_system_id;
  return item;
}

DataObjectItem* DataObjectItem::CreateFromURL(const String& url,
                                              const String& title) {
  DataObjectItem* item =
      MakeGarbageCollected<DataObjectItem>(kStringKind, kMimeTypeTextURIList);
  item->data_ = url;
  item->title_ = title;
  return item;
}

DataObjectItem* DataObjectItem::CreateFromHTML(const String& html,
                                               const KURL& base_url) {
  DataObjectItem* item =
      MakeGarbageCollected<DataObjectItem>(kStringKind, kMimeTypeTextHTML);
  item->data_ = html;
  item->base_url_ = base_url;
  return item;
}

DataObjectItem* DataObjectItem::CreateFromSharedBuffer(
    scoped_refptr<SharedBuffer> buffer,
    const KURL& source_url,
    const String& filename_extension,
    const AtomicString& content_disposition) {
  DataObjectItem* item = MakeGarbageCollected<DataObjectItem>(
      kFileKind,
      MIMETypeRegistry::GetWellKnownMIMETypeForExtension(filename_extension));
  item->shared_buffer_ = std::move(buffer);
  item->filename_extension_ = filename_extension;
  // TODO(dcheng): Rename these fields to be more generically named.
  item->title_ = content_disposition;
  item->base_url_ = source_url;
  return item;
}

DataObjectItem* DataObjectItem::CreateFromClipboard(const String& type,
                                                    uint64_t sequence_number) {
  if (type == kMimeTypeImagePng) {
    return MakeGarbageCollected<DataObjectItem>(kFileKind, type,
                                                sequence_number);
  }
  return MakeGarbageCollected<DataObjectItem>(kStringKind, type,
                                              sequence_number);
}

DataObjectItem::DataObjectItem(ItemKind kind, const String& type)
    : source_(kInternalSource), kind_(kind), type_(type), sequence_number_(0) {}

DataObjectItem::DataObjectItem(ItemKind kind,
                               const String& type,
                               uint64_t sequence_number)
    : source_(kClipboardSource),
      kind_(kind),
      type_(type),
      sequence_number_(sequence_number) {}

File* DataObjectItem::GetAsFile() const {
  if (Kind() != kFileKind)
    return nullptr;

  if (source_ == kInternalSource) {
    if (file_)
      return file_.Get();
    DCHECK(shared_buffer_);
    // TODO: This code is currently impossible--we never populate
    // |shared_buffer_| when dragging in. At some point though, we may need to
    // support correctly converting a shared buffer into a file.
    return nullptr;
  }

  DCHECK_EQ(source_, kClipboardSource);
  if (GetType() == kMimeTypeImagePng) {
    SkBitmap bitmap = SystemClipboard::GetInstance().ReadImage(
        mojom::ClipboardBuffer::kStandard);

    SkPixmap pixmap;
    bitmap.peekPixels(&pixmap);

    // Set encoding options to favor speed over size.
    SkPngEncoder::Options options;
    options.fZLibLevel = 1;
    options.fFilterFlags = SkPngEncoder::FilterFlag::kNone;

    Vector<uint8_t> png_data;
    if (!ImageEncoder::Encode(&png_data, pixmap, options))
      return nullptr;

    auto data = std::make_unique<BlobData>();
    data->SetContentType(kMimeTypeImagePng);
    data->AppendBytes(png_data.data(), png_data.size());
    const uint64_t length = data->length();
    auto blob = BlobDataHandle::Create(std::move(data), length);
    return File::Create("image.png", base::Time::Now().ToDoubleT() * 1000.0,
                        std::move(blob));
  }

  return nullptr;
}

String DataObjectItem::GetAsString() const {
  DCHECK_EQ(kind_, kStringKind);

  if (source_ == kInternalSource)
    return data_;

  DCHECK_EQ(source_, kClipboardSource);

  String data;
  // This is ugly but there's no real alternative.
  if (type_ == kMimeTypeTextPlain) {
    data = SystemClipboard::GetInstance().ReadPlainText();
  } else if (type_ == kMimeTypeTextRTF) {
    data = SystemClipboard::GetInstance().ReadRTF();
  } else if (type_ == kMimeTypeTextHTML) {
    KURL ignored_source_url;
    unsigned ignored;
    data = SystemClipboard::GetInstance().ReadHTML(ignored_source_url, ignored,
                                                   ignored);
  } else {
    data = SystemClipboard::GetInstance().ReadCustomData(type_);
  }

  return SystemClipboard::GetInstance().SequenceNumber() == sequence_number_
             ? data
             : String();
}

bool DataObjectItem::IsFilename() const {
  // TODO(https://bugs.webkit.org/show_bug.cgi?id=81261): When we properly
  // support File dragout, we'll need to make sure this works as expected for
  // DragDataChromium.
  return kind_ == kFileKind && file_;
}

bool DataObjectItem::HasFileSystemId() const {
  return kind_ == kFileKind && !file_system_id_.IsEmpty();
}

String DataObjectItem::FileSystemId() const {
  return file_system_id_;
}

void DataObjectItem::Trace(blink::Visitor* visitor) {
  visitor->Trace(file_);
}

}  // namespace blink
