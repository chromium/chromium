/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/bindings/v8_value_cache.h"

#include <utility>
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/string_resource.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

StringCacheMapTraits::MapType* StringCacheMapTraits::MapFromWeakCallbackInfo(
    const v8::WeakCallbackInfo<WeakCallbackDataType>& data) {
  return &(V8PerIsolateData::From(data.GetIsolate())
               ->GetStringCache()
               ->string_cache_);
}

void StringCacheMapTraits::Dispose(v8::Isolate* isolate,
                                   v8::Global<v8::String> value,
                                   StringImpl* key) {
  key->Release();
}

void StringCacheMapTraits::DisposeWeak(
    const v8::WeakCallbackInfo<WeakCallbackDataType>& data) {
  data.GetParameter()->Release();
}

ParkableStringCacheMapTraits::MapType*
ParkableStringCacheMapTraits::MapFromWeakCallbackInfo(
    const v8::WeakCallbackInfo<WeakCallbackDataType>& data) {
  return &(V8PerIsolateData::From(data.GetIsolate())
               ->GetStringCache()
               ->parkable_string_cache_);
}

void ParkableStringCacheMapTraits::Dispose(v8::Isolate* isolate,
                                           v8::Global<v8::String> value,
                                           ParkableStringImpl* key) {
  key->Release();
}

void ParkableStringCacheMapTraits::DisposeWeak(
    const v8::WeakCallbackInfo<WeakCallbackDataType>& data) {
  data.GetParameter()->Release();
}

void ParkableStringCacheMapTraits::OnWeakCallback(
    const v8::WeakCallbackInfo<WeakCallbackDataType>& data) {}

void StringCache::Dispose() {
  // The MapType::Dispose callback calls StringCache::InvalidateLastString,
  // which will only work while the destructor has not yet finished. Thus,
  // we need to clear the map before the destructor has completed.
  string_cache_.Clear();
}

static v8::Local<v8::String> MakeExternalString(v8::Isolate* isolate,
                                                String string) {
  if (string.Is8Bit()) {
    StringResource8* string_resource =
        new StringResource8(isolate, std::move(string));
    v8::Local<v8::String> new_string;
    if (!v8::String::NewExternalOneByte(isolate, string_resource)
             .ToLocal(&new_string)) {
      delete string_resource;
      return v8::String::Empty(isolate);
    }
    return new_string;
  }

  StringResource16* string_resource =
      new StringResource16(isolate, std::move(string));
  v8::Local<v8::String> new_string;
  if (!v8::String::NewExternalTwoByte(isolate, string_resource)
           .ToLocal(&new_string)) {
    delete string_resource;
    return v8::String::Empty(isolate);
  }
  return new_string;
}

static v8::Local<v8::String> MakeExternalString(v8::Isolate* isolate,
                                                const ParkableString string) {
  if (string.Is8Bit()) {
    auto* string_resource =
        new ParkableStringResource8(isolate, std::move(string));
    v8::Local<v8::String> new_string;
    if (!v8::String::NewExternalOneByte(isolate, string_resource)
             .ToLocal(&new_string)) {
      delete string_resource;
      return v8::String::Empty(isolate);
    }
    return new_string;
  }

  auto* string_resource =
      new ParkableStringResource16(isolate, std::move(string));
  v8::Local<v8::String> new_string;
  if (!v8::String::NewExternalTwoByte(isolate, string_resource)
           .ToLocal(&new_string)) {
    delete string_resource;
    return v8::String::Empty(isolate);
  }
  return new_string;
}

v8::Local<v8::String> StringCache::V8ExternalString(v8::Isolate* isolate,
                                                    StringImpl* string_impl) {
  DCHECK(string_impl);
  RUNTIME_CALL_TIMER_SCOPE(isolate,
                           RuntimeCallStats::CounterId::kV8ExternalStringSlow);
  if (!string_impl->length())
    return v8::String::Empty(isolate);

  StringCacheMapTraits::MapType::PersistentValueReference cached_v8_string =
      string_cache_.GetReference(string_impl);
  if (!cached_v8_string.IsEmpty()) {
    return cached_v8_string.NewLocal(isolate);
  }

  return CreateStringAndInsertIntoCache(isolate, string_impl);
}

v8::Local<v8::String> StringCache::V8ExternalString(
    v8::Isolate* isolate,
    const ParkableString& string) {
  if (!string.length())
    return v8::String::Empty(isolate);

  ParkableStringCacheMapTraits::MapType::PersistentValueReference
      cached_v8_string = parkable_string_cache_.GetReference(string.Impl());
  if (!cached_v8_string.IsEmpty()) {
    return cached_v8_string.NewLocal(isolate);
  }

  return CreateStringAndInsertIntoCache(isolate, string);
}

void StringCache::SetReturnValueFromString(
    v8::ReturnValue<v8::Value> return_value,
    StringImpl* string_impl) {
  DCHECK(string_impl);
  RUNTIME_CALL_TIMER_SCOPE(
      return_value.GetIsolate(),
      RuntimeCallStats::CounterId::kSetReturnValueFromStringSlow);
  if (!string_impl->length()) {
    return_value.SetEmptyString();
    return;
  }

  StringCacheMapTraits::MapType::PersistentValueReference cached_v8_string =
      string_cache_.GetReference(string_impl);
  if (!cached_v8_string.IsEmpty()) {
    cached_v8_string.SetReturnValue(return_value);
    return;
  }

  return_value.Set(
      CreateStringAndInsertIntoCache(return_value.GetIsolate(), string_impl));
}

v8::Local<v8::String> StringCache::CreateStringAndInsertIntoCache(
    v8::Isolate* isolate,
    StringImpl* string_impl) {
  DCHECK(!string_cache_.Contains(string_impl));
  DCHECK(string_impl->length());

  v8::Local<v8::String> new_string =
      MakeExternalString(isolate, String(string_impl));
  DCHECK(!new_string.IsEmpty());
  DCHECK(new_string->Length());

  string_impl->AddRef();
  string_cache_.Set(string_impl, new_string);

  return new_string;
}

v8::Local<v8::String> StringCache::CreateStringAndInsertIntoCache(
    v8::Isolate* isolate,
    ParkableString string) {
  ParkableStringImpl* string_impl = string.Impl();
  DCHECK(!parkable_string_cache_.Contains(string_impl));
  DCHECK(string_impl->length());

  v8::Local<v8::String> new_string =
      MakeExternalString(isolate, std::move(string));
  DCHECK(!new_string.IsEmpty());
  DCHECK(new_string->Length());

  v8::UniquePersistent<v8::String> wrapper(isolate, new_string);

  string_impl->AddRef();
  // ParkableStringImpl objects are not cache in |string_cache_| or
  // |last_string_impl_|.
  ParkableStringCacheMapTraits::MapType::PersistentValueReference unused;
  parkable_string_cache_.Set(string_impl, std::move(wrapper), &unused);

  return new_string;
}

}  // namespace blink
