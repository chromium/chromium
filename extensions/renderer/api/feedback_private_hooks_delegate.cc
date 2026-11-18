// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/feedback_private_hooks_delegate.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/v8_helpers.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/web/web_blob.h"

namespace extensions {

using v8_helpers::GetPropertyUnsafe;
using v8_helpers::ToV8StringUnsafe;

namespace {
constexpr char kSendFeedback[] = "feedbackPrivate.sendFeedback";

bool AttachBlobToRequest(v8::Local<v8::Context> context,
                         v8::Local<v8::Value> blob_value,
                         v8::Local<v8::Object> object_with_index,
                         const char* index_property_name,
                         APIBindingHooks::RequestResult& result) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  blink::WebBlob blob = blink::WebBlob::FromV8Value(isolate, blob_value);
  if (blob.IsNull()) {
    return false;
  }
  CHECK(
      object_with_index
          ->CreateDataProperty(
              context, ToV8StringUnsafe(isolate, index_property_name),
              v8::Integer::New(isolate, static_cast<int>(result.blobs.size())))
          .ToChecked());
  result.blobs.emplace_back(blob.CloneBlobRemote());
  return true;
}

// Function that, for callback based API calls, will make the values associated
// with each property on the return object a separate argument in a new result
// vector it returns instead.
// Note: This is to allow the promise version of the API to return a
// single object, while still supporting the previous callback version which
// expects multiple parameters to be passed to the callback.
v8::LocalVector<v8::Value> MassageSendFeedbackResults(
    const v8::LocalVector<v8::Value>& result_args,
    v8::Local<v8::Context> context,
    binding::AsyncResponseType async_type) {
  // If this is not a callback based API call, we don't need to modify anything.
  if (async_type != binding::AsyncResponseType::kCallback) {
    return result_args;
  }

  DCHECK_EQ(1u, result_args.size());
  DCHECK(result_args[0]->IsObject());
  v8::Local<v8::Object> result_obj = result_args[0].As<v8::Object>();

  // The object sent back has two properties on it which we need to split into
  // two separate arguments"
  v8::Local<v8::Value> status;
  bool success =
      v8_helpers::GetProperty(context, result_obj, "status", &status);
  DCHECK(success);
  v8::Local<v8::Value> landingPageType;
  success = v8_helpers::GetProperty(context, result_obj, "landingPageType",
                                    &landingPageType);
  DCHECK(success);
  v8::LocalVector<v8::Value> new_args(v8::Isolate::GetCurrent(),
                                      {status, landingPageType});
  return new_args;
}

}  // namespace

using RequestResult = APIBindingHooks::RequestResult;

FeedbackPrivateHooksDelegate::FeedbackPrivateHooksDelegate() = default;
FeedbackPrivateHooksDelegate::~FeedbackPrivateHooksDelegate() = default;

RequestResult FeedbackPrivateHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value>* arguments,
    const APITypeReferenceMap& refs) {
  // Only add the result handler for the sendFeedback function.
  if (method_name != kSendFeedback) {
    return RequestResult(RequestResult::NOT_HANDLED);
  }

  std::optional<RequestResult> parse_error =
      binding::ParseArgumentsForHook(*signature, context, arguments, refs);
  if (parse_error) {
    return std::move(*parse_error);
  }

  // Convert blob objects to blob remotes and set reference indexes.
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  RequestResult result(RequestResult::ARGUMENTS_UPDATED,
                       v8::Local<v8::Function>() /*custom_callback*/,
                       base::BindOnce(MassageSendFeedbackResults));
  v8::Local<v8::Object> feedback = (*arguments)[0].As<v8::Object>();
  v8::Local<v8::Value> attached_file =
      GetPropertyUnsafe(context, feedback, "attachedFile");
  if (!attached_file->IsUndefined()) {
    v8::Local<v8::Object> file = attached_file.As<v8::Object>();
    v8::Local<v8::Value> data = GetPropertyUnsafe(context, file, "data");
    if (!AttachBlobToRequest(context, data, file, "attachedFileBlobIndex",
                             result)) {
      return RequestResult("Attached file data must be a Blob.");
    }
    CHECK(file->Delete(context, ToV8StringUnsafe(isolate, "data")).ToChecked());
  }

  v8::Local<v8::Value> screenshot =
      GetPropertyUnsafe(context, feedback, "screenshot");
  if (!screenshot->IsUndefined() &&
      !AttachBlobToRequest(context, screenshot, feedback, "screenshotBlobIndex",
                           result)) {
    return RequestResult("Screenshot must be a Blob.");
  }
  CHECK(feedback->Delete(context, ToV8StringUnsafe(isolate, "screenshot"))
            .ToChecked());
  return result;
}

}  // namespace extensions
