// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CAPABILITIES_WORKER_NAVIGATOR_MEDIA_CAPABILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CAPABILITIES_WORKER_NAVIGATOR_MEDIA_CAPABILITIES_H_

#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class MediaCapabilities;
class WorkerNavigator;

// Provides MediaCapabilities as a supplement of WorkerNavigator as an
// attribute.
class WorkerNavigatorMediaCapabilities final
    : public GarbageCollected<WorkerNavigatorMediaCapabilities>,
      public Supplement<WorkerNavigator> {
  USING_GARBAGE_COLLECTED_MIXIN(WorkerNavigatorMediaCapabilities);

 public:
  static const char kSupplementName[];

  static MediaCapabilities* mediaCapabilities(WorkerNavigator&);

  explicit WorkerNavigatorMediaCapabilities(WorkerNavigator&);

  void Trace(blink::Visitor*) override;

 private:
  static WorkerNavigatorMediaCapabilities& From(WorkerNavigator&);

  // The MediaCapabilities instance of this WorkerNavigator.
  Member<MediaCapabilities> capabilities_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CAPABILITIES_WORKER_NAVIGATOR_MEDIA_CAPABILITIES_H_
