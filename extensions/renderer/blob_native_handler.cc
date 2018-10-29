// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/blob_native_handler.h"

#include "base/bind.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_blob.h"

namespace {

// Expects a single Blob argument. Returns the Blob's UUID.
void GetBlobUuid(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  blink::WebBlob blob = blink::WebBlob::FromV8Value(args[0]);
  args.GetReturnValue().Set(v8::String::NewFromUtf8(args.GetIsolate(),
                                                    blob.Uuid().Utf8().data(),
                                                    v8::NewStringType::kNormal)
                                .ToLocalChecked());
}

}  // namespace

namespace extensions {

BlobNativeHandler::BlobNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void BlobNativeHandler::AddRoutes() {
  RouteHandlerFunction("GetBlobUuid", base::Bind(&GetBlobUuid));
  RouteHandlerFunction("TakeBrowserProcessBlob",
                       base::Bind(&BlobNativeHandler::TakeBrowserProcessBlob,
                                  base::Unretained(this)));
}

// Take ownership of a Blob created on the browser process. Expects the Blob's
// UUID, type, and size as arguments. Returns the Blob we just took to
// Javascript. The Blob reference in the browser process is dropped through
// a separate flow to avoid leaking Blobs if the script context is destroyed.
void BlobNativeHandler::TakeBrowserProcessBlob(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(3, args.Length());
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsString());
  CHECK(args[2]->IsInt32());
  v8::Isolate* isolate = args.GetIsolate();
  std::string uuid(*v8::String::Utf8Value(isolate, args[0]));
  std::string type(*v8::String::Utf8Value(isolate, args[1]));
  blink::WebBlob blob = blink::WebBlob::CreateFromUUID(
      blink::WebString::FromUTF8(uuid), blink::WebString::FromUTF8(type),
      args[2].As<v8::Int32>()->Value());
  args.GetReturnValue().Set(
      blob.ToV8Value(context()->v8_context()->Global(), isolate));
}

}  // namespace extensions
