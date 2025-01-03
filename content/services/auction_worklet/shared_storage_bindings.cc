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
#include "services/network/public/cpp/shared_storage_utils.h"
#include "services/network/public/mojom/shared_storage.mojom.h"
#include "third_party/blink/public/common/features.h"
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

constexpr char kNotAConstructorError[] =
    "The shared storage method object constructor cannot be called as a "
    "function";

constexpr char kSharedStorageSetMethodName[] = "SharedStorageSetMethod";
constexpr char kSharedStorageAppendMethodName[] = "SharedStorageAppendMethod";
constexpr char kSharedStorageDeleteMethodName[] = "SharedStorageDeleteMethod";
constexpr char kSharedStorageClearMethodName[] = "SharedStorageClearMethod";

network::mojom::SharedStorageModifierMethodWithOptionsPtr
CreateMojomSetMethodFromParameters(
    AuctionV8Helper* v8_helper,
    const v8::FunctionCallbackInfo<v8::Value>& args,
    bool shared_storage_permissions_policy_allowed,
    const std::string& function_name) {
  v8::Isolate* isolate = v8_helper->isolate();

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(v8_helper, time_limit_scope,
                               base::StrCat({function_name, "(): "}), &args,
                               /*min_required_args=*/2);
  std::u16string arg0_key;
  std::u16string arg1_value;
  args_converter.ConvertArg(0, "key", arg0_key);
  args_converter.ConvertArg(1, "value", arg1_value);

  std::optional<bool> ignore_if_present;
  std::optional<std::string> with_lock;
  if (args_converter.is_success() && args.Length() > 2) {
    DictConverter options_dict_converter(
        v8_helper, time_limit_scope,
        base::StrCat({function_name, " 'options' argument "}), args[2]);
    options_dict_converter.GetOptional("ignoreIfPresent", ignore_if_present);
    options_dict_converter.GetOptional("withLock", with_lock);
    args_converter.SetStatus(options_dict_converter.TakeStatus());
  }

  if (args_converter.is_failed()) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return nullptr;
  }

  if (!shared_storage_permissions_policy_allowed) {
    isolate->ThrowException(v8::Exception::TypeError(
        gin::StringToV8(isolate, kPermissionsPolicyError)));
    return nullptr;
  }

  // IDL portions of checking done, now do semantic checking.
  if (!network::IsValidSharedStorageKeyStringLength(arg0_key.size())) {
    isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
        isolate,
        base::StrCat({"Invalid 'key' argument in ", function_name, "()"}))));
    return nullptr;
  }

  if (!network::IsValidSharedStorageValueStringLength(arg1_value.size())) {
    isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
        isolate,
        base::StrCat({"Invalid 'value' argument in ", function_name, "()"}))));
    return nullptr;
  }

  auto method = network::mojom::SharedStorageModifierMethod::NewSetMethod(
      network::mojom::SharedStorageSetMethod::New(
          arg0_key, arg1_value, ignore_if_present.value_or(false)));

  return network::mojom::SharedStorageModifierMethodWithOptions::New(
      std::move(method), std::move(with_lock));
}

network::mojom::SharedStorageModifierMethodWithOptionsPtr
CreateMojomAppendMethodFromParameters(
    AuctionV8Helper* v8_helper,
    const v8::FunctionCallbackInfo<v8::Value>& args,
    bool shared_storage_permissions_policy_allowed,
    const std::string& function_name) {
  v8::Isolate* isolate = v8_helper->isolate();

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(v8_helper, time_limit_scope,
                               base::StrCat({function_name, "(): "}), &args,
                               /*min_required_args=*/2);

  std::u16string arg0_key;
  std::u16string arg1_value;
  args_converter.ConvertArg(0, "key", arg0_key);
  args_converter.ConvertArg(1, "value", arg1_value);

  std::optional<std::string> with_lock;
  if (args_converter.is_success() && args.Length() > 2) {
    DictConverter options_dict_converter(
        v8_helper, time_limit_scope,
        base::StrCat({function_name, " 'options' argument "}), args[2]);
    options_dict_converter.GetOptional("withLock", with_lock);
    args_converter.SetStatus(options_dict_converter.TakeStatus());
  }

  if (args_converter.is_failed()) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return nullptr;
  }

  if (!shared_storage_permissions_policy_allowed) {
    isolate->ThrowException(v8::Exception::TypeError(
        gin::StringToV8(isolate, kPermissionsPolicyError)));
    return nullptr;
  }

  // IDL portions of checking done, now do semantic checking.
  if (!network::IsValidSharedStorageKeyStringLength(arg0_key.size())) {
    isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
        isolate,
        base::StrCat({"Invalid 'key' argument in ", function_name, "()"}))));
    return nullptr;
  }

  if (!network::IsValidSharedStorageValueStringLength(arg1_value.size())) {
    isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
        isolate,
        base::StrCat({"Invalid 'value' argument in ", function_name, "()"}))));
    return nullptr;
  }

  auto method = network::mojom::SharedStorageModifierMethod::NewAppendMethod(
      network::mojom::SharedStorageAppendMethod::New(arg0_key, arg1_value));

  return network::mojom::SharedStorageModifierMethodWithOptions::New(
      std::move(method), std::move(with_lock));
}

network::mojom::SharedStorageModifierMethodWithOptionsPtr
CreateMojomDeleteMethodFromParameters(
    AuctionV8Helper* v8_helper,
    const v8::FunctionCallbackInfo<v8::Value>& args,
    bool shared_storage_permissions_policy_allowed,
    const std::string& function_name) {
  v8::Isolate* isolate = v8_helper->isolate();

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(v8_helper, time_limit_scope,
                               base::StrCat({function_name, "(): "}), &args,
                               /*min_required_args=*/1);

  std::u16string arg0_key;
  args_converter.ConvertArg(0, "key", arg0_key);

  std::optional<std::string> with_lock;
  if (args_converter.is_success() && args.Length() > 1) {
    DictConverter options_dict_converter(
        v8_helper, time_limit_scope,
        base::StrCat({function_name, " 'options' argument "}), args[1]);
    options_dict_converter.GetOptional("withLock", with_lock);
    args_converter.SetStatus(options_dict_converter.TakeStatus());
  }

  if (args_converter.is_failed()) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return nullptr;
  }

  if (!shared_storage_permissions_policy_allowed) {
    isolate->ThrowException(v8::Exception::TypeError(
        gin::StringToV8(isolate, kPermissionsPolicyError)));
    return nullptr;
  }

  // IDL portions of checking done, now do semantic checking.
  if (!network::IsValidSharedStorageKeyStringLength(arg0_key.size())) {
    isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
        isolate,
        base::StrCat({"Invalid 'key' argument in ", function_name, "()"}))));
    return nullptr;
  }

  auto method = network::mojom::SharedStorageModifierMethod::NewDeleteMethod(
      network::mojom::SharedStorageDeleteMethod::New(arg0_key));

  return network::mojom::SharedStorageModifierMethodWithOptions::New(
      std::move(method), std::move(with_lock));
}

network::mojom::SharedStorageModifierMethodWithOptionsPtr
CreateMojomClearMethodFromParameters(
    AuctionV8Helper* v8_helper,
    const v8::FunctionCallbackInfo<v8::Value>& args,
    bool shared_storage_permissions_policy_allowed,
    const std::string& function_name) {
  v8::Isolate* isolate = v8_helper->isolate();

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(v8_helper, time_limit_scope,
                               base::StrCat({function_name, "(): "}), &args,
                               /*min_required_args=*/0);

  std::optional<std::string> with_lock;
  if (args_converter.is_success() && args.Length() > 0) {
    DictConverter options_dict_converter(
        v8_helper, time_limit_scope,
        base::StrCat({function_name, " 'options' argument "}), args[0]);
    options_dict_converter.GetOptional("withLock", with_lock);
    args_converter.SetStatus(options_dict_converter.TakeStatus());
  }

  if (args_converter.is_failed()) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return nullptr;
  }

  if (!shared_storage_permissions_policy_allowed) {
    isolate->ThrowException(v8::Exception::TypeError(
        gin::StringToV8(isolate, kPermissionsPolicyError)));
    return nullptr;
  }

  auto method = network::mojom::SharedStorageModifierMethod::NewClearMethod(
      network::mojom::SharedStorageClearMethod::New());

  return network::mojom::SharedStorageModifierMethodWithOptions::New(
      std::move(method), std::move(with_lock));
}

// SharedStorageMethod represents a method for modifying shared storage. It
// manages its own lifecycle through weak reference handling to support
// automatic garbage collection.
class SharedStorageMethod {
 public:
  // Constructs a SharedStorageMethod with a Mojom method and sets up
  // weak reference management.
  //
  // Responsibilities:
  // - Create a V8 External wrapping the C++ object
  // - Establish a persistent, weak reference to the External
  // - Set the External as an internal field of the JavaScript object
  // - Ensure proper cleanup when the JavaScript object is garbage collected
  SharedStorageMethod(
      v8::Isolate* isolate,
      v8::Local<v8::Object> obj,
      network::mojom::SharedStorageModifierMethodWithOptionsPtr mojom_method)
      : mojom_method_(std::move(mojom_method)) {
    v8::Local<v8::External> external = v8::External::New(isolate, this);
    persistent_external_.Reset(isolate, external);
    obj->SetInternalField(0, external);
    persistent_external_.SetWeak(this, WeakCallback,
                                 v8::WeakCallbackType::kParameter);
  }

  // Weak callback invoked by V8's garbage collector when the associated
  // JavaScript object becomes unreachable.
  //
  // Responsibilities:
  // - Clear the persistent external reference
  // - Delete the SharedStorageMethod instance
  static void WeakCallback(
      const v8::WeakCallbackInfo<SharedStorageMethod>& data) {
    SharedStorageMethod* method = data.GetParameter();
    method->persistent_external_.Reset();
    delete method;
  }

 private:
  network::mojom::SharedStorageModifierMethodWithOptionsPtr mojom_method_;
  v8::Persistent<v8::External> persistent_external_;
};

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

  // These modifier methods are part of the Web Locks integration launch.
  if (base::FeatureList::IsEnabled(blink::features::kSharedStorageWebLocks)) {
    v8::Local<v8::FunctionTemplate> base_modifier_method_template =
        v8::FunctionTemplate::New(v8_helper_->isolate());
    base_modifier_method_template->SetClassName(
        v8_helper_->CreateStringFromLiteral("SharedStorageModifierMethod"));

    v8::Local<v8::FunctionTemplate> set_method_ctor_template =
        v8::FunctionTemplate::New(v8_helper_->isolate(),
                                  &SharedStorageBindings::SetMethodConstructor,
                                  v8_this);
    set_method_ctor_template->InstanceTemplate()->SetInternalFieldCount(1);
    set_method_ctor_template->Inherit(base_modifier_method_template);
    set_method_ctor_template->SetClassName(
        v8_helper_->CreateStringFromLiteral(kSharedStorageSetMethodName));
    v8::Local<v8::Function> set_method_ctor =
        set_method_ctor_template->GetFunction(context).ToLocalChecked();
    context->Global()
        ->Set(context,
              v8_helper_->CreateStringFromLiteral(kSharedStorageSetMethodName),
              set_method_ctor)
        .Check();

    v8::Local<v8::FunctionTemplate> append_method_ctor_template =
        v8::FunctionTemplate::New(
            v8_helper_->isolate(),
            &SharedStorageBindings::AppendMethodConstructor, v8_this);
    append_method_ctor_template->InstanceTemplate()->SetInternalFieldCount(1);
    append_method_ctor_template->Inherit(base_modifier_method_template);
    append_method_ctor_template->SetClassName(
        v8_helper_->CreateStringFromLiteral(kSharedStorageAppendMethodName));
    v8::Local<v8::Function> append_method_ctor =
        append_method_ctor_template->GetFunction(context).ToLocalChecked();
    context->Global()
        ->Set(
            context,
            v8_helper_->CreateStringFromLiteral(kSharedStorageAppendMethodName),
            append_method_ctor)
        .Check();

    v8::Local<v8::FunctionTemplate> delete_method_ctor_template =
        v8::FunctionTemplate::New(
            v8_helper_->isolate(),
            &SharedStorageBindings::DeleteMethodConstructor, v8_this);
    delete_method_ctor_template->InstanceTemplate()->SetInternalFieldCount(1);
    delete_method_ctor_template->Inherit(base_modifier_method_template);
    delete_method_ctor_template->SetClassName(
        v8_helper_->CreateStringFromLiteral(kSharedStorageDeleteMethodName));
    v8::Local<v8::Function> delete_method_ctor =
        delete_method_ctor_template->GetFunction(context).ToLocalChecked();
    context->Global()
        ->Set(
            context,
            v8_helper_->CreateStringFromLiteral(kSharedStorageDeleteMethodName),
            delete_method_ctor)
        .Check();

    v8::Local<v8::FunctionTemplate> clear_method_ctor_template =
        v8::FunctionTemplate::New(
            v8_helper_->isolate(),
            &SharedStorageBindings::ClearMethodConstructor, v8_this);
    clear_method_ctor_template->InstanceTemplate()->SetInternalFieldCount(1);
    clear_method_ctor_template->Inherit(base_modifier_method_template);
    clear_method_ctor_template->SetClassName(
        v8_helper_->CreateStringFromLiteral(kSharedStorageClearMethodName));
    v8::Local<v8::Function> clear_method_ctor =
        clear_method_ctor_template->GetFunction(context).ToLocalChecked();
    context->Global()
        ->Set(
            context,
            v8_helper_->CreateStringFromLiteral(kSharedStorageClearMethodName),
            clear_method_ctor)
        .Check();
  }
}

void SharedStorageBindings::Reset() {}

// static
void SharedStorageBindings::Set(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  SharedStorageBindings* bindings = static_cast<SharedStorageBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  network::mojom::SharedStorageModifierMethodWithOptionsPtr mojom_method =
      CreateMojomSetMethodFromParameters(
          v8_helper, args, bindings->shared_storage_permissions_policy_allowed_,
          /*function_name=*/"sharedStorage.set");
  if (!mojom_method) {
    return;
  }

  bindings->shared_storage_host_->SharedStorageUpdate(
      std::move(mojom_method), bindings->source_auction_worklet_function_);
}

// static
void SharedStorageBindings::Append(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  SharedStorageBindings* bindings = static_cast<SharedStorageBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  network::mojom::SharedStorageModifierMethodWithOptionsPtr mojom_method =
      CreateMojomAppendMethodFromParameters(
          v8_helper, args, bindings->shared_storage_permissions_policy_allowed_,
          /*function_name=*/"sharedStorage.append");
  if (!mojom_method) {
    return;
  }

  bindings->shared_storage_host_->SharedStorageUpdate(
      std::move(mojom_method), bindings->source_auction_worklet_function_);
}

// static
void SharedStorageBindings::Delete(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  SharedStorageBindings* bindings = static_cast<SharedStorageBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  network::mojom::SharedStorageModifierMethodWithOptionsPtr mojom_method =
      CreateMojomDeleteMethodFromParameters(
          v8_helper, args, bindings->shared_storage_permissions_policy_allowed_,
          /*function_name=*/"sharedStorage.delete");
  if (!mojom_method) {
    return;
  }

  bindings->shared_storage_host_->SharedStorageUpdate(
      std::move(mojom_method), bindings->source_auction_worklet_function_);
}

// static
void SharedStorageBindings::Clear(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  SharedStorageBindings* bindings = static_cast<SharedStorageBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  network::mojom::SharedStorageModifierMethodWithOptionsPtr mojom_method =
      CreateMojomClearMethodFromParameters(
          v8_helper, args, bindings->shared_storage_permissions_policy_allowed_,
          /*function_name=*/"sharedStorage.clear");
  if (!mojom_method) {
    return;
  }

  bindings->shared_storage_host_->SharedStorageUpdate(
      std::move(mojom_method), bindings->source_auction_worklet_function_);
}

// static
void SharedStorageBindings::SetMethodConstructor(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  SharedStorageBindings* bindings = static_cast<SharedStorageBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;
  v8::Isolate* isolate = v8_helper->isolate();

  if (!args.IsConstructCall()) {
    isolate->ThrowException(v8::Exception::TypeError(
        gin::StringToV8(isolate, kNotAConstructorError)));
    return;
  }

  network::mojom::SharedStorageModifierMethodWithOptionsPtr mojom_method =
      CreateMojomSetMethodFromParameters(
          v8_helper, args, bindings->shared_storage_permissions_policy_allowed_,
          /*function_name=*/kSharedStorageSetMethodName);
  if (!mojom_method) {
    return;
  }

  v8::Local<v8::Object> obj = args.This();
  new SharedStorageMethod(isolate, obj, std::move(mojom_method));
  args.GetReturnValue().Set(obj);
}

// static
void SharedStorageBindings::AppendMethodConstructor(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  SharedStorageBindings* bindings = static_cast<SharedStorageBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;
  v8::Isolate* isolate = v8_helper->isolate();

  if (!args.IsConstructCall()) {
    isolate->ThrowException(v8::Exception::TypeError(
        gin::StringToV8(isolate, kNotAConstructorError)));
    return;
  }

  network::mojom::SharedStorageModifierMethodWithOptionsPtr mojom_method =
      CreateMojomAppendMethodFromParameters(
          v8_helper, args, bindings->shared_storage_permissions_policy_allowed_,
          /*function_name=*/kSharedStorageAppendMethodName);
  if (!mojom_method) {
    return;
  }

  v8::Local<v8::Object> obj = args.This();
  new SharedStorageMethod(isolate, obj, std::move(mojom_method));
  args.GetReturnValue().Set(obj);
}

// static
void SharedStorageBindings::DeleteMethodConstructor(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  SharedStorageBindings* bindings = static_cast<SharedStorageBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;
  v8::Isolate* isolate = v8_helper->isolate();

  if (!args.IsConstructCall()) {
    isolate->ThrowException(v8::Exception::TypeError(
        gin::StringToV8(isolate, kNotAConstructorError)));
    return;
  }

  network::mojom::SharedStorageModifierMethodWithOptionsPtr mojom_method =
      CreateMojomDeleteMethodFromParameters(
          v8_helper, args, bindings->shared_storage_permissions_policy_allowed_,
          /*function_name=*/kSharedStorageDeleteMethodName);
  if (!mojom_method) {
    return;
  }

  v8::Local<v8::Object> obj = args.This();
  new SharedStorageMethod(isolate, obj, std::move(mojom_method));
  args.GetReturnValue().Set(obj);
}

// static
void SharedStorageBindings::ClearMethodConstructor(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  SharedStorageBindings* bindings = static_cast<SharedStorageBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;
  v8::Isolate* isolate = v8_helper->isolate();

  if (!args.IsConstructCall()) {
    isolate->ThrowException(v8::Exception::TypeError(
        gin::StringToV8(isolate, kNotAConstructorError)));
    return;
  }

  network::mojom::SharedStorageModifierMethodWithOptionsPtr mojom_method =
      CreateMojomClearMethodFromParameters(
          v8_helper, args, bindings->shared_storage_permissions_policy_allowed_,
          /*function_name=*/kSharedStorageClearMethodName);
  if (!mojom_method) {
    return;
  }

  v8::Local<v8::Object> obj = args.This();
  new SharedStorageMethod(isolate, obj, std::move(mojom_method));
  args.GetReturnValue().Set(obj);
}

}  // namespace auction_worklet
