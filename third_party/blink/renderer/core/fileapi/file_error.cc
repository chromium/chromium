/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/fileapi/file_error.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace file_error {

const char kAbortErrorMessage[] =
    "An ongoing operation was aborted, typically with a call to abort().";
const char kEncodingErrorMessage[] =
    "A URI supplied to the API was malformed, or the resulting Data URL has "
    "exceeded the URL length limitations for Data URLs.";
const char kInvalidStateErrorMessage[] =
    "An operation that depends on state cached in an interface object was made "
    "but the state had changed since it was read from disk.";
const char kNoModificationAllowedErrorMessage[] =
    "An attempt was made to write to a file or directory which could not be "
    "modified due to the state of the underlying filesystem.";
const char kNotFoundErrorMessage[] =
    "A requested file or directory could not be found at the time an operation "
    "was processed.";
const char kNotReadableErrorMessage[] =
    "The requested file could not be read, typically due to permission "
    "problems that have occurred after a reference to a file was acquired.";
const char kPathExistsErrorMessage[] =
    "An attempt was made to create a file or directory where an element "
    "already exists.";
const char kQuotaExceededErrorMessage[] =
    "The operation failed because it would cause the application to exceed its "
    "storage quota.";
const char kSecurityErrorMessage[] =
    "It was determined that certain files are unsafe for access within a Web "
    "application, or that too many calls are being made on file resources.";
const char kSyntaxErrorMessage[] =
    "An invalid or unsupported argument was given, like an invalid line ending "
    "specifier.";
const char kTypeMismatchErrorMessage[] =
    "The path supplied exists, but was not an entry of requested type.";

namespace {

DOMExceptionCode ErrorCodeToExceptionCode(FileErrorCode code) {
  switch (code) {
    case FileErrorCode::kOK:
      return DOMExceptionCode::kNoError;
    case FileErrorCode::kNotFoundErr:
      return DOMExceptionCode::kNotFoundError;
    case FileErrorCode::kSecurityErr:
      return DOMExceptionCode::kSecurityError;
    case FileErrorCode::kAbortErr:
      return DOMExceptionCode::kAbortError;
    case FileErrorCode::kNotReadableErr:
      return DOMExceptionCode::kNotReadableError;
    case FileErrorCode::kEncodingErr:
      return DOMExceptionCode::kEncodingError;
    case FileErrorCode::kNoModificationAllowedErr:
      return DOMExceptionCode::kNoModificationAllowedError;
    case FileErrorCode::kInvalidStateErr:
      return DOMExceptionCode::kInvalidStateError;
    case FileErrorCode::kSyntaxErr:
      return DOMExceptionCode::kSyntaxError;
    case FileErrorCode::kInvalidModificationErr:
      return DOMExceptionCode::kInvalidModificationError;
    case FileErrorCode::kQuotaExceededErr:
      return DOMExceptionCode::kQuotaExceededError;
    case FileErrorCode::kTypeMismatchErr:
      return DOMExceptionCode::kTypeMismatchError;
    case FileErrorCode::kPathExistsErr:
      return DOMExceptionCode::kPathExistsError;
    default:
      NOTREACHED();
      return DOMExceptionCode::kUnknownError;
  }
}

const char* ErrorCodeToMessage(FileErrorCode code) {
  // Note that some of these do not set message. If message is 0 then the
  // default message is used.
  switch (code) {
    case FileErrorCode::kOK:
      return nullptr;
    case FileErrorCode::kSecurityErr:
      return kSecurityErrorMessage;
    case FileErrorCode::kNotFoundErr:
      return kNotFoundErrorMessage;
    case FileErrorCode::kAbortErr:
      return kAbortErrorMessage;
    case FileErrorCode::kNotReadableErr:
      return kNotReadableErrorMessage;
    case FileErrorCode::kEncodingErr:
      return kEncodingErrorMessage;
    case FileErrorCode::kNoModificationAllowedErr:
      return kNoModificationAllowedErrorMessage;
    case FileErrorCode::kInvalidStateErr:
      return kInvalidStateErrorMessage;
    case FileErrorCode::kSyntaxErr:
      return kSyntaxErrorMessage;
    case FileErrorCode::kInvalidModificationErr:
      return nullptr;
    case FileErrorCode::kQuotaExceededErr:
      return kQuotaExceededErrorMessage;
    case FileErrorCode::kTypeMismatchErr:
      return nullptr;
    case FileErrorCode::kPathExistsErr:
      return kPathExistsErrorMessage;
    default:
      NOTREACHED();
      return nullptr;
  }
}

DOMExceptionCode FileErrorToExceptionCode(base::File::Error code) {
  switch (code) {
    case base::File::FILE_OK:
      return DOMExceptionCode::kNoError;
    case base::File::FILE_ERROR_FAILED:
      return DOMExceptionCode::kInvalidStateError;
    // TODO(https://crbug.com/883062): base::File::FILE_ERROR_EXISTS should map
    // to kPathExistsError, but that currently breaks tests. Fix the test
    // expectations and make the change.
    case base::File::FILE_ERROR_EXISTS:
    case base::File::FILE_ERROR_NOT_EMPTY:
    case base::File::FILE_ERROR_INVALID_OPERATION:
      return DOMExceptionCode::kInvalidModificationError;
    case base::File::FILE_ERROR_TOO_MANY_OPENED:
    case base::File::FILE_ERROR_NO_MEMORY:
      return DOMExceptionCode::kUnknownError;
    case base::File::FILE_ERROR_NOT_FOUND:
      return DOMExceptionCode::kNotFoundError;
    case base::File::FILE_ERROR_IN_USE:
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return DOMExceptionCode::kNoModificationAllowedError;
    case base::File::FILE_ERROR_NO_SPACE:
      return DOMExceptionCode::kQuotaExceededError;
    case base::File::FILE_ERROR_NOT_A_DIRECTORY:
    case base::File::FILE_ERROR_NOT_A_FILE:
      return DOMExceptionCode::kTypeMismatchError;
    case base::File::FILE_ERROR_ABORT:
      return DOMExceptionCode::kAbortError;
    case base::File::FILE_ERROR_SECURITY:
      return DOMExceptionCode::kSecurityError;
    case base::File::FILE_ERROR_INVALID_URL:
      return DOMExceptionCode::kEncodingError;
    case base::File::FILE_ERROR_IO:
      return DOMExceptionCode::kNotReadableError;
    case base::File::FILE_ERROR_MAX:
      NOTREACHED();
      return DOMExceptionCode::kUnknownError;
  }
  NOTREACHED();
  return DOMExceptionCode::kUnknownError;
}

const char* FileErrorToMessage(base::File::Error code) {
  // Note that some of these do not set message. If message is null then the
  // default message is used.
  switch (code) {
    case base::File::FILE_ERROR_NOT_FOUND:
      return kNotFoundErrorMessage;
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return kNoModificationAllowedErrorMessage;
    case base::File::FILE_ERROR_FAILED:
      return kInvalidStateErrorMessage;
    case base::File::FILE_ERROR_ABORT:
      return kAbortErrorMessage;
    case base::File::FILE_ERROR_SECURITY:
      return kSecurityErrorMessage;
    case base::File::FILE_ERROR_NO_SPACE:
      return kQuotaExceededErrorMessage;
    case base::File::FILE_ERROR_INVALID_URL:
      return kEncodingErrorMessage;
    case base::File::FILE_ERROR_IO:
      return kNotReadableErrorMessage;
    case base::File::FILE_ERROR_EXISTS:
      return kPathExistsErrorMessage;
    case base::File::FILE_ERROR_NOT_A_DIRECTORY:
    case base::File::FILE_ERROR_NOT_A_FILE:
      return kTypeMismatchErrorMessage;
    case base::File::FILE_OK:
    case base::File::FILE_ERROR_INVALID_OPERATION:
    case base::File::FILE_ERROR_NOT_EMPTY:
    case base::File::FILE_ERROR_NO_MEMORY:
    case base::File::FILE_ERROR_TOO_MANY_OPENED:
    case base::File::FILE_ERROR_IN_USE:
      // TODO(mek): More specific error messages for at least some of these
      // errors.
      return nullptr;
    case base::File::FILE_ERROR_MAX:
      NOTREACHED();
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

void ThrowDOMException(ExceptionState& exception_state,
                       FileErrorCode code,
                       String message) {
  if (code == FileErrorCode::kOK)
    return;

  // SecurityError is special-cased, as we want to route those exceptions
  // through ExceptionState::ThrowSecurityError.
  if (code == FileErrorCode::kSecurityErr) {
    exception_state.ThrowSecurityError(kSecurityErrorMessage);
    return;
  }

  if (message.IsNull()) {
    message = ErrorCodeToMessage(code);
  }

  exception_state.ThrowDOMException(ErrorCodeToExceptionCode(code), message);
}

void ThrowDOMException(ExceptionState& exception_state,
                       base::File::Error error,
                       String message) {
  if (error == base::File::FILE_OK)
    return;

  // SecurityError is special-cased, as we want to route those exceptions
  // through ExceptionState::ThrowSecurityError.
  if (error == base::File::FILE_ERROR_SECURITY) {
    exception_state.ThrowSecurityError(kSecurityErrorMessage);
    return;
  }

  if (message.IsNull()) {
    message = FileErrorToMessage(error);
  }

  exception_state.ThrowDOMException(FileErrorToExceptionCode(error), message);
}

DOMException* CreateDOMException(FileErrorCode code) {
  DCHECK_NE(code, FileErrorCode::kOK);
  return MakeGarbageCollected<DOMException>(ErrorCodeToExceptionCode(code),
                                            ErrorCodeToMessage(code));
}

DOMException* CreateDOMException(base::File::Error code) {
  DCHECK_NE(code, base::File::FILE_OK);
  return MakeGarbageCollected<DOMException>(FileErrorToExceptionCode(code),
                                            FileErrorToMessage(code));
}

}  // namespace file_error

}  // namespace blink
