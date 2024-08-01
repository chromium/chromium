// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_error.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// ExtensionError

ExtensionError::ExtensionError(Type type,
                               const ExtensionId& extension_id,
                               bool from_incognito,
                               logging::LogSeverity level,
                               const std::u16string& source,
                               const std::u16string& message)
    : type_(type),
      extension_id_(extension_id),
      id_(0),
      from_incognito_(from_incognito),
      level_(level),
      source_(source),
      message_(message),
      occurrences_(1u) {}

ExtensionError::~ExtensionError() {
}

std::string ExtensionError::GetDebugString() const {
  return std::string("Extension Error:") +
         "\n  OTR:     " + std::string(from_incognito_ ? "true" : "false") +
         "\n  Level:   " + base::NumberToString(level_) +
         "\n  Source:  " + base::UTF16ToUTF8(source_) +
         "\n  Message: " + base::UTF16ToUTF8(message_) +
         "\n  ID:      " + extension_id_;
}

bool ExtensionError::IsEqual(const ExtensionError* rhs) const {
  // We don't check |source_| or |level_| here, since they are constant for
  // manifest errors. Check them in RuntimeError::IsEqualImpl() instead.
  return type_ == rhs->type_ &&
         extension_id_ == rhs->extension_id_ &&
         message_ == rhs->message_ &&
         IsEqualImpl(rhs);
}

////////////////////////////////////////////////////////////////////////////////
// ManifestError

ManifestError::ManifestError(const ExtensionId& extension_id,
                             const std::u16string& message,
                             const std::string& manifest_key,
                             const std::u16string& manifest_specific)
    : ExtensionError(
          ExtensionError::Type::kManifestError,
          extension_id,
          false,  // extensions can't be installed while incognito.
          logging::LOGGING_WARNING,  // All manifest errors are warnings.
          base::FilePath(kManifestFilename).AsUTF16Unsafe(),
          message),
      manifest_key_(manifest_key),
      manifest_specific_(manifest_specific) {}

ManifestError::~ManifestError() {
}

std::string ManifestError::GetDebugString() const {
  return ExtensionError::GetDebugString() +
         "\n  Type:    ManifestError";
}

bool ManifestError::IsEqualImpl(const ExtensionError* rhs) const {
  // If two manifest errors have the same extension id and message (which are
  // both checked in ExtensionError::IsEqual), then they are equal.
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// RuntimeError

RuntimeError::RuntimeError(const ExtensionId& extension_id,
                           bool from_incognito,
                           const std::u16string& source,
                           const std::u16string& message,
                           const StackTrace& stack_trace,
                           const GURL& context_url,
                           logging::LogSeverity level,
                           int render_frame_id,
                           int render_process_id)
    : ExtensionError(ExtensionError::Type::kRuntimeError,
                     !extension_id.empty() ? extension_id : GURL(source).host(),
                     from_incognito,
                     level,
                     source,
                     message),
      context_url_(context_url),
      stack_trace_(stack_trace),
      render_frame_id_(render_frame_id),
      render_process_id_(render_process_id) {
  CleanUpInit();
}

RuntimeError::~RuntimeError() {
}

std::string RuntimeError::GetDebugString() const {
  std::string result = ExtensionError::GetDebugString() +
         "\n  Type:    RuntimeError"
         "\n  Context: " + context_url_.spec() +
         "\n  Stack Trace: ";
  for (auto iter = stack_trace_.cbegin(); iter != stack_trace_.cend(); ++iter) {
    result += "\n    {";
    result += "\n      Line:     " + base::NumberToString(iter->line_number) +
              "\n      Column:   " + base::NumberToString(iter->column_number) +
              "\n      URL:      " + base::UTF16ToUTF8(iter->source) +
              "\n      Function: " + base::UTF16ToUTF8(iter->function) +
              "\n    }";
  }
  return result;
}

bool RuntimeError::IsEqualImpl(const ExtensionError* rhs) const {
  const RuntimeError* error = static_cast<const RuntimeError*>(rhs);

  // Only look at the first frame of a stack trace to save time and group
  // nearly-identical errors. The most recent error is kept, so there's no risk
  // of displaying an old and inaccurate stack trace.
  return level_ == error->level_ &&
         source_ == error->source_ &&
         context_url_ == error->context_url_ &&
         stack_trace_.size() == error->stack_trace_.size() &&
         (stack_trace_.empty() || stack_trace_[0] == error->stack_trace_[0]);
}

void RuntimeError::CleanUpInit() {
  // If the error came from a generated background page, the "context" is empty
  // because there's no visible URL. We should set context to be the generated
  // background page in this case.
  GURL source_url = GURL(source_);
  if (context_url_.is_empty() &&
      source_url.path_piece() ==
          std::string("/") + kGeneratedBackgroundPageFilename) {
    context_url_ = source_url;
  }

  // In some instances (due to the fact that we're reusing error reporting from
  // other systems), the source won't match up with the final entry in the stack
  // trace. (For instance, in a browser action error, the source is the page -
  // sometimes the background page - but the error is thrown from the script.)
  // Make the source match the stack trace, since that is more likely the cause
  // of the error.
  if (!stack_trace_.empty() && source_ != stack_trace_[0].source) {
    source_ = stack_trace_[0].source;
  }
}

////////////////////////////////////////////////////////////////////////////////
// InternalError

InternalError::InternalError(const ExtensionId& extension_id,
                             const std::u16string& message,
                             logging::LogSeverity level)
    : ExtensionError(ExtensionError::Type::kInternalError,
                     extension_id,
                     false,  // not incognito.
                     level,
                     std::u16string(),
                     message) {}

InternalError::~InternalError() {
}

std::string InternalError::GetDebugString() const {
  return ExtensionError::GetDebugString() +
         "\n  Type:    InternalError";
}

bool InternalError::IsEqualImpl(const ExtensionError* rhs) const {
  // ExtensionError logic is sufficient for comparison.
  return true;
}

}  // namespace extensions
