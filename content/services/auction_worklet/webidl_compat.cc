// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/webidl_compat.h"

#include <cmath>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "gin/converter.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-primitive.h"

namespace auction_worklet {

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
      MarkFailed(base::StrCat(
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
                            double& out) {
  v8::Isolate* isolate = v8_helper_->isolate();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Number> number_value;
  if (value->IsNumber()) {
    number_value = value.As<v8::Number>();
  } else if (!value->ToNumber(isolate->GetCurrentContext())
                  .ToLocal(&number_value)) {
    // Converting a non-Number to a Number failed somehow.
    if (try_catch.HasTerminated()) {
      MarkFailed(base::StrCat(
          {"Converting field '", field, "' to Number timed out."}));
    } else if (try_catch.HasCaught()) {
      MarkFailedWithException(try_catch);
    } else {
      MarkFailed(base::StrCat(
          {"Trouble converting field '", field, "' to a Number."}));
    }
    return false;
  }

  out = number_value->Value();

  if (!std::isfinite(out)) {
    MarkFailed(
        base::StrCat({"Converting field '", field,
                      "' to a Number did not produce a finite double."}));
    return false;
  }
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
      MarkFailed(base::StrCat(
          {"Converting field '", field, "' to String timed out."}));
    } else if (try_catch.HasCaught()) {
      MarkFailedWithException(try_catch);
    } else {
      MarkFailed(base::StrCat(
          {"Trouble converting field '", field, "' to a String."}));
    }
    return false;
  }

  if (!gin::Converter<std::string>::FromV8(isolate, v8_string, &out)) {
    return false;
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
  failure_message_ = base::StrCat({error_prefix_, " ", fail_message});
}

void DictConverter::MarkFailedWithException(const v8::TryCatch& catcher) {
  DCHECK(!failed_);
  failed_ = true;
  failure_message_ = v8_helper_->FormatExceptionMessage(
      v8_helper_->isolate()->GetCurrentContext(), catcher.Message());
  failure_exception_ = catcher.Exception();
}

}  // namespace auction_worklet
