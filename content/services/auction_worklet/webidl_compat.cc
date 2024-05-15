// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/webidl_compat.h"

#include <cmath>
#include <initializer_list>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "gin/converter.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-primitive.h"

namespace auction_worklet {

namespace {

IdlConvert::Status MakeRecordConversionFailure(
    const v8::TryCatch& try_catch,
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject) {
  return IdlConvert::MakeConversionFailure(
      try_catch, error_prefix, error_subject, "record<DOMString, USVString>");
}

IdlConvert::Status MakeIterFailure(
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject,
    const v8::TryCatch& catcher) {
  if (catcher.HasTerminated()) {
    return IdlConvert::Status::MakeTimeout(
        base::StrCat({error_prefix, "Timeout iterating over ",
                      base::StrCat(error_subject), "."}));
  } else if (catcher.HasCaught()) {
    return IdlConvert::Status::MakeException(catcher.Exception(),
                                             catcher.Message());
  } else {
    return IdlConvert::Status::MakeErrorMessage(
        base::StrCat({error_prefix, "Trouble iterating over ",
                      base::StrCat(error_subject), "."}));
  }
}

}  // namespace

const size_t IdlConvert::kSequenceLengthLimit;

IdlConvert::Status::Status() : value_(Success::kSuccessTag) {}
IdlConvert::Status::Status(Status&& other) = default;
IdlConvert::Status::~Status() = default;
IdlConvert::Status& IdlConvert::Status::operator=(Status&&) = default;

std::string IdlConvert::Status::ConvertToErrorString(
    v8::Isolate* isolate) const {
  switch (type()) {
    case Type::kSuccess:
      NOTREACHED_IN_MIGRATION();
      return std::string();
    case Type::kTimeout:
      return absl::get<Timeout>(value_).timeout_message;
    case Type::kErrorMessage:
      return absl::get<std::string>(value_);
    case Type::kException:
      return AuctionV8Helper::FormatExceptionMessage(
          isolate->GetCurrentContext(), absl::get<Exception>(value_).message);
  }
}

void IdlConvert::Status::PropagateErrorsToV8(AuctionV8Helper* v8_helper) {
  switch (type()) {
    case Type::kSuccess:
      break;
    case Type::kTimeout:
      // We don't want to set an exception in case of timeout since it would
      // override the timeout.
      break;
    case Type::kErrorMessage: {
      std::string message = absl::get<std::string>(value_);
      // Remove any trailing period since v8 will add one.
      if (base::EndsWith(message, ".")) {
        message.pop_back();
      }
      v8_helper->isolate()->ThrowException(v8::Exception::TypeError(
          v8_helper->CreateUtf8String(message).ToLocalChecked()));
      break;
    }
    case Type::kException: {
      v8_helper->isolate()->ThrowException(
          absl::get<Exception>(value_).exception);
      break;
    }
  }
}

IdlConvert::Status::Status(StatusValue value) : value_(std::move(value)) {}

// These mostly match the various routines in
// third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h

// static
IdlConvert::Status IdlConvert::Convert(
    v8::Isolate* isolate,
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject,
    v8::Local<v8::Value> value,
    UnrestrictedDouble& out) {
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Number> number_value;
  if (value->IsNumber()) {
    number_value = value.As<v8::Number>();
  } else if (!value->ToNumber(isolate->GetCurrentContext())
                  .ToLocal(&number_value)) {
    // Converting a non-Number to a Number failed somehow.
    return MakeConversionFailure(try_catch, error_prefix, error_subject,
                                 "Number");
  }

  out.number = number_value->Value();
  return Status::MakeSuccess();
}

// static
IdlConvert::Status IdlConvert::Convert(
    v8::Isolate* isolate,
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject,
    v8::Local<v8::Value> value,
    double& out) {
  UnrestrictedDouble unrestricted_out;
  IdlConvert::Status unrestricted_double_result =
      Convert(isolate, error_prefix, error_subject, value, unrestricted_out);
  if (unrestricted_double_result.type() != Status::Type::kSuccess) {
    return unrestricted_double_result;
  }

  if (!std::isfinite(unrestricted_out.number)) {
    return Status::MakeErrorMessage(
        base::StrCat({error_prefix, "Converting ", base::StrCat(error_subject),
                      " to a Number did not produce a finite double."}));
  }
  out = unrestricted_out.number;
  return Status::MakeSuccess();
}

// static
IdlConvert::Status IdlConvert::Convert(
    v8::Isolate* isolate,
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject,
    v8::Local<v8::Value> value,
    bool& out) {
  if (value->IsBoolean()) {
    out = value.As<v8::Boolean>()->Value();
  } else {
    out = value->BooleanValue(isolate);
  }
  return Status::MakeSuccess();
}

// static
IdlConvert::Status IdlConvert::Convert(
    v8::Isolate* isolate,
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject,
    v8::Local<v8::Value> value,
    std::string& out) {
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::String> v8_string;
  if (value->IsString()) {
    v8_string = value.As<v8::String>();
  } else if (!value->ToString(isolate->GetCurrentContext())
                  .ToLocal(&v8_string)) {
    return MakeConversionFailure(try_catch, error_prefix, error_subject,
                                 "String");
  }

  bool gin_ok = gin::Converter<std::string>::FromV8(isolate, v8_string, &out);
  DCHECK(gin_ok);  // Should never fail on a v8::String
  return Status::MakeSuccess();
}

IdlConvert::Status IdlConvert::Convert(
    v8::Isolate* isolate,
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject,
    v8::Local<v8::Value> value,
    std::u16string& out) {
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::String> v8_string;
  if (value->IsString()) {
    v8_string = value.As<v8::String>();
  } else if (!value->ToString(isolate->GetCurrentContext())
                  .ToLocal(&v8_string)) {
    return MakeConversionFailure(try_catch, error_prefix, error_subject,
                                 "String");
  }

  bool gin_ok =
      gin::Converter<std::u16string>::FromV8(isolate, v8_string, &out);
  DCHECK(gin_ok);  // Should never fail on a v8::String
  return Status::MakeSuccess();
}

// static
IdlConvert::Status IdlConvert::Convert(
    v8::Isolate* isolate,
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject,
    v8::Local<v8::Value> value,
    v8::Local<v8::BigInt>& out) {
  v8::TryCatch try_catch(isolate);
  if (!value->ToBigInt(isolate->GetCurrentContext()).ToLocal(&out)) {
    return MakeConversionFailure(try_catch, error_prefix, error_subject,
                                 "BigInt");
  }
  return Status::MakeSuccess();
}

// static
IdlConvert::Status IdlConvert::Convert(
    v8::Isolate* isolate,
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject,
    v8::Local<v8::Value> value,
    int32_t& out) {
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Int32> int32_value;
  if (!value->ToInt32(isolate->GetCurrentContext()).ToLocal(&int32_value)) {
    return MakeConversionFailure(try_catch, error_prefix, error_subject,
                                 "ToInt32");
  }

  out = int32_value->Value();
  return Status::MakeSuccess();
}

// static
IdlConvert::Status IdlConvert::Convert(
    v8::Isolate* isolate,
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject,
    v8::Local<v8::Value> value,
    uint32_t& out) {
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Uint32> uint32_value;
  if (!value->ToUint32(isolate->GetCurrentContext()).ToLocal(&uint32_value)) {
    return MakeConversionFailure(try_catch, error_prefix, error_subject,
                                 "ToUint32");
  }

  out = uint32_value->Value();
  return Status::MakeSuccess();
}

// static
IdlConvert::Status IdlConvert::Convert(
    v8::Isolate* isolate,
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject,
    v8::Local<v8::Value> value,
    absl::variant<int32_t, v8::Local<v8::BigInt>>& out) {
  // A union that has both a BigInt and a normal number follows special rules
  // to disambiguate.
  //
  // https://webidl.spec.whatwg.org/#converted-to-a-numeric-type-or-bigint
  if (value->IsBigInt()) {
    out.emplace<v8::Local<v8::BigInt>>();
    return IdlConvert::Convert(isolate, error_prefix, error_subject, value,
                               absl::get<v8::Local<v8::BigInt>>(out));
  } else if (value->IsNumber()) {
    out.emplace<int32_t>();
    return IdlConvert::Convert(isolate, error_prefix, error_subject, value,
                               absl::get<int32_t>(out));
  } else {
    v8::TryCatch try_catch(isolate);
    v8::Local<v8::Numeric> num_value;
    if (!value->ToNumeric(isolate->GetCurrentContext()).ToLocal(&num_value)) {
      return MakeConversionFailure(try_catch, error_prefix, error_subject,
                                   "(bigint or long)");
    }
    if (num_value->IsBigInt()) {
      out = num_value.As<v8::BigInt>();
      return Status::MakeSuccess();
    } else {
      out.emplace<int32_t>();
      return IdlConvert::Convert(isolate, error_prefix, error_subject,
                                 num_value, absl::get<int32_t>(out));
    }
  }
}

// static
IdlConvert::Status IdlConvert::Convert(
    v8::Isolate* isolate,
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject,
    v8::Local<v8::Value> value,
    v8::Local<v8::Value>& out) {
  out = value;
  return Status::MakeSuccess();
}

// static
IdlConvert::Status IdlConvert::MakeConversionFailure(
    const v8::TryCatch& catcher,
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject,
    std::string_view type_name) {
  if (catcher.HasTerminated()) {
    return Status::MakeTimeout(
        base::StrCat({error_prefix, "Converting ", base::StrCat(error_subject),
                      " to ", type_name, " timed out."}));
  } else if (catcher.HasCaught()) {
    return Status::MakeException(catcher.Exception(), catcher.Message());
  } else {
    std::string fail_message =
        base::StrCat({error_prefix, "Trouble converting ",
                      base::StrCat(error_subject), " to a ", type_name, "."});
    return Status::MakeErrorMessage(std::move(fail_message));
  }
}

IdlConvert::Status ConvertRecord(
    AuctionV8Helper* v8_helper,
    AuctionV8Helper::TimeLimitScope& time_limit_scope,
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject,
    v8::Local<v8::Value> in_val,
    std::vector<std::pair<std::string, std::string>>& out) {
  DCHECK(time_limit_scope.has_time_limit());
  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  // This follows https://webidl.spec.whatwg.org/#es-record, and is heavily
  // based on NativeValueTraits<IDLRecord<K, V>>::NativeValue in
  // third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h
  if (!in_val->IsObject()) {
    return IdlConvert::Status::MakeErrorMessage(base::StrCat(
        {error_prefix, "Cannot convert ", base::StrCat(error_subject),
         " to a record since it's not an Object."}));
  }

  v8::Local<v8::Object> in = in_val.As<v8::Object>();

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::String> enumerable_name =
      v8_helper->CreateStringFromLiteral("enumerable");

  v8::Local<v8::Array> keys;
  if (!in->GetOwnPropertyNames(context, v8::PropertyFilter::ALL_PROPERTIES,
                               v8::KeyConversionMode::kConvertToString)
           .ToLocal(&keys)) {
    return MakeRecordConversionFailure(try_catch, error_prefix, error_subject);
  }

  out.clear();
  for (size_t i = 0; i < keys->Length(); i++) {
    v8::Local<v8::Value> key;
    if (!keys->Get(context, i).ToLocal(&key)) {
      return MakeRecordConversionFailure(try_catch, error_prefix,
                                         error_subject);
    }

    v8::Local<v8::Value> desc;
    if (!in->GetOwnPropertyDescriptor(context, key.As<v8::Name>())
             .ToLocal(&desc)) {
      return MakeRecordConversionFailure(try_catch, error_prefix,
                                         error_subject);
    }

    // Undefined `desc` gets skipped.
    if (desc->IsUndefined()) {
      continue;
    }

    v8::Local<v8::Value> enumerable;
    if (!desc->IsObject() || !desc.As<v8::Object>()
                                  ->Get(context, enumerable_name)
                                  .ToLocal(&enumerable)) {
      return MakeRecordConversionFailure(try_catch, error_prefix,
                                         error_subject);
    }

    // As do things which are not actually enumerable.
    if (!enumerable->BooleanValue(isolate)) {
      continue;
    }

    std::string typed_key;
    IdlConvert::Status key_status = IdlConvert::Convert(
        isolate, error_prefix, error_subject, key, typed_key);
    if (!key_status.is_success()) {
      return key_status;
    }

    v8::Local<v8::Value> value;
    if (!in->Get(context, key).ToLocal(&value)) {
      return MakeRecordConversionFailure(try_catch, error_prefix,
                                         error_subject);
    }

    // For USVString, we are supposed to replace mismatched surrogates with
    // U+FFFD. Luckily, UTF16ToUTF8 does it, so we just get a 16-bit value
    // from v8 and feed it to that.
    std::u16string typed_value16;
    IdlConvert::Status value_status = IdlConvert::Convert(
        isolate, error_prefix, error_subject, value, typed_value16);
    if (!value_status.is_success()) {
      return value_status;
    }
    std::string typed_value = base::UTF16ToUTF8(typed_value16);

    // Since our keys are DOMString, and not USVString, we don't actually have
    // to worry about duplicates.
    out.emplace_back(std::move(typed_key), std::move(typed_value));
  }

  return IdlConvert::Status::MakeSuccess();
}

DictConverter::DictConverter(AuctionV8Helper* v8_helper,
                             AuctionV8Helper::TimeLimitScope& time_limit_scope,
                             std::string error_prefix,
                             v8::Local<v8::Value> value)
    : v8_helper_(v8_helper), error_prefix_(error_prefix) {
  DCHECK(time_limit_scope.has_time_limit());
  if (value->IsObject()) {
    object_ = v8::Local<v8::Object>::Cast(value);
  } else if (!value->IsNullOrUndefined()) {
    // WebIDL has a magic case where null or undefined are OK as a dict,
    // and every field returns undefined. (So it's OK if nothing is required).
    MarkFailed(
        "Value passed as dictionary is neither object, null, nor undefined.");
  }
}

DictConverter::~DictConverter() = default;

bool DictConverter::GetOptionalSequence(
    std::string_view field,
    base::OnceClosure exists_callback,
    base::RepeatingCallback<IdlConvert::Status(v8::Local<v8::Value>)>
        item_callback) {
  if (is_failed()) {
    return false;
  }

  v8::Local<v8::Value> val = GetMember(field);
  if (is_failed()) {
    return false;
  }

  if (val->IsUndefined()) {
    return true;
  }

  std::move(exists_callback).Run();

  std::initializer_list<std::string_view> field_info = {"field '", field, "'"};

  // If Type(V) is not Object, throw a TypeError.
  if (!val->IsObject()) {
    status_ = IdlConvert::Status::MakeErrorMessage(base::StrCat(
        {error_prefix_, "Sequence field '", field, "' must be an Object."}));
    return false;
  }

  v8::Local<v8::Object> iterable = val.As<v8::Object>();
  v8::Local<v8::Object> iterator_factory;
  status_ =
      IdlConvert::CheckForSequence(v8_helper_->isolate(), error_prefix_,
                                   field_info, iterable, iterator_factory);
  if (iterator_factory.IsEmpty()) {
    if (status_.is_success()) {
      status_ = IdlConvert::Status::MakeErrorMessage(
          base::StrCat({error_prefix_, "Trouble iterating over field '", field,
                        "' as it does not appear to be a sequence."}));
    }
    return false;
  }

  status_ = IdlConvert::ConvertSequence(v8_helper_.get(), error_prefix_,
                                        field_info, iterable, iterator_factory,
                                        std::move(item_callback));
  return is_success();
}

std::string DictConverter::ErrorMessage() const {
  return status_.is_success()
             ? std::string()
             : status_.ConvertToErrorString(v8_helper_->isolate());
}

v8::MaybeLocal<v8::Value> DictConverter::FailureException() const {
  if (status_.type() == IdlConvert::Status::Type::kException) {
    return status_.GetException().exception;
  } else {
    return v8::MaybeLocal<v8::Value>();
  }
}

bool DictConverter::FailureIsTimeout() const {
  return status_.type() == IdlConvert::Status::Type::kTimeout;
}

void DictConverter::SetStatus(IdlConvert::Status status) {
  DCHECK(!is_failed());
  status_ = std::move(status);
}

v8::Local<v8::Value> DictConverter::GetMember(std::string_view field) {
  v8::Isolate* isolate = v8_helper_->isolate();
  // WebIDL treats undefined or null as a dict with all fields undefined.
  if (object_.IsEmpty()) {
    return v8::Undefined(isolate);
  }

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> val;
  if (!object_
           ->Get(isolate->GetCurrentContext(), gin::StringToV8(isolate, field))
           .ToLocal(&val)) {
    // An exception can get thrown in case of getter, proxy, etc, or those
    // conversions may fail to terminate, causing the time out to be hit.
    if (try_catch.HasTerminated()) {
      MarkFailedWithTimeout(base::StrCat(
          {"Execution timed out trying to access field '", field, "'."}));
    } else if (try_catch.HasCaught()) {
      MarkFailedWithException(try_catch);
    }
    // Failing to get otherwise isn't a problem unless this is a required
    // field, which will get checked in `GetRequired()`.
    return v8::Undefined(isolate);
  }

  return val;
}

// static
IdlConvert::Status IdlConvert::CheckForSequence(
    v8::Isolate* isolate,
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject,
    v8::Local<v8::Object> maybe_iterable,
    v8::Local<v8::Object>& result) {
  v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
  v8::TryCatch try_catch(isolate);
  // Both converting a known sequence and resolving a union for a sequence
  // relies on the following spec step, as basically implemented in this
  // method:
  //    "Let method be ? GetMethod(V, @@iterator)."
  // We just have to be a bit careful to distinguish cases where the step hard
  // fails with an exception, or just says it's not a sequence, as the union
  // case behaves differently between the two.
  v8::Local<v8::Value> method_val;
  if (!maybe_iterable->Get(current_context, v8::Symbol::GetIterator(isolate))
           .ToLocal(&method_val)) {
    return MakeIterFailure(error_prefix, error_subject, try_catch);
  }

  if (method_val->IsNullOrUndefined()) {
    // This is a non-error, non-iterable case.
    return Status::MakeSuccess();
  }

  if (!method_val->IsObject()) {
    return MakeIterFailure(error_prefix, error_subject, try_catch);
  }
  v8::Local<v8::Object> method = method_val.As<v8::Object>();
  if (!method->IsCallable()) {
    return MakeIterFailure(error_prefix, error_subject, try_catch);
  }
  result = method;
  return Status::MakeSuccess();
}

// static
IdlConvert::Status IdlConvert::ConvertSequence(
    AuctionV8Helper* v8_helper,
    std::string_view error_prefix,
    std::initializer_list<std::string_view> error_subject,
    v8::Local<v8::Object> iterable,
    v8::Local<v8::Object> method,
    base::RepeatingCallback<IdlConvert::Status(v8::Local<v8::Value>)>
        item_callback) {
  // This is based on https://webidl.spec.whatwg.org/#es-sequence and its
  // implementation in
  // third_party/blink/renderer/bindings/core/v8/script_iterator.cc
  //
  // It's probably best understood from:
  //   https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Iteration_protocols
  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
  v8::TryCatch try_catch(isolate);

  v8::Local<v8::String> next_name = v8_helper->CreateStringFromLiteral("next");
  v8::Local<v8::String> done_name = v8_helper->CreateStringFromLiteral("done");
  v8::Local<v8::String> value_name =
      v8_helper->CreateStringFromLiteral("value");

  // Let iter be ? GetIterator(iterable, sync, method)
  v8::Local<v8::Value> iterator_val;
  if (!method
           ->CallAsFunction(current_context, iterable, /*argc=*/0,
                            /*argv=*/nullptr)
           .ToLocal(&iterator_val) ||
      !iterator_val->IsObject()) {
    return MakeIterFailure(error_prefix, error_subject, try_catch);
  }
  v8::Local<v8::Object> iterator = iterator_val.As<v8::Object>();

  v8::Local<v8::Value> next_method_val;
  if (!iterator->Get(current_context, next_name).ToLocal(&next_method_val) ||
      !next_method_val->IsObject()) {
    return MakeIterFailure(error_prefix, error_subject, try_catch);
  }
  v8::Local<v8::Object> next_method = next_method_val.As<v8::Object>();
  if (!next_method->IsCallable()) {
    return MakeIterFailure(error_prefix, error_subject, try_catch);
  }

  size_t out_size = 0;
  while (true) {
    v8::Local<v8::Value> result_val;
    if (!next_method
             ->CallAsFunction(current_context, iterator, /*argc=*/0,
                              /*argv=*/nullptr)
             .ToLocal(&result_val) ||
        !result_val->IsObject()) {
      return MakeIterFailure(error_prefix, error_subject, try_catch);
    }
    v8::Local<v8::Object> result = result_val.As<v8::Object>();

    bool done = false;  // ToBoolean(undefined)
    v8::Local<v8::Value> done_val;
    if (result->Get(current_context, done_name).ToLocal(&done_val)) {
      // Can use our Convert() since it doesn't fail for bools.
      IdlConvert::Convert(isolate, "", {}, done_val, done);
    } else if (try_catch.HasCaught() || try_catch.HasTerminated()) {
      return MakeIterFailure(error_prefix, error_subject, try_catch);
    }

    if (done) {
      break;
    }

    // Check if we're over limit (before actually getting the item that
    // would put us over).
    ++out_size;
    if (out_size > kSequenceLengthLimit) {
      return Status::MakeErrorMessage(
          base::StrCat({error_prefix, "Length limit for sequence ",
                        base::StrCat(error_subject), " exceeded."}));
    }

    v8::Local<v8::Value> next_item;
    if (!result->Get(current_context, value_name).ToLocal(&next_item)) {
      return MakeIterFailure(error_prefix, error_subject, try_catch);
    }

    Status entry_status = item_callback.Run(next_item);
    if (!entry_status.is_success()) {
      return entry_status;
    }
  }

  return Status::MakeSuccess();
}

void DictConverter::MarkFailed(std::string_view fail_message) {
  DCHECK(!is_failed());
  status_ = IdlConvert::Status::MakeErrorMessage(
      base::StrCat({error_prefix_, fail_message}));
}

void DictConverter::MarkFailedWithTimeout(std::string_view fail_message) {
  DCHECK(!is_failed());
  status_ = IdlConvert::Status::MakeTimeout(
      base::StrCat({error_prefix_, fail_message}));
}

void DictConverter::MarkFailedWithException(const v8::TryCatch& catcher) {
  DCHECK(!is_failed());
  status_ =
      IdlConvert::Status::MakeException(catcher.Exception(), catcher.Message());
}

ArgsConverter::ArgsConverter(AuctionV8Helper* v8_helper,
                             AuctionV8Helper::TimeLimitScope& time_limit_scope,
                             std::string error_prefix,
                             const v8::FunctionCallbackInfo<v8::Value>* args,
                             int min_required_args)
    : v8_helper_(v8_helper), error_prefix_(error_prefix), args_(*args) {
  DCHECK(time_limit_scope.has_time_limit());
  if (args->Length() < min_required_args) {
    status_ = IdlConvert::Status::MakeErrorMessage(base::StrCat(
        {error_prefix_, "at least ", base::NumberToString(min_required_args),
         " argument(s) are required."}));
  }
}

ArgsConverter::~ArgsConverter() = default;

void ArgsConverter::SetStatus(IdlConvert::Status status) {
  DCHECK(!is_failed());
  status_ = std::move(status);
}

}  // namespace auction_worklet
