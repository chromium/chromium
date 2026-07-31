// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-encoders/image_encoder_utils.h"

#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

const ImageEncodingMimeType ImageEncoderUtils::kDefaultEncodingMimeType =
    kMimeTypePng;
const char ImageEncoderUtils::kDefaultRequestedMimeType[] = "image/png";

ImageEncodingMimeType ImageEncoderUtils::ToEncodingMimeType(
    const String& mime_type_name) {
  String lowercase_mime_type = mime_type_name.ToAsciiLower();
  if (mime_type_name.IsNull()) {
    lowercase_mime_type = kDefaultRequestedMimeType;
  }

  ImageEncodingMimeType encoding_mime_type = kDefaultEncodingMimeType;
  // FIXME: Make isSupportedImageMIMETypeForEncoding threadsafe (to allow this
  // method to be used on a worker thread).
  if (MIMETypeRegistry::IsSupportedImageMIMETypeForEncoding(
          lowercase_mime_type)) {
    ParseMimeType(lowercase_mime_type, encoding_mime_type);
  }
  return encoding_mime_type;
}

bool ImageEncoderUtils::ParseMimeType(const String& mime_type_name,
                                      ImageEncodingMimeType& mime_type) {
  if (mime_type_name == "image/png") {
    mime_type = kMimeTypePng;
  } else if (mime_type_name == "image/jpeg") {
    mime_type = kMimeTypeJpeg;
  } else if (mime_type_name == "image/webp") {
    mime_type = kMimeTypeWebp;
  } else {
    return false;
  }
  return true;
}

String ImageEncoderUtils::MimeTypeName(ImageEncodingMimeType mime_type) {
  DCHECK_GE(mime_type, 0);
  DCHECK_LT(mime_type, 3);
  constexpr std::array<const char* const, 3> kMimeTypeNames = {
      "image/png", "image/jpeg", "image/webp"};
  return kMimeTypeNames[mime_type];
}

}  // namespace blink
