// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "third_party/blink/renderer/modules/launch/launch_params.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_handle.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

LaunchParams::LaunchParams(HeapVector<Member<NativeFileSystemHandle>> files)
    : files_(files) {}

LaunchParams::~LaunchParams() = default;

Request* LaunchParams::request(ScriptState* script_state) {
  if (!fetch_request_)
    return nullptr;

  if (!request_) {
    request_ =
        Request::Create(script_state, *fetch_request_.get(),
                        FetchRequestData::ForServiceWorkerFetchEvent::kFalse);
  }

  return request_;
}

void LaunchParams::Trace(blink::Visitor* visitor) {
  visitor->Trace(files_);
  visitor->Trace(request_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
