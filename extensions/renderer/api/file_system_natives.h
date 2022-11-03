// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_FILE_SYSTEM_NATIVES_H_
#define EXTENSIONS_RENDERER_API_FILE_SYSTEM_NATIVES_H_

#include "base/compiler_specific.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8-forward.h"

namespace extensions {
class ScriptContext;

// Custom bindings for the nativeFileSystem API.
class FileSystemNatives : public ObjectBackedNativeHandler {
 public:
  explicit FileSystemNatives(ScriptContext* context);

  FileSystemNatives(const FileSystemNatives&) = delete;
  FileSystemNatives& operator=(const FileSystemNatives&) = delete;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  void GetFileEntry(const v8::FunctionCallbackInfo<v8::Value>& args);
  void GetIsolatedFileSystem(const v8::FunctionCallbackInfo<v8::Value>& args);
  void CrackIsolatedFileSystemName(
      const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_FILE_SYSTEM_NATIVES_H_
