// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_WEBIDL_COMPAT_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_WEBIDL_COMPAT_H_

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-value.h"

namespace v8 {
class TryCatch;
class Isolate;
}  // namespace v8

namespace auction_worklet {

// Helps distinguish between double and unrestricted double in IDL.
struct CONTENT_EXPORT UnrestrictedDouble {
  double number;
};

// IdlConvert converts v8::Value values to a C++ representation
// following the semantics of WebIDL for certain simpler, non-structured,
// WebIDL types. The IDL type desired is denoted by the output parameter of the
// particular Convert() overload used.
//
// Clients should have AuctionV8Helper::TimeLimitScope active to protect against
// non-termination.
//
// `error_prefix` is prepended to non-exception error messages,
//  in order to identify where the error occurred, e.g. foo.js:100, etc.
//
// `error_subject` names the thing being converted (e.g. field foo,
//  argument bar, etc.)
class CONTENT_EXPORT IdlConvert {
 public:
  // This should be bigger than the biggest Sequence<> the users of this need to
  // handle, which is currently up to
  // blink::kMaxAdAuctionAdComponentsConfigLimit.
  static const size_t kSequenceLengthLimit = 101;

  // Outcome of the conversion, either success or some kind of error.
  class CONTENT_EXPORT Status {
   public:
    enum class Success { kSuccessTag };

    struct CONTENT_EXPORT Timeout {
      std::string timeout_message;
    };

    struct CONTENT_EXPORT Exception {
      v8::Local<v8::Value> exception;
      v8::Local<v8::Message> message;
    };

    // This must match the order inside `StatusValue`
    enum class Type { kSuccess, kTimeout, kErrorMessage, kException };
    using StatusValue = absl::variant<Success, Timeout, std::string, Exception>;

    Status();
    Status(const Status& other) = delete;
    Status(Status&& other);
    ~Status();

    Status& operator=(const Status&) = delete;
    Status& operator=(Status&&);

    static Status MakeTimeout(std::string timeout_message) {
      Timeout t;
      t.timeout_message = std::move(timeout_message);
      return Status(std::move(t));
    }

    static Status MakeException(v8::Local<v8::Value> exception,
                                v8::Local<v8::Message> message) {
      Exception e;
      e.exception = exception;
      e.message = message;
      return Status(std::move(e));
    }

    static Status MakeErrorMessage(std::string message) {
      return Status(StatusValue(std::move(message)));
    }

    static Status MakeSuccess() {
      return Status(StatusValue(Success::kSuccessTag));
    }

    Type type() const { return static_cast<Type>(value_.index()); }

    bool is_success() const { return type() == Type::kSuccess; }
    bool is_timeout() const { return type() == Type::kTimeout; }

    // Serializes the conversion error message. This can only be called if
    // the status is not a success.
    std::string ConvertToErrorString(v8::Isolate* isolate) const;

    // Propagates any error to v8, raising exceptions if needed. This is
    // intended to be called from method bindings to propagate the failures in
    // type-checking their arguments up to the caller.
    void PropagateErrorsToV8(AuctionV8Helper* v8_helper);

    const Exception& GetException() const {
      DCHECK_EQ(type(), Type::kException);
      return absl::get<Exception>(value_);
    }

   private:
    explicit Status(StatusValue value);

    StatusValue value_;
  };

  // For values that should be converted to WebIDL "unrestricted double" type.
  // Unlike "double" it permits NaN and +/- infinity.
  static Status Convert(v8::Isolate* isolate,
                        std::string_view error_prefix,
                        std::initializer_list<std::string_view> error_subject,
                        v8::Local<v8::Value> value,
                        UnrestrictedDouble& out);

  // For values that should be converted to WebIDL "double" type.
  static Status Convert(v8::Isolate* isolate,
                        std::string_view error_prefix,
                        std::initializer_list<std::string_view> error_subject,
                        v8::Local<v8::Value> value,
                        double& out);

  // For values that should be converted to WebIDL "boolean" type.
  static Status Convert(v8::Isolate* isolate,
                        std::string_view error_prefix,
                        std::initializer_list<std::string_view> error_subject,
                        v8::Local<v8::Value> value,
                        bool& out);

  // For values that should be converted to WebIDL "DOMString" type.
  static Status Convert(v8::Isolate* isolate,
                        std::string_view error_prefix,
                        std::initializer_list<std::string_view> error_subject,
                        v8::Local<v8::Value> value,
                        std::string& out);

  // For values that should be converted to WebIDL "DOMString" type, with the
  // destination using 16-bit strings.
  static Status Convert(v8::Isolate* isolate,
                        std::string_view error_prefix,
                        std::initializer_list<std::string_view> error_subject,
                        v8::Local<v8::Value> value,
                        std::u16string& out);

  // For values that should be converted to WebIDL "bigint" type.
  static Status Convert(v8::Isolate* isolate,
                        std::string_view error_prefix,
                        std::initializer_list<std::string_view> error_subject,
                        v8::Local<v8::Value> value,
                        v8::Local<v8::BigInt>& out);

  // For values that should be converted to WebIDL "long" type.
  static Status Convert(v8::Isolate* isolate,
                        std::string_view error_prefix,
                        std::initializer_list<std::string_view> error_subject,
                        v8::Local<v8::Value> value,
                        int32_t& out);

  // For values that should be converted to WebIDL "unsigned long" type.
  static Status Convert(v8::Isolate* isolate,
                        std::string_view error_prefix,
                        std::initializer_list<std::string_view> error_subject,
                        v8::Local<v8::Value> value,
                        uint32_t& out);

  // For values that should be converted to WebIDL "(bigint or long)" type.
  static Status Convert(v8::Isolate* isolate,
                        std::string_view error_prefix,
                        std::initializer_list<std::string_view> error_subject,
                        v8::Local<v8::Value> value,
                        absl::variant<int32_t, v8::Local<v8::BigInt>>& out);

  // For values that should be converted to WebIDL "any" type.
  // This just passes the incoming value through, and is here for benefit of
  // DictConverter; it should not be used directly.
  static Status Convert(v8::Isolate* isolate,
                        std::string_view error_prefix,
                        std::initializer_list<std::string_view> error_subject,
                        v8::Local<v8::Value> value,
                        v8::Local<v8::Value>& out);

  // Tries to use `iterator_factory` returned by CheckForSequence to iterate
  // over `iterable`. Entries will be provided one-by-one to `item_callback`
  // (0 to kSequenceLengthLimit times).
  //
  // `item_callback` is expected to typecheck its input and return the status,
  // with failures interrupting the conversion and propagated up.
  static Status ConvertSequence(
      AuctionV8Helper* v8_helper,
      std::string_view error_prefix,
      std::initializer_list<std::string_view> error_subject,
      v8::Local<v8::Object> iterable,
      v8::Local<v8::Object> iterator_factory,
      base::RepeatingCallback<Status(v8::Local<v8::Value>)> item_callback);

  // Check if given `maybe_iterable` can be treated as a sequence.
  // If it can be, sets `result` to the value of the @@iterator property and
  // returns success.
  //
  // If trying to look up the @@iterator property throws an exception or times
  // out, or returns in a non-null-or-undefined value that's not a callable
  // object, returns a failure.
  //
  // Otherwise, returns success and keeps `result` unchanged. This denotes
  // `maybe_iterable` not being a sequence, and that it can be used as some
  // other type in a union containing a sequence.
  static Status CheckForSequence(
      v8::Isolate* isolate,
      std::string_view error_prefix,
      std::initializer_list<std::string_view> error_subject,
      v8::Local<v8::Object> maybe_iterable,
      v8::Local<v8::Object>& result);

  // Makes a failure result based on state of `catcher`. If nothing is set on
  // `catcher`, will report a conversion failure.
  static Status MakeConversionFailure(
      const v8::TryCatch& catcher,
      std::string_view error_prefix,
      std::initializer_list<std::string_view> error_subject,
      std::string_view type_name);
};

// Tries to convert to WebIDL Record<DOMString, USVString>.
// `time_limit_scope` must have a non-null time limit.
CONTENT_EXPORT IdlConvert::Status ConvertRecord(
    AuctionV8Helper* v8_helper,
    AuctionV8Helper::TimeLimitScope& time_limit_scope,
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject,
    v8::Local<v8::Value> value,
    std::vector<std::pair<std::string, std::string>>& out);

// DictConverter helps convert a v8::Value that's supposed to be a WebIDL
// dictionary to C++ values one field at a time, following the appropriate
// semantics. Please see the constructor comment for more details.
//
// After construction, you should call GetOptional/GetRequired/
// GetOptionalSequence as appropriate for all the fields in the dictionary in
// lexicographic order. Unlike gin, this will do type conversions.
//
// All the Get... methods return true on success, false on failure.
// In particular, if GetOptional() is called on a field that's missing,
// the out-param is set to nullopt, and true is returned.
//
// In case of failure all further Get... ops will fail, and the state of
// out-param is unpredictable. You can use ErrorMessage() to get the
// description of the first detected error.
//
// The output type is what determines the conversion --- see the various
// `IdlConvert::Convert` overloads, except sequences are handled specially, via
//  GetOptionalSequence.
class CONTENT_EXPORT DictConverter {
 public:
  // Prepares to convert `value` to a WebIDL dictionary.
  //
  // Since fields conversions may loop infinitely, a `time_limit_scope` must
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
  ~DictConverter();

  template <typename T>
  bool GetRequired(std::string_view field, T& out) {
    if (is_failed()) {
      return false;
    }

    v8::Local<v8::Value> val = GetMember(field);
    if (is_failed()) {
      return false;
    }

    if (val->IsUndefined()) {
      MarkFailed(base::StrCat({"Required field '", field, "' is undefined."}));
      return false;
    }

    status_ = IdlConvert::Convert(v8_helper_->isolate(), error_prefix_,
                                  {"field '", field, "'"}, val, out);
    return is_success();
  }

  template <typename T>
  bool GetOptional(std::string_view field, std::optional<T>& out) {
    if (is_failed()) {
      return false;
    }

    v8::Local<v8::Value> val = GetMember(field);
    if (is_failed()) {
      return false;
    }

    if (val->IsUndefined()) {
      out = std::nullopt;
      return true;
    }

    out.emplace();
    status_ = IdlConvert::Convert(v8_helper_->isolate(), error_prefix_,
                                  {"field '", field, "'"}, val, out.value());
    return is_success();
  }

  // Gets an optional sequence field `field`. If the field exists,
  // `exists_callback` will be called, and then entries will be provided
  // one-by-one to `item_callback` (0 to kSequenceLengthLimit times).
  // `item_callback` is expected to typecheck its input and return status as
  // appropriate; with failures forwarded to `this`.
  bool GetOptionalSequence(
      std::string_view field,
      base::OnceClosure exists_callback,
      base::RepeatingCallback<IdlConvert::Status(v8::Local<v8::Value>)>
          item_callback);

  std::string ErrorMessage() const;

  // Returns conversion status. This clears any remembered errors.
  IdlConvert::Status TakeStatus() { return std::move(status_); }

  // Overrides status information with `status`.
  // `this` should not be in a failed state. The intent is to forward conversion
  // success/failure from recursive conversions.
  void SetStatus(IdlConvert::Status status);

  bool is_failed() const { return !is_success(); }

  bool is_success() const { return status_.is_success(); }

  // This is non-empty only when an exception specifically got thrown by
  // something invoked during conversion; not for any errors synthesized here.
  v8::MaybeLocal<v8::Value> FailureException() const;

  // Returns true if conversion failed because execution timeout has been
  // reached.
  bool FailureIsTimeout() const;

 private:
  // This can mark failure.
  v8::Local<v8::Value> GetMember(std::string_view field);

  void MarkFailed(std::string_view fail_message);
  void MarkFailedWithTimeout(std::string_view fail_message);
  void MarkFailedWithException(const v8::TryCatch& catcher);

  raw_ptr<AuctionV8Helper> v8_helper_;
  std::string error_prefix_;
  v8::Local<v8::Object> object_;  // Can be empty if undefined/null.

  // This gets latched to a non-success on first error, and all further ops fail
  // fast after that. This is needed because user-provided custom type coercions
  // can have side effects, so we should not run them when the process already
  // failed.
  IdlConvert::Status status_;
};

// ArgsConverter helps convert argument lists, one argument at a time.
class CONTENT_EXPORT ArgsConverter {
 public:
  // This checks that there are at least `min_required_args` arguments.
  //
  // `v8_helper` and `args` are expected to outlast `this`, and be non-null.
  ArgsConverter(AuctionV8Helper* v8_helper,
                AuctionV8Helper::TimeLimitScope& time_limit_scope,
                std::string error_prefix,
                const v8::FunctionCallbackInfo<v8::Value>* args,
                int min_required_args);
  ~ArgsConverter();

  template <typename T>
  bool ConvertArg(int pos, std::string_view arg_name, T& out) {
    if (is_failed()) {
      return false;
    }
    status_ =
        IdlConvert::Convert(v8_helper_->isolate(), error_prefix_,
                            {"argument '", arg_name, "'"}, (*args_)[pos], out);
    return is_success();
  }

  IdlConvert::Status TakeStatus() { return std::move(status_); }

  // Overrides status information with `status`.
  // `this` should not be in a failed state. The intent is to forward conversion
  // success/failure from recursive conversions.
  void SetStatus(IdlConvert::Status status);

  bool is_failed() const { return !is_success(); }

  bool is_success() const { return status_.is_success(); }

 private:
  raw_ptr<AuctionV8Helper> v8_helper_;
  std::string error_prefix_;
  const raw_ref<const v8::FunctionCallbackInfo<v8::Value>> args_;
  IdlConvert::Status status_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_WEBIDL_COMPAT_H_
