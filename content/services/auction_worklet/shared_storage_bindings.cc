// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/shared_storage_bindings.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/auction_shared_storage_host.mojom.h"
#include "content/services/auction_worklet/webidl_compat.h"
#include "gin/converter.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"

namespace auction_worklet {

namespace {

constexpr char kPermissionsPolicyError[] =
    "The \"shared-storage\" Permissions Policy denied the method on "
    "sharedStorage";

}  // namespace

SharedStorageBindings::SharedStorageBindings(
    AuctionV8Helper* v8_helper,
    mojom::AuctionSharedStorageHost* shared_storage_host,
    mojom::AuctionWorkletFunction source_auction_worklet_function,
    bool shared_storage_permissions_policy_allowed)
    : v8_helper_(v8_helper),
      shared_storage_host_(shared_storage_host),
      source_auction_worklet_function_(source_auction_worklet_function),
      shared_storage_permissions_policy_allowed_(
          shared_storage_permissions_policy_allowed) {
  DCHECK_EQ(!!shared_storage_host_, shared_storage_permissions_policy_allowed_);
}

SharedStorageBindings::~SharedStorageBindings() = default;

void SharedStorageBindings::AttachToContext(v8::Local<v8::Context> context) {
  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);

  v8::Local<v8::Object> shared_storage = v8::Object::New(v8_helper_->isolate());

  v8::Local<v8::Function> set_method_function =
      v8::Function::New(context, &SharedStorageBindings::Set, v8_this)
          .ToLocalChecked();
  shared_storage
      ->Set(context, v8_helper_->CreateStringFromLiteral("set"),
            set_method_function)
      .Check();

  v8::Local<v8::Function> append_method_function =
      v8::Function::New(context, &SharedStorageBindings::Append, v8_this)
          .ToLocalChecked();
  shared_storage
      ->Set(context, v8_helper_->CreateStringFromLiteral("append"),
            append_method_function)
      .Check();

  v8::Local<v8::Function> delete_method_function =
      v8::Function::New(context, &SharedStorageBindings::Delete, v8_this)
          .ToLocalChecked();
  shared_storage
      ->Set(context, v8_helper_->CreateStringFromLiteral("delete"),
            delete_method_function)
      .Check();

  v8::Local<v8::Function> clear_method_function =
      v8::Function::New(context, &SharedStorageBindings::Clear, v8_this)
          .ToLocalChecked();
  shared_storage
      ->Set(context, v8_helper_->CreateStringFromLiteral("clear"),
            clear_method_function)
      .Check();

  context->Global()
      ->Set(context, v8_helper_->CreateStringFromLiteral("sharedStorage"),
            shared_storage)
      .Check();
}

void SharedStorageBindings::Reset() {}

// static
void SharedStorageBindings::Set(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  SharedStorageBindings* bindings = static_cast<SharedStorageBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;
  v8::Isolate* isolate = v8_helper->isolate();

  if (!bindings->shared_storage_permissions_policy_allowed_) {
    isolate->ThrowException(v8::Exception::TypeError(
        gin::StringToV8(isolate, kPermissionsPolicyError)));
    return;
  }

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(v8_helper, time_limit_scope,
                               "sharedStorage.set(): ", &args,
                               /*min_required_args=*/2);
  std::u16string arg0_key;
  std::u16string arg1_value;
  args_converter.ConvertArg(0, "key", arg0_key);
  args_converter.ConvertArg(1, "value", arg1_value);

  std::optional<bool> ignore_if_present;
  if (args_converter.is_success() && args.Length() > 2) {
    DictConverter options_dict_converter(
        v8_helper, time_limit_scope, "sharedStorage.set 'options' argument ",
        args[2]);
    options_dict_converter.GetOptional("ignoreIfPresent", ignore_if_present);
    args_converter.SetStatus(options_dict_converter.TakeStatus());
  }

  if (args_converter.is_failed()) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return;
  }

  // IDL portions of checking done, now do semantic checking.
  if (!blink::IsValidSharedStorageKeyStringLength(arg0_key.size())) {
    isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
        isolate, "Invalid 'key' argument in sharedStorage.set()")));
    return;
  }

  if (!blink::IsValidSharedStorageValueStringLength(arg1_value.size())) {
    isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
        isolate, "Invalid 'value' argument in sharedStorage.set()")));
    return;
  }

  bindings->shared_storage_host_->Set(
      arg0_key, arg1_value, ignore_if_present.value_or(false),
      bindings->source_auction_worklet_function_);
}

// static
void SharedStorageBindings::Append(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  SharedStorageBindings* bindings = static_cast<SharedStorageBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;
  v8::Isolate* isolate = v8_helper->isolate();

  if (!bindings->shared_storage_permissions_policy_allowed_) {
    isolate->ThrowException(v8::Exception::TypeError(
        gin::StringToV8(isolate, kPermissionsPolicyError)));
    return;
  }

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(v8_helper, time_limit_scope,
                               "sharedStorage.append(): ", &args,
                               /*min_required_args=*/2);

  std::u16string arg0_key;
  std::u16string arg1_value;
  if (!args_converter.ConvertArg(0, "key", arg0_key) ||
      !args_converter.ConvertArg(1, "value", arg1_value)) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return;
  }

  // IDL portions of checking done, now do semantic checking.
  if (!blink::IsValidSharedStorageKeyStringLength(arg0_key.size())) {
    isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
        isolate, "Invalid 'key' argument in sharedStorage.append()")));
    return;
  }

  if (!blink::IsValidSharedStorageValueStringLength(arg1_value.size())) {
    isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
        isolate, "Invalid 'value' argument in sharedStorage.append()")));
    return;
  }

  bindings->shared_storage_host_->Append(
      arg0_key, arg1_value, bindings->source_auction_worklet_function_);
}

// static
void SharedStorageBindings::Delete(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  SharedStorageBindings* bindings = static_cast<SharedStorageBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;
  v8::Isolate* isolate = v8_helper->isolate();

  if (!bindings->shared_storage_permissions_policy_allowed_) {
    isolate->ThrowException(v8::Exception::TypeError(
        gin::StringToV8(isolate, kPermissionsPolicyError)));
    return;
  }

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(v8_helper, time_limit_scope,
                               "sharedStorage.delete(): ", &args,
                               /*min_required_args=*/1);

  std::u16string arg0_key;
  if (!args_converter.ConvertArg(0, "key", arg0_key)) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return;
  }

  // IDL portions of checking done, now do semantic checking.
  if (!blink::IsValidSharedStorageKeyStringLength(arg0_key.size())) {
    isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
        isolate, "Invalid 'key' argument in sharedStorage.delete()")));
    return;
  }

  bindings->shared_storage_host_->Delete(
      arg0_key, bindings->source_auction_worklet_function_);
}

// static
void SharedStorageBindings::Clear(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  SharedStorageBindings* bindings = static_cast<SharedStorageBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;
  v8::Isolate* isolate = v8_helper->isolate();

  if (!bindings->shared_storage_permissions_policy_allowed_) {
    isolate->ThrowException(v8::Exception::TypeError(
        gin::StringToV8(isolate, kPermissionsPolicyError)));
    return;
  }

  bindings->shared_storage_host_->Clear(
      bindings->source_auction_worklet_function_);
}
}  // namespace auction_worklet
