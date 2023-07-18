// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/webidl_compat.h"

#include <cmath>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "gin/converter.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-primitive.h"

namespace auction_worklet {

const size_t DictConverter::kSequenceLengthLimit;

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

bool DictConverter::GetOptionalSequence(
    base::StringPiece field,
    base::OnceClosure exists_callback,
    base::RepeatingCallback<bool(v8::Local<v8::Value>)> item_callback) {
  if (failed_) {
    return false;
  }

  v8::Local<v8::Value> val = GetMember(field);
  if (failed_) {
    return false;
  }

  if (val->IsUndefined()) {
    return true;
  }

  std::move(exists_callback).Run();

  return ConvertSequence(field, val, std::move(item_callback));
}

void DictConverter::PropagateErrorsFrom(DictConverter& other_converter) {
  DCHECK(!failed_);
  DCHECK(other_converter.failed_);
  failed_ = true;
  failure_exception_ = other_converter.failure_exception_;
  failure_message_ = std::move(other_converter.failure_message_);
  failed_with_timeout_ = other_converter.failed_with_timeout_;
}

v8::Local<v8::Value> DictConverter::GetMember(base::StringPiece field) {
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
    // Failing to get otherwise isn't a problem unless this is a required field,
    // which will get checked in `GetRequired()`.
    return v8::Undefined(isolate);
  }

  return val;
}

// These mostly match the various routines in
// third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h
bool DictConverter::Convert(base::StringPiece field,
                            v8::Local<v8::Value> value,
                            UnrestrictedDouble& out) {
  v8::Isolate* isolate = v8_helper_->isolate();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Number> number_value;
  if (value->IsNumber()) {
    number_value = value.As<v8::Number>();
  } else if (!value->ToNumber(isolate->GetCurrentContext())
                  .ToLocal(&number_value)) {
    // Converting a non-Number to a Number failed somehow.
    if (try_catch.HasTerminated()) {
      MarkFailedWithTimeout(base::StrCat(
          {"Converting field '", field, "' to Number timed out."}));
    } else {
      MarkFailedWithExceptionOrMessage(
          try_catch, base::StrCat({"Trouble converting field '", field,
                                   "' to a Number."}));
    }
    return false;
  }

  out.number = number_value->Value();
  return true;
}

bool DictConverter::Convert(base::StringPiece field,
                            v8::Local<v8::Value> value,
                            double& out) {
  UnrestrictedDouble unrestricted_out;
  if (!Convert(field, value, unrestricted_out)) {
    return false;
  }

  if (!std::isfinite(unrestricted_out.number)) {
    MarkFailed(
        base::StrCat({"Converting field '", field,
                      "' to a Number did not produce a finite double."}));
    return false;
  }
  out = unrestricted_out.number;
  return true;
}

bool DictConverter::Convert(base::StringPiece field,
                            v8::Local<v8::Value> value,
                            bool& out) {
  if (value->IsBoolean()) {
    out = value.As<v8::Boolean>()->Value();
  } else {
    out = value->BooleanValue(v8_helper_->isolate());
  }
  return true;
}

bool DictConverter::Convert(base::StringPiece field,
                            v8::Local<v8::Value> value,
                            std::string& out) {
  v8::Isolate* isolate = v8_helper_->isolate();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::String> v8_string;
  if (value->IsString()) {
    v8_string = value.As<v8::String>();
  } else if (!value->ToString(isolate->GetCurrentContext())
                  .ToLocal(&v8_string)) {
    if (try_catch.HasTerminated()) {
      MarkFailedWithTimeout(base::StrCat(
          {"Converting field '", field, "' to String timed out."}));
    } else {
      MarkFailedWithExceptionOrMessage(
          try_catch, base::StrCat({"Trouble converting field '", field,
                                   "' to a String."}));
    }
    return false;
  }

  if (!gin::Converter<std::string>::FromV8(isolate, v8_string, &out)) {
    return false;
  }
  return true;
}

bool DictConverter::ConvertSequence(
    base::StringPiece field,
    v8::Local<v8::Value> value,
    base::RepeatingCallback<bool(v8::Local<v8::Value>)> item_callback) {
  // This is based on https://webidl.spec.whatwg.org/#es-sequence and its
  // implementation in
  // third_party/blink/renderer/bindings/core/v8/script_iterator.cc
  //
  // It's probably best understood from:
  //   https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Iteration_protocols
  v8::Isolate* isolate = v8_helper_->isolate();
  v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
  v8::TryCatch try_catch(isolate);

  // If Type(V) is not Object, throw a TypeError.
  if (!value->IsObject()) {
    MarkFailed(
        base::StrCat({"Sequence field '", field, "' must be an Object."}));
    return false;
  }
  v8::Local<v8::Object> iterable = value.As<v8::Object>();

  v8::Local<v8::String> next_name, done_name, value_name;
  if (!v8_helper_->CreateUtf8String("next").ToLocal(&next_name) ||
      !v8_helper_->CreateUtf8String("done").ToLocal(&done_name) ||
      !v8_helper_->CreateUtf8String("value").ToLocal(&value_name)) {
    MarkFailedIter(field, try_catch);
    return false;
  }

  // Let method be ? GetMethod(V, @@iterator).
  // If method is undefined, throw a TypeError.
  v8::Local<v8::Value> method_val;
  if (!iterable->Get(current_context, v8::Symbol::GetIterator(isolate))
           .ToLocal(&method_val)) {
    MarkFailedIter(field, try_catch);
    return false;
  }
  if (!method_val->IsObject()) {  // subsumes the undefined/null check.
    MarkFailedIter(field, try_catch);
    return false;
  }
  v8::Local<v8::Object> method = method_val.As<v8::Object>();
  if (!method->IsCallable()) {
    MarkFailedIter(field, try_catch);
    return false;
  }

  // Let iter be ? GetIterator(iterable, sync, method)
  v8::Local<v8::Value> iterator_val;
  if (!method
           ->CallAsFunction(current_context, iterable, /*argc=*/0,
                            /*argv=*/nullptr)
           .ToLocal(&iterator_val) ||
      !iterator_val->IsObject()) {
    MarkFailedIter(field, try_catch);
    return false;
  }
  v8::Local<v8::Object> iterator = iterator_val.As<v8::Object>();

  v8::Local<v8::Value> next_method_val;
  if (!iterator->Get(current_context, next_name).ToLocal(&next_method_val) ||
      !next_method_val->IsObject()) {
    MarkFailedIter(field, try_catch);
    return false;
  }
  v8::Local<v8::Object> next_method = next_method_val.As<v8::Object>();
  if (!next_method->IsCallable()) {
    MarkFailedIter(field, try_catch);
    return false;
  }

  size_t out_size = 0;
  while (true) {
    v8::Local<v8::Value> result_val;
    if (!next_method
             ->CallAsFunction(current_context, iterator, /*argc=*/0,
                              /*argv=*/nullptr)
             .ToLocal(&result_val) ||
        !result_val->IsObject()) {
      MarkFailedIter(field, try_catch);
      return false;
    }
    v8::Local<v8::Object> result = result_val.As<v8::Object>();

    bool done = false;  // ToBoolean(undefined)
    v8::Local<v8::Value> done_val;
    if (result->Get(current_context, done_name).ToLocal(&done_val)) {
      // Can use our Convert() since it doesn't fail for bools.
      Convert(field, done_val, done);
    } else if (try_catch.HasCaught() || try_catch.HasTerminated()) {
      MarkFailedIter(field, try_catch);
      return false;
    }

    if (done) {
      break;
    }

    // Check if we're over limit (before actually getting the item that
    // would put us over).
    ++out_size;
    if (out_size > kSequenceLengthLimit) {
      MarkFailed(base::StrCat(
          {"Length limit for sequence field '", field, "' exceeded."}));
      return false;
    }

    v8::Local<v8::Value> next_item;
    if (!result->Get(current_context, value_name).ToLocal(&next_item)) {
      MarkFailedIter(field, try_catch);
      return false;
    }

    if (!item_callback.Run(next_item)) {
      // It's possible that the callback already propagated an error to us;
      // if not, set one ourselves.
      if (!failed_) {
        MarkFailed(base::StrCat({"Conversion for an item for sequence field '",
                                 field, "' failed."}));
      }
      return false;
    }
  }

  return true;
}

bool DictConverter::Convert(base::StringPiece field,
                            v8::Local<v8::Value> value,
                            v8::Local<v8::Value>& out) {
  out = value;
  return true;
}

void DictConverter::MarkFailed(base::StringPiece fail_message) {
  DCHECK(!failed_);
  failed_ = true;
  failure_message_ = base::StrCat({error_prefix_, fail_message});
}

void DictConverter::MarkFailedWithTimeout(base::StringPiece fail_message) {
  failed_with_timeout_ = true;
  MarkFailed(fail_message);
}

void DictConverter::MarkFailedWithException(const v8::TryCatch& catcher) {
  DCHECK(!failed_);
  failed_ = true;
  failure_message_ = v8_helper_->FormatExceptionMessage(
      v8_helper_->isolate()->GetCurrentContext(), catcher.Message());
  failure_exception_ = catcher.Exception();
}

void DictConverter::MarkFailedWithExceptionOrMessage(
    const v8::TryCatch& catcher,
    base::StringPiece fail_message) {
  if (catcher.HasCaught()) {
    MarkFailedWithException(catcher);
  } else {
    MarkFailed(fail_message);
  }
}

void DictConverter::MarkFailedIter(base::StringPiece field,
                                   const v8::TryCatch& catcher) {
  if (catcher.HasTerminated()) {
    MarkFailedWithTimeout(
        base::StrCat({"Timeout iterating over '", field, "'."}));
  } else if (catcher.HasCaught()) {
    MarkFailedWithException(catcher);
  } else {
    MarkFailed(base::StrCat({"Trouble iterating over '", field, "'."}));
  }
}

}  // namespace auction_worklet
