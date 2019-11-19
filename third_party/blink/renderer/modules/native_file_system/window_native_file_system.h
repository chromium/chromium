// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_WINDOW_NATIVE_FILE_SYSTEM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_WINDOW_NATIVE_FILE_SYSTEM_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ChooseFileSystemEntriesOptions;
class LocalDOMWindow;
class ScriptPromise;
class ScriptState;

class WindowNativeFileSystem {
  STATIC_ONLY(WindowNativeFileSystem);

 public:
  static ScriptPromise chooseFileSystemEntries(
      ScriptState*,
      LocalDOMWindow&,
      const ChooseFileSystemEntriesOptions*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_WINDOW_NATIVE_FILE_SYSTEM_H_
