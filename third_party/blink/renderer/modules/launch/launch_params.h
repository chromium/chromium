// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_LAUNCH_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_LAUNCH_PARAMS_H_

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_handle.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Request;
class ScriptState;
class Visitor;

class LaunchParams final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  LaunchParams(HeapVector<Member<NativeFileSystemHandle>> files);
  ~LaunchParams() override;

  // LaunchParams IDL interface.
  const HeapVector<Member<NativeFileSystemHandle>>& files() { return files_; }
  Request* request(ScriptState* script_state);

  void Trace(blink::Visitor*) override;

 private:
  HeapVector<Member<NativeFileSystemHandle>> files_;
  Member<Request> request_;
  mojom::blink::FetchAPIRequestPtr fetch_request_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_LAUNCH_PARAMS_H_
