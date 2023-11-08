// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/model/public/overlay_request_queue_util.h"

#include "base/check.h"
#include "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"

bool GetIndexOfMatchingRequest(
    OverlayRequestQueue* queue,
    size_t* index,
    base::RepeatingCallback<bool(OverlayRequest*)> matcher) {
  DCHECK(queue);
  DCHECK(index);
  DCHECK(!matcher.is_null());
  for (size_t i = 0; i < queue->size(); ++i) {
    if (matcher.Run(queue->GetRequest(i))) {
      *index = i;
      return true;
    }
  }
  return false;
}
