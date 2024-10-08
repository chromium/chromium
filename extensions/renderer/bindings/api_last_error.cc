// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_last_error.h"

#include <optional>
#include <tuple>

#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"

namespace extensions {

namespace {

constexpr char kLastErrorProperty[] = "lastError";
constexpr char kScriptSuppliedValueKey[] = "script_supplied_value";
constexpr char kUncheckedErrorPrefix[] = "Unchecked runtime.lastError: ";

// The object corresponding to the lastError property, containing a single
// property ('message') with the last error. This object is stored on the parent
// (chrome.runtime in production) as a private property, and is returned via an
// accessor which marks the error as accessed.
class LastErrorObject final : public gin::Wrappable<LastErrorObject> {
 public:
  explicit LastErrorObject(const std::string& error) : error_(error) {}

  LastErrorObject(const LastErrorObject&) = delete;
  LastErrorObject& operator=(const LastErrorObject&) = delete;

  static gin::WrapperInfo kWrapperInfo;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override {
    DCHECK(isolate);
    return Wrappable<LastErrorObject>::GetObjectTemplateBuilder(isolate)
        .SetProperty("message", &LastErrorObject::error);
  }

  void Reset(const std::string& error) {
    error_ = error;
    accessed_ = false;
  }

  const std::string& error() const { return error_; }
  bool accessed() const { return accessed_; }
  void set_accessed() { accessed_ = true; }

 private:
  std::string error_;
  bool accessed_ = false;
};

gin::WrapperInfo LastErrorObject::kWrapperInfo = {gin::kEmbedderNativeGin};

// An accessor to retrieve the last error property (curried in through data),
// and mark it as accessed.
void LastErrorGetter(v8::Local<v8::Name> property,
                     const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> holder = info.Holder();
  v8::Local<v8::Context> context = holder->GetCreationContextChecked(isolate);

  v8::Local<v8::Value> last_error;
  v8::Local<v8::Private> last_error_key = v8::Private::ForApi(
      isolate, gin::StringToSymbol(isolate, kLastErrorProperty));
  if (!holder->GetPrivate(context, last_error_key).ToLocal(&last_error) ||
      last_error != info.Data()) {
    // Something funny happened - our private properties aren't set right.
    NOTREACHED_IN_MIGRATION();
    return;
  }

  v8::Local<v8::Value> return_value;

  // It's possible that some script has set their own value for the last error
  // property. If so, return that. Otherwise, return the real last error.
  v8::Local<v8::Private> script_value_key = v8::Private::ForApi(
      isolate, gin::StringToSymbol(isolate, kScriptSuppliedValueKey));
  v8::Local<v8::Value> script_value;
  if (holder->GetPrivate(context, script_value_key).ToLocal(&script_value) &&
      !script_value->IsUndefined()) {
    return_value = script_value;
  } else {
    LastErrorObject* last_error_obj = nullptr;
    CHECK(gin::Converter<LastErrorObject*>::FromV8(isolate, last_error,
                                                   &last_error_obj));
    last_error_obj->set_accessed();
    return_value = last_error;
  }

  info.GetReturnValue().Set(return_value);
}

// Allow script to set the last error property.
void LastErrorSetter(v8::Local<v8::Name> property,
                     v8::Local<v8::Value> value,
                     const v8::PropertyCallbackInfo<void>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> holder = info.Holder();
  v8::Local<v8::Context> context = holder->GetCreationContextChecked(isolate);

  v8::Local<v8::Private> script_value_key = v8::Private::ForApi(
      isolate, gin::StringToSymbol(isolate, kScriptSuppliedValueKey));
  v8::Maybe<bool> set_private =
      holder->SetPrivate(context, script_value_key, value);
  if (!set_private.IsJust() || !set_private.FromJust())
    NOTREACHED_IN_MIGRATION();
}

}  // namespace

APILastError::APILastError(GetParent get_parent,
                           binding::AddConsoleError add_console_error)
    : get_parent_(std::move(get_parent)),
      add_console_error_(std::move(add_console_error)) {}
APILastError::APILastError(APILastError&& other) = default;
APILastError::~APILastError() = default;

void APILastError::SetError(v8::Local<v8::Context> context,
                            const std::string& error) {
  v8::Isolate* isolate = context->GetIsolate();
  DCHECK(isolate);
  v8::HandleScope handle_scope(isolate);

  // The various accesses/sets on an object could potentially fail if script has
  // set any crazy interceptors. For the most part, we don't care about behaving
  // perfectly in these circumstances, but we eat the exception so callers don't
  // have to worry about it. We also SetVerbose() so that developers will have a
  // clue what happened if this does arise.
  // TODO(devlin): Whether or not this needs to be verbose is debatable.
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  v8::Local<v8::Object> secondary_parent;
  v8::Local<v8::Object> parent = get_parent_.Run(context, &secondary_parent);

  SetErrorOnPrimaryParent(context, parent, error);
  SetErrorOnSecondaryParent(context, secondary_parent, error);
}

void APILastError::ClearError(v8::Local<v8::Context> context,
                              bool report_if_unchecked) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Object> parent;
  v8::Local<v8::Object> secondary_parent;
  LastErrorObject* last_error = nullptr;
  v8::Local<v8::String> key;
  v8::Local<v8::Private> private_key;
  {
    // See comment in SetError().
    v8::TryCatch try_catch(isolate);
    try_catch.SetVerbose(true);

    parent = get_parent_.Run(context, &secondary_parent);
    if (parent.IsEmpty())
      return;
    key = gin::StringToSymbol(isolate, kLastErrorProperty);
    private_key = v8::Private::ForApi(isolate, key);
    v8::Local<v8::Value> error;
    // Access through GetPrivate() so that we don't trigger accessed().
    if (!parent->GetPrivate(context, private_key).ToLocal(&error) ||
        !gin::Converter<LastErrorObject*>::FromV8(context->GetIsolate(), error,
                                                  &last_error)) {
      return;
    }
  }

  if (report_if_unchecked && !last_error->accessed())
    ReportUncheckedError(context, last_error->error());

  // See comment in SetError().
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  v8::Maybe<bool> delete_private = parent->DeletePrivate(context, private_key);
  if (!delete_private.IsJust() || !delete_private.FromJust()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  // These Delete()s can fail, but there's nothing to do if it does (the
  // exception will be caught by the TryCatch above).
  std::ignore = parent->Delete(context, key);
  if (!secondary_parent.IsEmpty())
    std::ignore = secondary_parent->Delete(context, key);
}

bool APILastError::HasError(v8::Local<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  // See comment in SetError().
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  v8::Local<v8::Object> parent = get_parent_.Run(context, nullptr);
  if (parent.IsEmpty())
    return false;
  v8::Local<v8::Value> error;
  v8::Local<v8::Private> key = v8::Private::ForApi(
      isolate, gin::StringToSymbol(isolate, kLastErrorProperty));
  // Access through GetPrivate() so we don't trigger accessed().
  if (!parent->GetPrivate(context, key).ToLocal(&error))
    return false;

  LastErrorObject* last_error = nullptr;
  return gin::Converter<LastErrorObject*>::FromV8(context->GetIsolate(), error,
                                                  &last_error);
}

std::optional<std::string> APILastError::GetErrorMessage(
    v8::Local<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  // See comment in SetError().
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  v8::Local<v8::Object> parent = get_parent_.Run(context, nullptr);
  if (parent.IsEmpty()) {
    return std::nullopt;
  }
  v8::Local<v8::Value> error;
  v8::Local<v8::Private> key = v8::Private::ForApi(
      isolate, gin::StringToSymbol(isolate, kLastErrorProperty));
  // Access through GetPrivate() so we don't trigger accessed() and ensure we
  // get the original error and not any overrides.
  if (!parent->GetPrivate(context, key).ToLocal(&error)) {
    return std::nullopt;
  }

  LastErrorObject* last_error = nullptr;
  if (gin::Converter<LastErrorObject*>::FromV8(context->GetIsolate(), error,
                                               &last_error)) {
    return last_error->error();
  }
  return std::nullopt;
}

void APILastError::ReportUncheckedError(v8::Local<v8::Context> context,
                                        const std::string& error) {
  add_console_error_.Run(context, kUncheckedErrorPrefix + error);
}

void APILastError::SetErrorOnPrimaryParent(v8::Local<v8::Context> context,
                                           v8::Local<v8::Object> parent,
                                           const std::string& error) {
  if (parent.IsEmpty())
    return;
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::String> key = gin::StringToSymbol(isolate, kLastErrorProperty);
  v8::Local<v8::Value> v8_error;
  // Two notes: this Get() is visible to external script, and this will actually
  // mark the lastError as accessed, if one exists. These shouldn't be a
  // problem (lastError is meant to be helpful, but isn't designed to handle
  // crazy chaining, etc). However, if we decide we needed to be fancier, we
  // could detect the presence of a current error through a GetPrivate(), and
  // optionally throw it if one exists.
  if (!parent->Get(context, key).ToLocal(&v8_error))
    return;

  if (!v8_error->IsUndefined()) {
    // There may be an existing last error to overwrite.
    LastErrorObject* last_error = nullptr;
    if (!gin::Converter<LastErrorObject*>::FromV8(isolate, v8_error,
                                                  &last_error)) {
      // If it's not a real lastError (e.g. if a script manually set it), don't
      // do anything. We shouldn't mangle a property set by other script.
      // TODO(devlin): Or should we? If someone sets chrome.runtime.lastError,
      // it might be the right course of action to overwrite it.
      return;
    }
    last_error->Reset(error);
  } else {
    v8::Local<v8::Value> last_error =
        gin::CreateHandle(isolate, new LastErrorObject(error)).ToV8();
    v8::Maybe<bool> set_private = parent->SetPrivate(
        context, v8::Private::ForApi(isolate, key), last_error);
    if (!set_private.IsJust() || !set_private.FromJust()) {
      NOTREACHED_IN_MIGRATION();
      return;
    }
    DCHECK(!last_error.IsEmpty());
    // This SetNativeDataProperty() can fail, but there's nothing to do if it
    // does (the exception will be caught by the TryCatch in SetError()).
    std::ignore = parent->SetNativeDataProperty(context, key, &LastErrorGetter,
                                                &LastErrorSetter, last_error);
  }
}

void APILastError::SetErrorOnSecondaryParent(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> secondary_parent,
    const std::string& error) {
  if (secondary_parent.IsEmpty())
    return;

  // For the secondary parent, simply set chrome.extension.lastError to
  // {message: <error>}.
  // TODO(devlin): Gather metrics on how frequently this is checked. It'd be
  // nice to get rid of it.
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::String> key = gin::StringToSymbol(isolate, kLastErrorProperty);
  // This CreateDataProperty() can fail, but there's nothing to do if it does
  // (the exception will be caught by the TryCatch in SetError()).
  std::ignore = secondary_parent->CreateDataProperty(
      context, key,
      gin::DataObjectBuilder(isolate).Set("message", error).Build());
}

}  // namespace extensions
