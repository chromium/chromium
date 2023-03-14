// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/shared_storage.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "content/services/shared_storage_worklet/shared_storage_iterator.h"
#include "content/services/shared_storage_worklet/worklet_v8_helper.h"
#include "gin/arguments.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "v8/include/v8-exception.h"

namespace shared_storage_worklet {

namespace {

// Convert ECMAScript value to IDL DOMString:
// https://webidl.spec.whatwg.org/#es-DOMString
bool ToIDLDOMString(v8::Isolate* isolate,
                    v8::Local<v8::Value> val,
                    std::u16string& out) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  WorkletV8Helper::HandleScope scope(isolate);
  v8::TryCatch try_catch(isolate);

  v8::Local<v8::String> str;
  if (!val->ToString(context).ToLocal(&str))
    return false;

  return gin::ConvertFromV8<std::u16string>(isolate, str, &out);
}

void LogTimingHistogramForVoidOperation(
    blink::SharedStorageVoidOperation caller,
    base::TimeTicks start_time) {
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  switch (caller) {
    case blink::SharedStorageVoidOperation::kSet:
      base::UmaHistogramMediumTimes("Storage.SharedStorage.Worklet.Timing.Set",
                                    elapsed_time);
      break;
    case blink::SharedStorageVoidOperation::kAppend:
      base::UmaHistogramMediumTimes(
          "Storage.SharedStorage.Worklet.Timing.Append", elapsed_time);
      break;
    case blink::SharedStorageVoidOperation::kDelete:
      base::UmaHistogramMediumTimes(
          "Storage.SharedStorage.Worklet.Timing.Delete", elapsed_time);
      break;
    case blink::SharedStorageVoidOperation::kClear:
      base::UmaHistogramMediumTimes(
          "Storage.SharedStorage.Worklet.Timing.Clear", elapsed_time);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace

SharedStorage::SharedStorage(
    blink::mojom::SharedStorageWorkletServiceClient* client,
    const absl::optional<std::u16string>& embedder_context)
    : client_(client), embedder_context_(embedder_context) {}

SharedStorage::~SharedStorage() = default;

gin::WrapperInfo SharedStorage::kWrapperInfo = {gin::kEmbedderNativeGin};

gin::ObjectTemplateBuilder SharedStorage::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<SharedStorage>::GetObjectTemplateBuilder(isolate)
      .SetMethod("set", &SharedStorage::Set)
      .SetMethod("append", &SharedStorage::Append)
      .SetMethod("delete", &SharedStorage::Delete)
      .SetMethod("clear", &SharedStorage::Clear)
      .SetMethod("get", &SharedStorage::Get)
      .SetMethod("keys", &SharedStorage::Keys)
      .SetMethod("entries", &SharedStorage::Entries)
      .SetMethod("length", &SharedStorage::Length)
      .SetMethod("remainingBudget", &SharedStorage::RemainingBudget)
      .SetProperty("context", &SharedStorage::Context);
}

const char* SharedStorage::GetTypeName() {
  return "SharedStorage";
}

v8::Local<v8::Promise> SharedStorage::Set(gin::Arguments* args) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  v8::Isolate* isolate = args->isolate();

  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(args->GetHolderCreationContext())
          .ToLocalChecked();

  v8::Local<v8::Promise> promise = resolver->GetPromise();

  std::vector<v8::Local<v8::Value>> v8_args = args->GetAll();

  std::u16string arg0_key;
  if (v8_args.size() < 1 || !ToIDLDOMString(isolate, v8_args[0], arg0_key) ||
      !blink::IsValidSharedStorageKeyStringLength(arg0_key.size())) {
    resolver
        ->Reject(
            args->GetHolderCreationContext(),
            gin::StringToV8(
                isolate,
                "Missing or invalid \"key\" argument in sharedStorage.set()"))
        .ToChecked();
    return promise;
  }

  std::u16string arg1_value;
  if (v8_args.size() < 2 || !ToIDLDOMString(isolate, v8_args[1], arg1_value) ||
      !blink::IsValidSharedStorageValueStringLength(arg1_value.size())) {
    resolver
        ->Reject(
            args->GetHolderCreationContext(),
            gin::StringToV8(
                isolate,
                "Missing or invalid \"value\" argument in sharedStorage.set()"))
        .ToChecked();
    return promise;
  }

  gin::Dictionary arg2_options_dict = gin::Dictionary::CreateEmpty(isolate);

  if (v8_args.size() > 2) {
    if (!gin::ConvertFromV8(isolate, v8_args[2], &arg2_options_dict)) {
      resolver
          ->Reject(args->GetHolderCreationContext(),
                   gin::StringToV8(
                       isolate,
                       "Invalid \"options\" argument in sharedStorage.set()"))
          .ToChecked();
      return promise;
    }
  }

  bool ignore_if_present = false;
  arg2_options_dict.Get<bool>("ignoreIfPresent", &ignore_if_present);

  client_->SharedStorageSet(
      arg0_key, arg1_value, ignore_if_present,
      base::BindOnce(&SharedStorage::OnVoidOperationFinished,
                     weak_ptr_factory_.GetWeakPtr(), isolate,
                     v8::Global<v8::Promise::Resolver>(isolate, resolver),
                     blink::SharedStorageVoidOperation::kSet, start_time));

  return promise;
}

v8::Local<v8::Promise> SharedStorage::Append(gin::Arguments* args) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  v8::Isolate* isolate = args->isolate();

  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(args->GetHolderCreationContext())
          .ToLocalChecked();

  v8::Local<v8::Promise> promise = resolver->GetPromise();

  std::vector<v8::Local<v8::Value>> v8_args = args->GetAll();

  std::u16string arg0_key;
  if (v8_args.size() < 1 || !ToIDLDOMString(isolate, v8_args[0], arg0_key) ||
      !blink::IsValidSharedStorageKeyStringLength(arg0_key.size())) {
    resolver
        ->Reject(args->GetHolderCreationContext(),
                 gin::StringToV8(isolate,
                                 "Missing or invalid \"key\" argument in "
                                 "sharedStorage.append()"))
        .ToChecked();
    return promise;
  }

  std::u16string arg1_value;
  if (v8_args.size() < 2 || !ToIDLDOMString(isolate, v8_args[1], arg1_value) ||
      !blink::IsValidSharedStorageValueStringLength(arg1_value.size())) {
    resolver
        ->Reject(args->GetHolderCreationContext(),
                 gin::StringToV8(isolate,
                                 "Missing or invalid \"value\" argument in "
                                 "sharedStorage.append()"))
        .ToChecked();
    return promise;
  }

  client_->SharedStorageAppend(
      arg0_key, arg1_value,
      base::BindOnce(&SharedStorage::OnVoidOperationFinished,
                     weak_ptr_factory_.GetWeakPtr(), isolate,
                     v8::Global<v8::Promise::Resolver>(isolate, resolver),
                     blink::SharedStorageVoidOperation::kAppend, start_time));

  return promise;
}

v8::Local<v8::Promise> SharedStorage::Delete(gin::Arguments* args) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  v8::Isolate* isolate = args->isolate();

  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(args->GetHolderCreationContext())
          .ToLocalChecked();

  v8::Local<v8::Promise> promise = resolver->GetPromise();

  std::vector<v8::Local<v8::Value>> v8_args = args->GetAll();

  std::u16string arg0_key;
  if (v8_args.size() < 1 || !ToIDLDOMString(isolate, v8_args[0], arg0_key) ||
      !blink::IsValidSharedStorageKeyStringLength(arg0_key.size())) {
    resolver
        ->Reject(args->GetHolderCreationContext(),
                 gin::StringToV8(isolate,
                                 "Missing or invalid \"key\" argument in "
                                 "sharedStorage.delete()"))
        .ToChecked();
    return promise;
  }

  client_->SharedStorageDelete(
      arg0_key,
      base::BindOnce(&SharedStorage::OnVoidOperationFinished,
                     weak_ptr_factory_.GetWeakPtr(), isolate,
                     v8::Global<v8::Promise::Resolver>(isolate, resolver),
                     blink::SharedStorageVoidOperation::kDelete, start_time));

  return promise;
}

v8::Local<v8::Promise> SharedStorage::Clear(gin::Arguments* args) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  v8::Isolate* isolate = args->isolate();

  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(args->GetHolderCreationContext())
          .ToLocalChecked();

  v8::Local<v8::Promise> promise = resolver->GetPromise();

  client_->SharedStorageClear(base::BindOnce(
      &SharedStorage::OnVoidOperationFinished, weak_ptr_factory_.GetWeakPtr(),
      isolate, v8::Global<v8::Promise::Resolver>(isolate, resolver),
      blink::SharedStorageVoidOperation::kClear, start_time));

  return promise;
}

v8::Local<v8::Promise> SharedStorage::Get(gin::Arguments* args) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  v8::Isolate* isolate = args->isolate();

  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(args->GetHolderCreationContext())
          .ToLocalChecked();

  v8::Local<v8::Promise> promise = resolver->GetPromise();

  std::vector<v8::Local<v8::Value>> v8_args = args->GetAll();

  std::u16string arg0_key;
  if (v8_args.size() < 1 || !ToIDLDOMString(isolate, v8_args[0], arg0_key) ||
      !blink::IsValidSharedStorageKeyStringLength(arg0_key.size())) {
    resolver
        ->Reject(
            args->GetHolderCreationContext(),
            gin::StringToV8(
                isolate,
                "Missing or invalid \"key\" argument in sharedStorage.get()"))
        .ToChecked();
    return promise;
  }

  client_->SharedStorageGet(
      arg0_key,
      base::BindOnce(&SharedStorage::OnStringRetrievalOperationFinished,
                     weak_ptr_factory_.GetWeakPtr(), isolate,
                     v8::Global<v8::Promise::Resolver>(isolate, resolver),
                     start_time));
  return promise;
}

v8::Local<v8::Object> SharedStorage::Keys(gin::Arguments* args) {
  return (new SharedStorageIterator(SharedStorageIterator::Mode::kKey, client_))
      ->GetWrapper(args->isolate())
      .ToLocalChecked();
}

v8::Local<v8::Object> SharedStorage::Entries(gin::Arguments* args) {
  return (new SharedStorageIterator(SharedStorageIterator::Mode::kKeyValue,
                                    client_))
      ->GetWrapper(args->isolate())
      .ToLocalChecked();
}

v8::Local<v8::Promise> SharedStorage::Length(gin::Arguments* args) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  v8::Isolate* isolate = args->isolate();

  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(args->GetHolderCreationContext())
          .ToLocalChecked();

  v8::Local<v8::Promise> promise = resolver->GetPromise();

  client_->SharedStorageLength(base::BindOnce(
      &SharedStorage::OnLengthOperationFinished, weak_ptr_factory_.GetWeakPtr(),
      isolate, v8::Global<v8::Promise::Resolver>(isolate, resolver),
      start_time));

  return promise;
}

v8::Local<v8::Promise> SharedStorage::RemainingBudget(gin::Arguments* args) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  v8::Isolate* isolate = args->isolate();

  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(args->GetHolderCreationContext())
          .ToLocalChecked();

  v8::Local<v8::Promise> promise = resolver->GetPromise();

  client_->SharedStorageRemainingBudget(base::BindOnce(
      &SharedStorage::OnBudgetOperationFinished, weak_ptr_factory_.GetWeakPtr(),
      isolate, v8::Global<v8::Promise::Resolver>(isolate, resolver),
      start_time));

  return promise;
}

v8::Local<v8::Value> SharedStorage::Context(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();

  if (!embedder_context_) {
    base::UmaHistogramBoolean("Storage.SharedStorage.Worklet.Context.IsDefined",
                              false);
    return v8::Undefined(isolate);
  }

  base::UmaHistogramBoolean("Storage.SharedStorage.Worklet.Context.IsDefined",
                            true);
  return gin::ConvertToV8(isolate, embedder_context_.value());
}

void SharedStorage::OnVoidOperationFinished(
    v8::Isolate* isolate,
    v8::Global<v8::Promise::Resolver> global_resolver,
    blink::SharedStorageVoidOperation caller,
    base::TimeTicks start_time,
    bool success,
    const std::string& error_message) {
  WorkletV8Helper::HandleScope scope(isolate);
  v8::Local<v8::Promise::Resolver> resolver = global_resolver.Get(isolate);
  v8::Local<v8::Context> context = resolver->GetCreationContextChecked();

  if (success) {
    resolver->Resolve(context, v8::Undefined(isolate)).ToChecked();
    LogTimingHistogramForVoidOperation(caller, start_time);
    return;
  }

  resolver->Reject(context, gin::StringToV8(isolate, error_message))
      .ToChecked();
}

void SharedStorage::OnStringRetrievalOperationFinished(
    v8::Isolate* isolate,
    v8::Global<v8::Promise::Resolver> global_resolver,
    base::TimeTicks start_time,
    blink::mojom::SharedStorageGetStatus status,
    const std::string& error_message,
    const std::u16string& result) {
  WorkletV8Helper::HandleScope scope(isolate);
  v8::Local<v8::Promise::Resolver> resolver = global_resolver.Get(isolate);
  v8::Local<v8::Context> context = resolver->GetCreationContextChecked();
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;

  if (status == blink::mojom::SharedStorageGetStatus::kSuccess) {
    resolver->Resolve(context, gin::ConvertToV8(isolate, result)).ToChecked();
    base::UmaHistogramMediumTimes("Storage.SharedStorage.Worklet.Timing.Get",
                                  elapsed_time);
    return;
  }

  if (status == blink::mojom::SharedStorageGetStatus::kNotFound) {
    resolver->Resolve(context, v8::Undefined(isolate)).ToChecked();
    base::UmaHistogramMediumTimes("Storage.SharedStorage.Worklet.Timing.Get",
                                  elapsed_time);
    return;
  }

  resolver->Reject(context, gin::StringToV8(isolate, error_message))
      .ToChecked();
}

void SharedStorage::OnLengthOperationFinished(
    v8::Isolate* isolate,
    v8::Global<v8::Promise::Resolver> global_resolver,
    base::TimeTicks start_time,
    bool success,
    const std::string& error_message,
    uint32_t length) {
  WorkletV8Helper::HandleScope scope(isolate);
  v8::Local<v8::Promise::Resolver> resolver = global_resolver.Get(isolate);
  v8::Local<v8::Context> context = resolver->GetCreationContextChecked();

  if (success) {
    resolver->Resolve(context, gin::Converter<uint32_t>::ToV8(isolate, length))
        .ToChecked();
    base::UmaHistogramMediumTimes("Storage.SharedStorage.Worklet.Timing.Length",
                                  base::TimeTicks::Now() - start_time);
    return;
  }

  resolver->Reject(context, gin::StringToV8(isolate, error_message))
      .ToChecked();
}

void SharedStorage::OnBudgetOperationFinished(
    v8::Isolate* isolate,
    v8::Global<v8::Promise::Resolver> global_resolver,
    base::TimeTicks start_time,
    bool success,
    const std::string& error_message,
    double bits) {
  WorkletV8Helper::HandleScope scope(isolate);
  v8::Local<v8::Promise::Resolver> resolver = global_resolver.Get(isolate);
  v8::Local<v8::Context> context = resolver->GetCreationContextChecked();

  if (success) {
    resolver->Resolve(context, gin::Converter<double>::ToV8(isolate, bits))
        .ToChecked();
    base::UmaHistogramMediumTimes(
        "Storage.SharedStorage.Worklet.Timing.RemainingBudget",
        base::TimeTicks::Now() - start_time);
    return;
  }

  resolver->Reject(context, gin::StringToV8(isolate, error_message))
      .ToChecked();
}

}  // namespace shared_storage_worklet
