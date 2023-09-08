// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/blob_native_handler.h"

#include "base/functional/bind.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_blob.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-primitive.h"

namespace {

// Expects a single Blob argument. Returns the Blob's UUID.
void GetBlobUuid(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  blink::WebBlob blob = blink::WebBlob::FromV8Value(args.GetIsolate(), args[0]);
  args.GetReturnValue().Set(
      v8::String::NewFromUtf8(args.GetIsolate(), blob.Uuid().Utf8().data())
          .ToLocalChecked());
}

}  // namespace

namespace extensions {

BlobNativeHandler::BlobNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void BlobNativeHandler::AddRoutes() {
  RouteHandlerFunction("GetBlobUuid", base::BindRepeating(&GetBlobUuid));
}

}  // namespace extensions
