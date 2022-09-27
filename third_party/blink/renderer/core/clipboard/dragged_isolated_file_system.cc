// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/clipboard/dragged_isolated_file_system.h"

#include "base/check.h"

namespace blink {

DraggedIsolatedFileSystem::FileSystemIdPreparationCallback
    DraggedIsolatedFileSystem::prepare_callback_ = nullptr;

void DraggedIsolatedFileSystem::Init(
    DraggedIsolatedFileSystem::FileSystemIdPreparationCallback callback) {
  DCHECK(!prepare_callback_);
  prepare_callback_ = callback;
}

void DraggedIsolatedFileSystem::PrepareForDataObject(DataObject* data_object) {
  DCHECK(prepare_callback_);
  (*prepare_callback_)(data_object);
}

}  // namespace blink
