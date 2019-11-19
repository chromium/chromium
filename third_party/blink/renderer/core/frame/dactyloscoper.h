// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DACTYLOSCOPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DACTYLOSCOPER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ExecutionContext;

class CORE_EXPORT Dactyloscoper {
  DISALLOW_NEW();

 public:
  Dactyloscoper();

  void Record(WebFeature);

  static void Record(ExecutionContext*, WebFeature);

 private:
  DISALLOW_COPY_AND_ASSIGN(Dactyloscoper);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DACTYLOSCOPER_H_
