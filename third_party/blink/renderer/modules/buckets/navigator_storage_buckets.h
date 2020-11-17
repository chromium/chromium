// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BUCKETS_NAVIGATOR_STORAGE_BUCKETS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BUCKETS_NAVIGATOR_STORAGE_BUCKETS_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class BucketManager;
class ExceptionState;
class Navigator;
class WorkerNavigator;
class ScriptState;

class NavigatorStorageBuckets final {
  STATIC_ONLY(NavigatorStorageBuckets);

 public:
  static BucketManager* storageBuckets(ScriptState*,
                                       Navigator&,
                                       ExceptionState&);
  static BucketManager* storageBuckets(ScriptState*,
                                       WorkerNavigator&,
                                       ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BUCKETS_NAVIGATOR_STORAGE_BUCKETS_H_
