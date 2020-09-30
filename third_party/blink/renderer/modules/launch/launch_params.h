// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_LAUNCH_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_LAUNCH_PARAMS_H_

#include "third_party/blink/renderer/modules/file_system_access/native_file_system_handle.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

class Visitor;

class LaunchParams final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  LaunchParams(HeapVector<Member<NativeFileSystemHandle>> files);
  ~LaunchParams() override;

  // LaunchParams IDL interface.
  const HeapVector<Member<NativeFileSystemHandle>>& files() { return files_; }

  void Trace(Visitor*) const override;

 private:
  HeapVector<Member<NativeFileSystemHandle>> files_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_LAUNCH_PARAMS_H_
