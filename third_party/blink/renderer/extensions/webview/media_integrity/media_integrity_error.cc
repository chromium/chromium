// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/webview/media_integrity/media_integrity_error.h"

#include "third_party/blink/renderer/bindings/extensions_webview/v8/v8_media_integrity_error_options.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {
String GetErrorMessageForName(V8MediaIntegrityErrorName::Enum name) {
  switch (name) {
    case V8MediaIntegrityErrorName::Enum::kInternalError:
      return "Internal Error. Retry with an exponential backoff.";
    case V8MediaIntegrityErrorName::Enum::kNonRecoverableError:
      return "Non-recoverable error. Do not retry.";
    case V8MediaIntegrityErrorName::Enum::kAPIDisabledByApplication:
      return "API disabled by application.";
    case V8MediaIntegrityErrorName::Enum::kInvalidArgument:
      return "Invalid input argument.";
    case V8MediaIntegrityErrorName::Enum::kTokenProviderInvalid:
      return "Token provider invalid.";
  }
}
}  // namespace

// static
MediaIntegrityError* MediaIntegrityError::Create(
    String message,
    const MediaIntegrityErrorOptions* options) {
  return MakeGarbageCollected<MediaIntegrityError>(
      std::move(message), options->mediaIntegrityErrorName());
}

// static
MediaIntegrityError* MediaIntegrityError::CreateForName(
    V8MediaIntegrityErrorName::Enum name) {
  return MakeGarbageCollected<MediaIntegrityError>(
      GetErrorMessageForName(name), V8MediaIntegrityErrorName(name));
}

MediaIntegrityError::MediaIntegrityError(String message,
                                         V8MediaIntegrityErrorName name)
    : DOMException(DOMExceptionCode::kOperationError, message),
      media_integrity_error_name_(std::move(name)) {}

MediaIntegrityError::~MediaIntegrityError() = default;

}  // namespace blink
