/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/dom/dom_exception.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"

namespace blink {

namespace {

// Name, description, and legacy code name and value of DOMExceptions.
// https://webidl.spec.whatwg.org/#idl-DOMException-error-names
const struct DOMExceptionEntry {
  DOMExceptionCode code;
  const char* name;
  const char* message;
} kDOMExceptionEntryTable[] = {
    // DOMException defined with legacy error code in Web IDL.
    {DOMExceptionCode::kIndexSizeError, "IndexSizeError",
     "Index or size was negative, or greater than the allowed value."},
    {DOMExceptionCode::kHierarchyRequestError, "HierarchyRequestError",
     "A Node was inserted somewhere it doesn't belong."},
    {DOMExceptionCode::kWrongDocumentError, "WrongDocumentError",
     "A Node was used in a different document than the one that created it "
     "(that doesn't support it)."},
    {DOMExceptionCode::kInvalidCharacterError, "InvalidCharacterError",
     "The string contains invalid characters."},
    {DOMExceptionCode::kNoModificationAllowedError,
     "NoModificationAllowedError",
     "An attempt was made to modify an object where modifications are not "
     "allowed."},
    {DOMExceptionCode::kNotFoundError, "NotFoundError",
     "An attempt was made to reference a Node in a context where it does not "
     "exist."},
    {DOMExceptionCode::kNotSupportedError, "NotSupportedError",
     "The implementation did not support the requested type of object or "
     "operation."},
    {DOMExceptionCode::kInUseAttributeError, "InUseAttributeError",
     "An attempt was made to add an attribute that is already in use "
     "elsewhere."},
    {DOMExceptionCode::kInvalidStateError, "InvalidStateError",
     "An attempt was made to use an object that is not, or is no longer, "
     "usable."},
    {DOMExceptionCode::kSyntaxError, "SyntaxError",
     "An invalid or illegal string was specified."},
    {DOMExceptionCode::kInvalidModificationError, "InvalidModificationError",
     "The object can not be modified in this way."},
    {DOMExceptionCode::kNamespaceError, "NamespaceError",
     "An attempt was made to create or change an object in a way which is "
     "incorrect with regard to namespaces."},
    {DOMExceptionCode::kInvalidAccessError, "InvalidAccessError",
     "A parameter or an operation was not supported by the underlying object."},
    {DOMExceptionCode::kTypeMismatchError, "TypeMismatchError",
     "The type of an object was incompatible with the expected type of the "
     "parameter associated to the object."},
    {DOMExceptionCode::kSecurityError, "SecurityError",
     "An attempt was made to break through the security policy of the user "
     "agent."},
    {DOMExceptionCode::kNetworkError, "NetworkError",
     "A network error occurred."},
    {DOMExceptionCode::kAbortError, "AbortError",
     "The user aborted a request."},
    {DOMExceptionCode::kURLMismatchError, "URLMismatchError",
     "A worker global scope represented an absolute URL that is not equal to "
     "the resulting absolute URL."},
    {DOMExceptionCode::kQuotaExceededError, "QuotaExceededError",
     "An attempt was made to add something to storage that exceeded the "
     "quota."},
    {DOMExceptionCode::kTimeoutError, "TimeoutError", "A timeout occurred."},
    {DOMExceptionCode::kInvalidNodeTypeError, "InvalidNodeTypeError",
     "The supplied node is invalid or has an invalid ancestor for this "
     "operation."},
    {DOMExceptionCode::kDataCloneError, "DataCloneError",
     "An object could not be cloned."},

    // DOMException defined without legacy error code in Web IDL.
    {DOMExceptionCode::kEncodingError, "EncodingError",
     "A URI supplied to the API was malformed, or the resulting Data URL has "
     "exceeded the URL length limitations for Data URLs."},
    {DOMExceptionCode::kNotReadableError, "NotReadableError",
     "The requested file could not be read, typically due to permission "
     "problems that have occurred after a reference to a file was acquired."},
    {DOMExceptionCode::kUnknownError, "UnknownError",
     "The operation failed for an unknown transient reason "
     "(e.g. out of memory)."},
    {DOMExceptionCode::kConstraintError, "ConstraintError",
     "A mutation operation in the transaction failed because a constraint was "
     "not satisfied."},
    {DOMExceptionCode::kDataError, "DataError",
     "The data provided does not meet requirements."},
    {DOMExceptionCode::kTransactionInactiveError, "TransactionInactiveError",
     "A request was placed against a transaction which is either currently not "
     "active, or which is finished."},
    {DOMExceptionCode::kReadOnlyError, "ReadOnlyError",
     "A write operation was attempted in a read-only transaction."},
    {DOMExceptionCode::kVersionError, "VersionError",
     "An attempt was made to open a database using a lower version than the "
     "existing version."},
    {DOMExceptionCode::kOperationError, "OperationError",
     "The operation failed for an operation-specific reason"},
    {DOMExceptionCode::kNotAllowedError, "NotAllowedError",
     "The request is not allowed by the user agent or the platform in the "
     "current context."},
    {DOMExceptionCode::kOptOutError, "OptOutError",
     "The user opted out of the process."},

    // DOMError (obsolete, not DOMException) defined in File system (obsolete).
    // https://www.w3.org/TR/2012/WD-file-system-api-20120417/
    {DOMExceptionCode::kPathExistsError, "PathExistsError",
     "An attempt was made to create a file or directory where an element "
     "already exists."},

    // Push API
    //
    // PermissionDeniedError (obsolete) was replaced with NotAllowedError in the
    // standard.
    // https://github.com/WICG/BackgroundSync/issues/124
    {DOMExceptionCode::kPermissionDeniedError, "PermissionDeniedError",
     "User or security policy denied the request."},

    // Serial API - https://wicg.github.io/serial
    {DOMExceptionCode::kBreakError, "BreakError",
     "A break condition has been detected."},
    {DOMExceptionCode::kBufferOverrunError, "BufferOverrunError",
     "A buffer overrun has been detected."},
    {DOMExceptionCode::kFramingError, "FramingError",
     "A framing error has been detected."},
    {DOMExceptionCode::kParityError, "ParityError",
     "A parity error has been detected."},
    {DOMExceptionCode::kWebTransportError, "WebTransportError",
     "The WebTransport operation failed."},

    // Smart Card API
    // https://wicg.github.io/web-smart-card/#smartcarderror-interface
    {DOMExceptionCode::kSmartCardError, "SmartCardError",
     "A Smart Card operation failed."},

    // WebGPU https://www.w3.org/TR/webgpu/
    {DOMExceptionCode::kGPUPipelineError, "GPUPipelineError",
     "A WebGPU pipeline creation failed."},

    // Media Capture and Streams API
    // https://w3c.github.io/mediacapture-main/#overconstrainederror-interface
    {DOMExceptionCode::kOverconstrainedError, "OverconstrainedError",
     "The desired set of constraints/capabilities cannot be met."},

    // FedCM API
    // https://fedidcg.github.io/FedCM/#browser-api-identity-credential-error-interface
    {DOMExceptionCode::kIdentityCredentialError, "IdentityCredentialError",
     "An attempt to retrieve an IdentityCredential has failed."},

    // WebSocketStream API https://websockets.spec.whatwg.org/
    {DOMExceptionCode::kWebSocketError, "WebSocketError",
     "The WebSocket connection was closed."},

    // Extra comment to keep the end of the initializer list on its own line.
};

uint16_t ToLegacyErrorCode(DOMExceptionCode exception_code) {
  if (DOMExceptionCode::kLegacyErrorCodeMin <= exception_code &&
      exception_code <= DOMExceptionCode::kLegacyErrorCodeMax) {
    return static_cast<uint16_t>(exception_code);
  }
  return 0;
}

const DOMExceptionEntry* FindErrorEntry(DOMExceptionCode exception_code) {
  for (const auto& entry : kDOMExceptionEntryTable) {
    if (exception_code == entry.code)
      return &entry;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

uint16_t FindLegacyErrorCode(const String& name) {
  for (const auto& entry : kDOMExceptionEntryTable) {
    if (name == entry.name)
      return ToLegacyErrorCode(entry.code);
  }
  return 0;
}

}  // namespace

// static
DOMException* DOMException::Create(const String& message, const String& name) {
  return MakeGarbageCollected<DOMException>(FindLegacyErrorCode(name), name,
                                            message, String());
}

// static
String DOMException::GetErrorName(DOMExceptionCode exception_code) {
  const DOMExceptionEntry* entry = FindErrorEntry(exception_code);

  DCHECK(entry);
  if (!entry)
    return "UnknownError";

  return entry->name;
}

// static
String DOMException::GetErrorMessage(DOMExceptionCode exception_code) {
  const DOMExceptionEntry* entry = FindErrorEntry(exception_code);

  DCHECK(entry);
  if (!entry)
    return "Unknown error.";

  return entry->message;
}

DOMException::DOMException(DOMExceptionCode exception_code,
                           String sanitized_message,
                           String unsanitized_message) {
  // Don't delegate to another constructor to avoid calling FindErrorEntry()
  // multiple times.
  auto* error_entry = FindErrorEntry(exception_code);
  CHECK(error_entry);
  legacy_code_ = ToLegacyErrorCode(error_entry->code);
  name_ = error_entry->name;
  sanitized_message_ = sanitized_message.IsNull()
                           ? String(error_entry->message)
                           : std::move(sanitized_message);
  unsanitized_message_ = std::move(unsanitized_message);
}

DOMException::DOMException(DOMExceptionCode exception_code,
                           const char* sanitized_message,
                           const char* unsanitized_message)
    : DOMException(
          exception_code,
          sanitized_message ? String(sanitized_message) : String(),
          unsanitized_message ? String(unsanitized_message) : String()) {}

DOMException::DOMException(uint16_t legacy_code,
                           const String& name,
                           const String& sanitized_message,
                           const String& unsanitized_message)
    : legacy_code_(legacy_code),
      name_(name),
      sanitized_message_(sanitized_message),
      unsanitized_message_(unsanitized_message) {
  DCHECK(name);
}

String DOMException::ToStringForConsole() const {
  // If an unsanitized message is present, we prefer it.
  const String& message_for_console =
      !unsanitized_message_.empty() ? unsanitized_message_ : sanitized_message_;
  return message_for_console.empty()
             ? String()
             : "Uncaught " + name() + ": " + message_for_console;
}

void DOMException::AddContextToMessages(const ExceptionContext& context) {
  sanitized_message_ =
      ExceptionMessages::AddContextToMessage(context, sanitized_message_);
  if (!unsanitized_message_.IsNull()) {
    unsanitized_message_ =
        ExceptionMessages::AddContextToMessage(context, unsanitized_message_);
  }
}

}  // namespace blink
