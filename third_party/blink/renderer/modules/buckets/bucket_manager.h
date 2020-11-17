// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BUCKETS_BUCKET_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BUCKETS_BUCKET_MANAGER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExceptionState;

class MODULES_EXPORT BucketManager final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  BucketManager();
  ~BucketManager() override = default;

  ScriptPromise openOrCreate(ScriptState* script_state,
                             const String& name,
                             ExceptionState& exception_state);
  ScriptPromise keys(ScriptState* script_state,
                     ExceptionState& exception_state);
  ScriptPromise Delete(ScriptState* script_state,
                       const String& name,
                       ExceptionState& exception_state);

  void Trace(Visitor*) const override;

 private:
  // TODO(ayui): Temporary list of bucket names. This information will be
  // obtained from the browser process in the future.
  Vector<String> bucket_list_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BUCKETS_BUCKET_MANAGER_H_
