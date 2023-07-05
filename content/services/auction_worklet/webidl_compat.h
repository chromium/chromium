// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_WEBIDL_COMPAT_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_WEBIDL_COMPAT_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-value.h"

namespace v8 {
class TryCatch;
}  // namespace v8

namespace auction_worklet {

class CONTENT_EXPORT DictConverter {
 public:
  // Prepares to convert `value` to a WebIDL dictionary. You should follow this
  // by calling appropriate Get... methods for all the fields in lexicographic
  // order. Unlike gin, this does type conversions.
  //
  // All the Get... methods return true on success, false on failure.
  // In particular, if an optional field is missing, the out-param is set to
  // nullopt, and true is returned.
  //
  // In case of failure all further Get... ops will fail, and the state of
  // out-param is unpredictable. You can use ErrorMessage() to get the
  // description of the first detected error.
  //
  // Output types <T> correspond to the following WebIDL types:
  //  double to double.
  //  bool to boolean.
  //  std::string to DOMString.
  //  v8::Local<v8::Value> to any.
  //
  // Since these conversions may loop infinitely, a `time_limit_scope` must
  // exist during its operation, and one with non-null time limit.
  //
  // Expects `v8_helper` and `time_limit_scope` will outlive `this`.
  // `error_prefix` will be prepended to error messages produced by
  // DictConverter itself (but not for those from exceptions thrown by user
  // code invoked during conversion, since that already has line information).
  DictConverter(AuctionV8Helper* v8_helper,
                AuctionV8Helper::TimeLimitScope& time_limit_scope,
                std::string error_prefix,
                v8::Local<v8::Value> value);

  template <typename T>
  bool GetRequired(base::StringPiece field, T& out) {
    if (failed_) {
      return false;
    }

    v8::Local<v8::Value> val = GetMember(field);
    if (failed_) {
      return false;
    }

    if (val->IsUndefined()) {
      MarkFailed(base::StrCat({"Required field '", field, "' missing."}));
      return false;
    }

    return Convert(field, val, out);
  }

  template <typename T>
  bool GetOptional(base::StringPiece field, absl::optional<T>& out) {
    if (failed_) {
      return false;
    }

    v8::Local<v8::Value> val = GetMember(field);
    if (failed_) {
      return false;
    }

    if (val->IsUndefined()) {
      out = absl::nullopt;
      return true;
    }

    out.emplace();
    return Convert(field, val, out.value());
  }

  const std::string& ErrorMessage() { return failure_message_; }

  // TODO(morlovich): Accessor for the exception object.

 private:
  // This can mark failure.
  v8::Local<v8::Value> GetMember(base::StringPiece field);

  // These mark failure as it happens.
  bool Convert(base::StringPiece field,
               v8::Local<v8::Value> value,
               double& out);
  bool Convert(base::StringPiece field, v8::Local<v8::Value> value, bool& out);
  bool Convert(base::StringPiece field,
               v8::Local<v8::Value> value,
               std::string& out);
  bool Convert(base::StringPiece field,
               v8::Local<v8::Value> value,
               v8::Local<v8::Value>& out);

  void MarkFailed(base::StringPiece fail_message);
  void MarkFailedWithException(const v8::TryCatch& catcher);

  raw_ptr<AuctionV8Helper> v8_helper_;
  std::string error_prefix_;
  v8::Local<v8::Object> object_;  // Can be empty if undefined/null.

  // This gets set to true on first error, and all further ops fail fast after
  // that. This is needed because user-provided custom type coercions can have
  // side effects, so we should not run them when the process already failed.
  bool failed_ = false;
  v8::Local<v8::Value> failure_exception_;
  std::string failure_message_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_WEBIDL_COMPAT_H_
