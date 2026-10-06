// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_read_image_trace.h"

#import "base/trace_event/trace_event.h"
#import "base/trace_event/trace_id_helper.h"

@implementation SnapshotReadImageTrace {
  uint64_t _flowId;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _flowId = base::trace_event::GetNextGlobalTraceId();
    TRACE_EVENT_INSTANT("ui", "ImageFileManager.readImage:post",
                        perfetto::Flow::ProcessScoped(_flowId));
  }
  return self;
}

- (void)backgroundTask:(void(NS_NOESCAPE ^)(void))block {
  TRACE_EVENT("ui", "ImageFileManager.readImage:backgroundTask",
              perfetto::Flow::ProcessScoped(_flowId));
  block();
}

- (void)completion:(void(NS_NOESCAPE ^)(void))block {
  TRACE_EVENT("ui", "ImageFileManager.readImage:completion",
              perfetto::TerminatingFlow::ProcessScoped(_flowId));
  block();
}

@end
