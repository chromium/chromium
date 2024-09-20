// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/webview/media_integrity/media_integrity_error.h"

#include "third_party/blink/public/mojom/webview/webview_media_integrity.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
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
  NOTREACHED();
}

V8MediaIntegrityErrorName::Enum MojomToV8Enum(
    mojom::blink::WebViewMediaIntegrityErrorCode error) {
  switch (error) {
    case mojom::blink::WebViewMediaIntegrityErrorCode::kInternalError:
      return V8MediaIntegrityErrorName::Enum::kInternalError;
    case mojom::blink::WebViewMediaIntegrityErrorCode::kNonRecoverableError:
      return V8MediaIntegrityErrorName::Enum::kNonRecoverableError;
    case mojom::blink::WebViewMediaIntegrityErrorCode::
        kApiDisabledByApplication:
      return V8MediaIntegrityErrorName::Enum::kAPIDisabledByApplication;
    case mojom::blink::WebViewMediaIntegrityErrorCode::kInvalidArgument:
      return V8MediaIntegrityErrorName::Enum::kInvalidArgument;
    case mojom::blink::WebViewMediaIntegrityErrorCode::kTokenProviderInvalid:
      return V8MediaIntegrityErrorName::Enum::kTokenProviderInvalid;
  }
  NOTREACHED();
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
v8::Local<v8::Value> MediaIntegrityError::CreateForName(
    v8::Isolate* isolate,
    V8MediaIntegrityErrorName::Enum name) {
  MediaIntegrityError* error = MakeGarbageCollected<MediaIntegrityError>(
      GetErrorMessageForName(name), V8MediaIntegrityErrorName(name));
  return blink::V8ThrowDOMException::AttachStackProperty(isolate, error);
}

// static
v8::Local<v8::Value> MediaIntegrityError::CreateFromMojomEnum(
    v8::Isolate* isolate,
    mojom::blink::WebViewMediaIntegrityErrorCode error_code) {
  V8MediaIntegrityErrorName::Enum name = MojomToV8Enum(error_code);
  MediaIntegrityError* error = MakeGarbageCollected<MediaIntegrityError>(
      GetErrorMessageForName(name), V8MediaIntegrityErrorName(name));
  return blink::V8ThrowDOMException::AttachStackProperty(isolate, error);
}

MediaIntegrityError::MediaIntegrityError(String message,
                                         V8MediaIntegrityErrorName name)
    : DOMException(DOMExceptionCode::kOperationError, message),
      media_integrity_error_name_(std::move(name)) {}

MediaIntegrityError::~MediaIntegrityError() = default;

}  // namespace blink
