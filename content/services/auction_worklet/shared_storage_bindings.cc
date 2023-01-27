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
#include "gin/arguments.h"
#include "gin/dictionary.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-template.h"

namespace auction_worklet {

namespace {

constexpr char kPermissionsPolicyError[] =
    "The \"shared-storage\" Permissions Policy denied the method on "
    "sharedStorage";

// Convert ECMAScript value to IDL DOMString:
// https://webidl.spec.whatwg.org/#es-DOMString
bool ToIDLDOMString(v8::Isolate* isolate,
                    v8::Local<v8::Value> val,
                    std::u16string& out) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);

  v8::TryCatch try_catch(isolate);

  v8::Local<v8::String> str;
  if (!val->ToString(context).ToLocal(&str)) {
    return false;
  }

  return gin::ConvertFromV8<std::u16string>(isolate, str, &out);
}

}  // namespace

SharedStorageBindings::SharedStorageBindings(
    AuctionV8Helper* v8_helper,
    mojom::AuctionSharedStorageHost* shared_storage_host,
    bool shared_storage_permissions_policy_allowed)
    : v8_helper_(v8_helper),
      shared_storage_host_(shared_storage_host),
      shared_storage_permissions_policy_allowed_(
          shared_storage_permissions_policy_allowed) {
  DCHECK_EQ(!!shared_storage_host_, shared_storage_permissions_policy_allowed_);
}

SharedStorageBindings::~SharedStorageBindings() = default;

void SharedStorageBindings::FillInGlobalTemplate(
    v8::Local<v8::ObjectTemplate> global_template) {
  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);

  v8::Local<v8::ObjectTemplate> shared_storage_template =
      v8::ObjectTemplate::New(v8_helper_->isolate());

  v8::Local<v8::FunctionTemplate> set_method_template =
      v8::FunctionTemplate::New(v8_helper_->isolate(),
                                &SharedStorageBindings::Set, v8_this);
  set_method_template->RemovePrototype();
  shared_storage_template->Set(v8_helper_->CreateStringFromLiteral("set"),
                               set_method_template);

  v8::Local<v8::FunctionTemplate> append_method_template =
      v8::FunctionTemplate::New(v8_helper_->isolate(),
                                &SharedStorageBindings::Append, v8_this);
  append_method_template->RemovePrototype();
  shared_storage_template->Set(v8_helper_->CreateStringFromLiteral("append"),
                               append_method_template);

  v8::Local<v8::FunctionTemplate> delete_method_template =
      v8::FunctionTemplate::New(v8_helper_->isolate(),
                                &SharedStorageBindings::Delete, v8_this);
  delete_method_template->RemovePrototype();
  shared_storage_template->Set(v8_helper_->CreateStringFromLiteral("delete"),
                               delete_method_template);

  v8::Local<v8::FunctionTemplate> clear_method_template =
      v8::FunctionTemplate::New(v8_helper_->isolate(),
                                &SharedStorageBindings::Clear, v8_this);
  clear_method_template->RemovePrototype();
  shared_storage_template->Set(v8_helper_->CreateStringFromLiteral("clear"),
                               clear_method_template);

  global_template->Set(v8_helper_->CreateStringFromLiteral("sharedStorage"),
                       shared_storage_template);
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

  gin::Arguments gin_args = gin::Arguments(args);

  std::vector<v8::Local<v8::Value>> v8_args = gin_args.GetAll();

  std::u16string arg0_key;
  if (v8_args.size() < 1 || !ToIDLDOMString(isolate, v8_args[0], arg0_key) ||
      !blink::IsValidSharedStorageKeyStringLength(arg0_key.size())) {
    isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
        isolate,
        "Missing or invalid \"key\" argument in sharedStorage.set()")));
    return;
  }

  std::u16string arg1_value;
  if (v8_args.size() < 2 || !ToIDLDOMString(isolate, v8_args[1], arg1_value) ||
      !blink::IsValidSharedStorageValueStringLength(arg1_value.size())) {
    isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
        isolate,
        "Missing or invalid \"value\" argument in sharedStorage.set()")));
    return;
  }

  gin::Dictionary arg2_options_dict = gin::Dictionary::CreateEmpty(isolate);

  if (v8_args.size() > 2) {
    if (!gin::ConvertFromV8(isolate, v8_args[2], &arg2_options_dict)) {
      isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
          isolate, "Invalid \"options\" argument in sharedStorage.set()")));
      return;
    }
  }

  bool ignore_if_present = false;
  arg2_options_dict.Get<bool>("ignoreIfPresent", &ignore_if_present);

  bindings->shared_storage_host_->Set(arg0_key, arg1_value, ignore_if_present);
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

  gin::Arguments gin_args = gin::Arguments(args);

  std::vector<v8::Local<v8::Value>> v8_args = gin_args.GetAll();

  std::u16string arg0_key;
  if (v8_args.size() < 1 || !ToIDLDOMString(isolate, v8_args[0], arg0_key) ||
      !blink::IsValidSharedStorageKeyStringLength(arg0_key.size())) {
    isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
        isolate,
        "Missing or invalid \"key\" argument in sharedStorage.append()")));
    return;
  }

  std::u16string arg1_value;
  if (v8_args.size() < 2 || !ToIDLDOMString(isolate, v8_args[1], arg1_value) ||
      !blink::IsValidSharedStorageValueStringLength(arg1_value.size())) {
    isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
        isolate,
        "Missing or invalid \"value\" argument in sharedStorage.append()")));
    return;
  }

  bindings->shared_storage_host_->Append(arg0_key, arg1_value);
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

  gin::Arguments gin_args = gin::Arguments(args);

  std::vector<v8::Local<v8::Value>> v8_args = gin_args.GetAll();

  std::u16string arg0_key;
  if (v8_args.size() < 1 || !ToIDLDOMString(isolate, v8_args[0], arg0_key) ||
      !blink::IsValidSharedStorageKeyStringLength(arg0_key.size())) {
    isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
        isolate,
        "Missing or invalid \"key\" argument in sharedStorage.delete()")));
    return;
  }

  bindings->shared_storage_host_->Delete(arg0_key);
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

  bindings->shared_storage_host_->Clear();
}
}  // namespace auction_worklet
