// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_isolated_world_info.h"

#include "third_party/blink/renderer/bindings/core/v8/isolated_world_csp.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"

namespace blink {

void SetIsolatedWorldInfo(int32_t world_id, const WebIsolatedWorldInfo& info) {
  CHECK_GT(world_id, DOMWrapperWorld::kMainWorldId);
  CHECK_LT(world_id, DOMWrapperWorld::kDOMWrapperWorldEmbedderWorldIdLimit);

  scoped_refptr<SecurityOrigin> security_origin =
      info.security_origin.Get() ? info.security_origin.Get()->IsolatedCopy()
                                 : nullptr;

  CHECK(info.content_security_policy.IsNull() || security_origin);

  DOMWrapperWorld::SetIsolatedWorldSecurityOrigin(world_id, security_origin);
  DOMWrapperWorld::SetNonMainWorldStableId(world_id, info.stable_id);
  DOMWrapperWorld::SetNonMainWorldHumanReadableName(world_id,
                                                    info.human_readable_name);
  IsolatedWorldCSP::Get().SetContentSecurityPolicy(
      world_id, info.content_security_policy, security_origin);
}

bool IsEqualOrExceedEmbedderWorldIdLimit(int world_id) {
  if (world_id >= IsolatedWorldId::kEmbedderWorldIdLimit)
    return true;
  return false;
}

WebString GetIsolatedWorldStableId(v8::Local<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  const DOMWrapperWorld& world = DOMWrapperWorld::World(isolate, context);
  DCHECK(!world.IsMainWorld());
  return world.NonMainWorldStableId();
}

WebString GetIsolatedWorldHumanReadableName(v8::Local<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  const DOMWrapperWorld& world = DOMWrapperWorld::World(isolate, context);
  DCHECK(!world.IsMainWorld());
  return world.NonMainWorldHumanReadableName();
}

}  // namespace blink
