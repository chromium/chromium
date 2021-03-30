// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_STORAGE_MANAGER_FILE_SYSTEM_ACCESS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_STORAGE_MANAGER_FILE_SYSTEM_ACCESS_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ExceptionState;
class ScriptPromise;
class ScriptState;
class StorageManager;

class StorageManagerFileSystemAccess {
  STATIC_ONLY(StorageManagerFileSystemAccess);

 public:
  static ScriptPromise getDirectory(ScriptState*,
                                    const StorageManager&,
                                    ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_STORAGE_MANAGER_FILE_SYSTEM_ACCESS_H_
