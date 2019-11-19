// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/v8_value_converter_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "v8/include/v8.h"

namespace content {

// Default implementation of V8ValueConverter::Strategy

bool V8ValueConverter::Strategy::FromV8Object(v8::Local<v8::Object> value,
                                              std::unique_ptr<base::Value>* out,
                                              v8::Isolate* isolate) {
  return false;
}

bool V8ValueConverter::Strategy::FromV8Array(v8::Local<v8::Array> value,
                                             std::unique_ptr<base::Value>* out,
                                             v8::Isolate* isolate) {
  return false;
}

bool V8ValueConverter::Strategy::FromV8ArrayBuffer(
    v8::Local<v8::Object> value,
    std::unique_ptr<base::Value>* out,
    v8::Isolate* isolate) {
  return false;
}

bool V8ValueConverter::Strategy::FromV8Number(
    v8::Local<v8::Number> value,
    std::unique_ptr<base::Value>* out) {
  return false;
}

bool V8ValueConverter::Strategy::FromV8Undefined(
    std::unique_ptr<base::Value>* out) {
  return false;
}

namespace {

// For the sake of the storage API, make this quite large.
const int kMaxRecursionDepth = 100;

}  // namespace

// The state of a call to FromV8Value.
class V8ValueConverterImpl::FromV8ValueState {
 public:
  // Level scope which updates the current depth of some FromV8ValueState.
  class Level {
   public:
    explicit Level(FromV8ValueState* state) : state_(state) {
      state_->max_recursion_depth_--;
    }
    ~Level() {
      state_->max_recursion_depth_++;
    }

   private:
    FromV8ValueState* state_;
  };

  explicit FromV8ValueState(bool avoid_identity_hash_for_testing)
      : max_recursion_depth_(kMaxRecursionDepth),
        avoid_identity_hash_for_testing_(avoid_identity_hash_for_testing) {}

  // If |handle| is not in |unique_map_|, then add it to |unique_map_| and
  // return true.
  //
  // Otherwise do nothing and return false. Here "A is unique" means that no
  // other handle B in the map points to the same object as A. Note that A can
  // be unique even if there already is another handle with the same identity
  // hash (key) in the map, because two objects can have the same hash.
  bool AddToUniquenessCheck(v8::Local<v8::Object> handle) {
    int hash;
    auto iter = GetIteratorInMap(handle, &hash);
    if (iter != unique_map_.end())
      return false;

    unique_map_.insert(std::make_pair(hash, handle));
    return true;
  }

  bool RemoveFromUniquenessCheck(v8::Local<v8::Object> handle) {
    int unused_hash;
    auto iter = GetIteratorInMap(handle, &unused_hash);
    if (iter == unique_map_.end())
      return false;
    unique_map_.erase(iter);
    return true;
  }

  bool HasReachedMaxRecursionDepth() {
    return max_recursion_depth_ < 0;
  }

 private:
  using HashToHandleMap = std::multimap<int, v8::Local<v8::Object>>;
  using Iterator = HashToHandleMap::const_iterator;

  Iterator GetIteratorInMap(v8::Local<v8::Object> handle, int* hash) {
    *hash = avoid_identity_hash_for_testing_ ? 0 : handle->GetIdentityHash();
    // We only compare using == with handles to objects with the same identity
    // hash. Different hash obviously means different objects, but two objects
    // in a couple of thousands could have the same identity hash.
    std::pair<Iterator, Iterator> range = unique_map_.equal_range(*hash);
    for (auto it = range.first; it != range.second; ++it) {
      // Operator == for handles actually compares the underlying objects.
      if (it->second == handle)
        return it;
    }
    // Not found.
    return unique_map_.end();
  }

  HashToHandleMap unique_map_;

  int max_recursion_depth_;

  bool avoid_identity_hash_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(FromV8ValueState);
};

// A class to ensure that objects/arrays that are being converted by
// this V8ValueConverterImpl do not have cycles.
//
// An example of cycle: var v = {}; v = {key: v};
// Not an example of cycle: var v = {}; a = [v, v]; or w = {a: v, b: v};
class V8ValueConverterImpl::ScopedUniquenessGuard {
 public:
  ScopedUniquenessGuard(V8ValueConverterImpl::FromV8ValueState* state,
                        v8::Local<v8::Object> value)
      : state_(state),
        value_(value),
        is_valid_(state_->AddToUniquenessCheck(value_)) {}
  ~ScopedUniquenessGuard() {
    if (is_valid_) {
      bool removed = state_->RemoveFromUniquenessCheck(value_);
      DCHECK(removed);
    }
  }

  bool is_valid() const { return is_valid_; }

 private:
  typedef std::multimap<int, v8::Local<v8::Object> > HashToHandleMap;
  V8ValueConverterImpl::FromV8ValueState* state_;
  v8::Local<v8::Object> value_;
  bool is_valid_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUniquenessGuard);
};

std::unique_ptr<V8ValueConverter> V8ValueConverter::Create() {
  return std::make_unique<V8ValueConverterImpl>();
}

V8ValueConverterImpl::V8ValueConverterImpl()
    : date_allowed_(false),
      reg_exp_allowed_(false),
      function_allowed_(false),
      strip_null_from_objects_(false),
      convert_negative_zero_to_int_(false),
      avoid_identity_hash_for_testing_(false),
      strategy_(nullptr) {}

void V8ValueConverterImpl::SetDateAllowed(bool val) {
  date_allowed_ = val;
}

void V8ValueConverterImpl::SetRegExpAllowed(bool val) {
  reg_exp_allowed_ = val;
}

void V8ValueConverterImpl::SetFunctionAllowed(bool val) {
  function_allowed_ = val;
}

void V8ValueConverterImpl::SetStripNullFromObjects(bool val) {
  strip_null_from_objects_ = val;
}

void V8ValueConverterImpl::SetConvertNegativeZeroToInt(bool val) {
  convert_negative_zero_to_int_ = val;
}

void V8ValueConverterImpl::SetStrategy(Strategy* strategy) {
  strategy_ = strategy;
}

v8::Local<v8::Value> V8ValueConverterImpl::ToV8Value(
    const base::Value* value,
    v8::Local<v8::Context> context) {
  v8::Context::Scope context_scope(context);
  v8::EscapableHandleScope handle_scope(context->GetIsolate());
  return handle_scope.Escape(
      ToV8ValueImpl(context->GetIsolate(), context->Global(), value));
}

std::unique_ptr<base::Value> V8ValueConverterImpl::FromV8Value(
    v8::Local<v8::Value> val,
    v8::Local<v8::Context> context) {
  v8::Context::Scope context_scope(context);
  v8::HandleScope handle_scope(context->GetIsolate());
  FromV8ValueState state(avoid_identity_hash_for_testing_);
  return FromV8ValueImpl(&state, val, context->GetIsolate());
}

v8::Local<v8::Value> V8ValueConverterImpl::ToV8ValueImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const base::Value* value) const {
  CHECK(value);
  switch (value->type()) {
    case base::Value::Type::NONE:
      return v8::Null(isolate);

    case base::Value::Type::BOOLEAN: {
      bool val = false;
      CHECK(value->GetAsBoolean(&val));
      return v8::Boolean::New(isolate, val);
    }

    case base::Value::Type::INTEGER: {
      int val = 0;
      CHECK(value->GetAsInteger(&val));
      return v8::Integer::New(isolate, val);
    }

    case base::Value::Type::DOUBLE: {
      double val = 0.0;
      CHECK(value->GetAsDouble(&val));
      return v8::Number::New(isolate, val);
    }

    case base::Value::Type::STRING: {
      std::string val;
      CHECK(value->GetAsString(&val));
      return v8::String::NewFromUtf8(isolate, val.c_str(),
                                     v8::NewStringType::kNormal, val.length())
          .ToLocalChecked();
    }

    case base::Value::Type::LIST:
      return ToV8Array(isolate,
                       creation_context,
                       static_cast<const base::ListValue*>(value));

    case base::Value::Type::DICTIONARY:
      return ToV8Object(isolate,
                        creation_context,
                        static_cast<const base::DictionaryValue*>(value));

    case base::Value::Type::BINARY:
      return ToArrayBuffer(isolate, creation_context, value);

    default:
      LOG(ERROR) << "Unexpected value type: " << value->type();
      return v8::Null(isolate);
  }
}

v8::Local<v8::Value> V8ValueConverterImpl::ToV8Array(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const base::ListValue* val) const {
  v8::Local<v8::Array> result(v8::Array::New(isolate, val->GetSize()));

  // TODO(robwu): Callers should pass in the context.
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  for (size_t i = 0; i < val->GetSize(); ++i) {
    const base::Value* child = nullptr;
    CHECK(val->Get(i, &child));

    v8::Local<v8::Value> child_v8 =
        ToV8ValueImpl(isolate, creation_context, child);
    CHECK(!child_v8.IsEmpty());

    v8::Maybe<bool> maybe =
        result->CreateDataProperty(context, static_cast<uint32_t>(i), child_v8);
    if (!maybe.IsJust() || !maybe.FromJust())
      LOG(ERROR) << "Failed to set value at index " << i;
  }

  return result;
}

v8::Local<v8::Value> V8ValueConverterImpl::ToV8Object(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const base::DictionaryValue* val) const {
  v8::Local<v8::Object> result(v8::Object::New(isolate));

  // TODO(robwu): Callers should pass in the context.
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  for (base::DictionaryValue::Iterator iter(*val);
       !iter.IsAtEnd(); iter.Advance()) {
    const std::string& key = iter.key();
    v8::Local<v8::Value> child_v8 =
        ToV8ValueImpl(isolate, creation_context, &iter.value());
    CHECK(!child_v8.IsEmpty());

    v8::Maybe<bool> maybe = result->CreateDataProperty(
        context,
        v8::String::NewFromUtf8(isolate, key.c_str(),
                                v8::NewStringType::kNormal, key.length())
            .ToLocalChecked(),
        child_v8);
    if (!maybe.IsJust() || !maybe.FromJust())
      LOG(ERROR) << "Failed to set property with key " << key;
  }

  return result;
}

v8::Local<v8::Value> V8ValueConverterImpl::ToArrayBuffer(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const base::Value* value) const {
  DCHECK(creation_context->CreationContext() == isolate->GetCurrentContext());
  v8::Local<v8::ArrayBuffer> buffer =
      v8::ArrayBuffer::New(isolate, value->GetBlob().size());
  memcpy(buffer->GetContents().Data(), value->GetBlob().data(),
         value->GetBlob().size());
  return buffer;
}

std::unique_ptr<base::Value> V8ValueConverterImpl::FromV8ValueImpl(
    FromV8ValueState* state,
    v8::Local<v8::Value> val,
    v8::Isolate* isolate) const {
  CHECK(!val.IsEmpty());

  FromV8ValueState::Level state_level(state);
  if (state->HasReachedMaxRecursionDepth())
    return nullptr;

  if (val->IsNull())
    return std::make_unique<base::Value>();

  if (val->IsBoolean())
    return std::make_unique<base::Value>(val->ToBoolean(isolate)->Value());

  if (val->IsNumber() && strategy_) {
    std::unique_ptr<base::Value> out;
    if (strategy_->FromV8Number(val.As<v8::Number>(), &out))
      return out;
  }

  if (val->IsInt32())
    return std::make_unique<base::Value>(val.As<v8::Int32>()->Value());

  if (val->IsNumber()) {
    double val_as_double = val.As<v8::Number>()->Value();
    if (!std::isfinite(val_as_double))
      return nullptr;
    // Normally, this would be an integer, and fall into IsInt32(). But if the
    // value is -0, it's treated internally as a double. Consumers are allowed
    // to ignore this esoterica and treat it as an integer.
    if (convert_negative_zero_to_int_ && val_as_double == 0.0)
      return std::make_unique<base::Value>(0);
    return std::make_unique<base::Value>(val_as_double);
  }

  if (val->IsString()) {
    v8::String::Utf8Value utf8(isolate, val);
    return std::make_unique<base::Value>(std::string(*utf8, utf8.length()));
  }

  if (val->IsUndefined()) {
    if (strategy_) {
      std::unique_ptr<base::Value> out;
      if (strategy_->FromV8Undefined(&out))
        return out;
    }
    // JSON.stringify ignores undefined.
    return nullptr;
  }

  if (val->IsDate()) {
    if (!date_allowed_)
      // JSON.stringify would convert this to a string, but an object is more
      // consistent within this class.
      return FromV8Object(val.As<v8::Object>(), state, isolate);
    v8::Date* date = v8::Date::Cast(*val);
    return std::make_unique<base::Value>(date->ValueOf() / 1000.0);
  }

  if (val->IsRegExp()) {
    if (!reg_exp_allowed_)
      // JSON.stringify converts to an object.
      return FromV8Object(val.As<v8::Object>(), state, isolate);
    return std::make_unique<base::Value>(*v8::String::Utf8Value(isolate, val));
  }

  // v8::Value doesn't have a ToArray() method for some reason.
  if (val->IsArray())
    return FromV8Array(val.As<v8::Array>(), state, isolate);

  if (val->IsFunction()) {
    if (!function_allowed_)
      // JSON.stringify refuses to convert function(){}.
      return nullptr;
    return FromV8Object(val.As<v8::Object>(), state, isolate);
  }

  if (val->IsArrayBuffer() || val->IsArrayBufferView())
    return FromV8ArrayBuffer(val.As<v8::Object>(), isolate);

  if (val->IsObject())
    return FromV8Object(val.As<v8::Object>(), state, isolate);

  LOG(ERROR) << "Unexpected v8 value type encountered.";
  return nullptr;
}

std::unique_ptr<base::Value> V8ValueConverterImpl::FromV8Array(
    v8::Local<v8::Array> val,
    FromV8ValueState* state,
    v8::Isolate* isolate) const {
  ScopedUniquenessGuard uniqueness_guard(state, val);
  if (!uniqueness_guard.is_valid())
    return std::make_unique<base::Value>();

  std::unique_ptr<v8::Context::Scope> scope;
  // If val was created in a different context than our current one, change to
  // that context, but change back after val is converted.
  if (!val->CreationContext().IsEmpty() &&
      val->CreationContext() != isolate->GetCurrentContext())
    scope.reset(new v8::Context::Scope(val->CreationContext()));

  if (strategy_) {
    std::unique_ptr<base::Value> out;
    if (strategy_->FromV8Array(val, &out, isolate))
      return out;
  }

  std::unique_ptr<base::ListValue> result(new base::ListValue());

  // Only fields with integer keys are carried over to the ListValue.
  for (uint32_t i = 0; i < val->Length(); ++i) {
    v8::TryCatch try_catch(isolate);
    v8::Local<v8::Value> child_v8;
    v8::MaybeLocal<v8::Value> maybe_child =
        val->Get(isolate->GetCurrentContext(), i);
    if (try_catch.HasCaught() || !maybe_child.ToLocal(&child_v8)) {
      LOG(ERROR) << "Getter for index " << i << " threw an exception.";
      child_v8 = v8::Null(isolate);
    }

    if (!val->HasRealIndexedProperty(isolate->GetCurrentContext(), i)
             .FromMaybe(false)) {
      result->Append(std::make_unique<base::Value>());
      continue;
    }

    std::unique_ptr<base::Value> child =
        FromV8ValueImpl(state, child_v8, isolate);
    if (child)
      result->Append(std::move(child));
    else
      // JSON.stringify puts null in places where values don't serialize, for
      // example undefined and functions. Emulate that behavior.
      result->Append(std::make_unique<base::Value>());
  }
  return std::move(result);
}

std::unique_ptr<base::Value> V8ValueConverterImpl::FromV8ArrayBuffer(
    v8::Local<v8::Object> val,
    v8::Isolate* isolate) const {
  if (strategy_) {
    std::unique_ptr<base::Value> out;
    if (strategy_->FromV8ArrayBuffer(val, &out, isolate))
      return out;
  }

  if (val->IsArrayBuffer()) {
    auto contents = val.As<v8::ArrayBuffer>()->GetContents();
    return base::Value::CreateWithCopiedBuffer(
        static_cast<const char*>(contents.Data()), contents.ByteLength());
  } else if (val->IsArrayBufferView()) {
    v8::Local<v8::ArrayBufferView> view = val.As<v8::ArrayBufferView>();
    size_t byte_length = view->ByteLength();
    std::vector<char> buffer(byte_length);
    view->CopyContents(buffer.data(), buffer.size());
    return std::make_unique<base::Value>(std::move(buffer));
  } else {
    NOTREACHED() << "Only ArrayBuffer and ArrayBufferView should get here.";
    return nullptr;
  }
}

std::unique_ptr<base::Value> V8ValueConverterImpl::FromV8Object(
    v8::Local<v8::Object> val,
    FromV8ValueState* state,
    v8::Isolate* isolate) const {
  ScopedUniquenessGuard uniqueness_guard(state, val);
  if (!uniqueness_guard.is_valid())
    return std::make_unique<base::Value>();

  std::unique_ptr<v8::Context::Scope> scope;
  // If val was created in a different context than our current one, change to
  // that context, but change back after val is converted.
  if (!val->CreationContext().IsEmpty() &&
      val->CreationContext() != isolate->GetCurrentContext())
    scope.reset(new v8::Context::Scope(val->CreationContext()));

  if (strategy_) {
    std::unique_ptr<base::Value> out;
    if (strategy_->FromV8Object(val, &out, isolate))
      return out;
  }

  // Don't consider DOM objects. This check matches isHostObject() in Blink's
  // bindings/v8/V8Binding.h used in structured cloning. It reads:
  //
  // If the object has any internal fields, then we won't be able to serialize
  // or deserialize them; conveniently, this is also a quick way to detect DOM
  // wrapper objects, because the mechanism for these relies on data stored in
  // these fields.
  //
  // NOTE: check this after |strategy_| so that callers have a chance to
  // do something else, such as convert to the node's name rather than NULL.
  //
  // ANOTHER NOTE: returning an empty dictionary here to minimise surprise.
  // See also http://crbug.com/330559.
  if (val->InternalFieldCount())
    return std::make_unique<base::DictionaryValue>();

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  v8::Local<v8::Array> property_names;
  if (!val->GetOwnPropertyNames(isolate->GetCurrentContext())
           .ToLocal(&property_names)) {
    return std::move(result);
  }

  for (uint32_t i = 0; i < property_names->Length(); ++i) {
    v8::Local<v8::Value> key =
        property_names->Get(isolate->GetCurrentContext(), i).ToLocalChecked();

    // Extend this test to cover more types as necessary and if sensible.
    if (!key->IsString() &&
        !key->IsNumber()) {
      NOTREACHED() << "Key \"" << *v8::String::Utf8Value(isolate, key)
                   << "\" "
                      "is neither a string nor a number";
      continue;
    }

    v8::String::Utf8Value name_utf8(isolate, key);

    v8::TryCatch try_catch(isolate);
    v8::Local<v8::Value> child_v8;
    v8::MaybeLocal<v8::Value> maybe_child =
        val->Get(isolate->GetCurrentContext(), key);
    if (try_catch.HasCaught() || !maybe_child.ToLocal(&child_v8)) {
      LOG(WARNING) << "Getter for property " << *name_utf8
                   << " threw an exception.";
      child_v8 = v8::Null(isolate);
    }

    std::unique_ptr<base::Value> child =
        FromV8ValueImpl(state, child_v8, isolate);
    if (!child)
      // JSON.stringify skips properties whose values don't serialize, for
      // example undefined and functions. Emulate that behavior.
      continue;

    // Strip null if asked (and since undefined is turned into null, undefined
    // too). The use case for supporting this is JSON-schema support,
    // specifically for extensions, where "optional" JSON properties may be
    // represented as null, yet due to buggy legacy code elsewhere isn't
    // treated as such (potentially causing crashes). For example, the
    // "tabs.create" function takes an object as its first argument with an
    // optional "windowId" property.
    //
    // Given just
    //
    //   tabs.create({})
    //
    // this will work as expected on code that only checks for the existence of
    // a "windowId" property (such as that legacy code). However given
    //
    //   tabs.create({windowId: null})
    //
    // there *is* a "windowId" property, but since it should be an int, code
    // on the browser which doesn't additionally check for null will fail.
    // We can avoid all bugs related to this by stripping null.
    if (strip_null_from_objects_ && child->is_none())
      continue;

    result->SetWithoutPathExpansion(std::string(*name_utf8, name_utf8.length()),
                                    std::move(child));
  }

  return std::move(result);
}

}  // namespace content
