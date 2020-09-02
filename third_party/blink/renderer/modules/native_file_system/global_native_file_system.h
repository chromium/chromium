// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_GLOBAL_NATIVE_FILE_SYSTEM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_GLOBAL_NATIVE_FILE_SYSTEM_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class OpenFilePickerOptions;
class SaveFilePickerOptions;
class DirectoryPickerOptions;
class ExceptionState;
class LocalDOMWindow;
class ScriptPromise;
class ScriptState;

class GlobalNativeFileSystem {
  STATIC_ONLY(GlobalNativeFileSystem);

 public:
  static ScriptPromise showOpenFilePicker(ScriptState*,
                                          LocalDOMWindow&,
                                          const OpenFilePickerOptions*,
                                          ExceptionState&);
  static ScriptPromise showSaveFilePicker(ScriptState*,
                                          LocalDOMWindow&,
                                          const SaveFilePickerOptions*,
                                          ExceptionState&);
  static ScriptPromise showDirectoryPicker(ScriptState*,
                                           LocalDOMWindow&,
                                           const DirectoryPickerOptions*,
                                           ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_GLOBAL_NATIVE_FILE_SYSTEM_H_
