// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BLOB_NATIVE_HANDLER_H_
#define EXTENSIONS_RENDERER_BLOB_NATIVE_HANDLER_H_

#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8-forward.h"

namespace extensions {
class ScriptContext;

// This native handler is used to extract Blobs' UUIDs and pass them over to the
// browser process extension implementation via argument modification. This is
// necessary to support extension functions that take Blob parameters, as Blobs
// are not serialized and sent over to the browser process in the normal way.
//
// Blobs sent via this method don't have their ref-counts incremented, so the
// app using this technique must be sure to keep a reference.
class BlobNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit BlobNativeHandler(ScriptContext* context);

  // ObjectBackedNativeHandler:
  void AddRoutes() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BLOB_NATIVE_HANDLER_H_
