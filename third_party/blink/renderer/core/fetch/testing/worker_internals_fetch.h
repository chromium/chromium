// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_TESTING_WORKER_INTERNALS_FETCH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_TESTING_WORKER_INTERNALS_FETCH_H_

#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class WorkerGlobalScope;
class WorkerInternals;
class Response;

class WorkerInternalsFetch {
  STATIC_ONLY(WorkerInternalsFetch);

 public:
  static Vector<String> getInternalResponseURLList(WorkerInternals&, Response*);
  static int getResourcePriority(WorkerInternals&,
                                 const String& url,
                                 WorkerGlobalScope*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_TESTING_WORKER_INTERNALS_FETCH_H_
