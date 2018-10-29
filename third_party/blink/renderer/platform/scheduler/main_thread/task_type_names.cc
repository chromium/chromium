// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/task_type_names.h"

#include "base/logging.h"

namespace blink {
namespace scheduler {

// static
const char* TaskTypeNames::TaskTypeToString(TaskType task_type) {
  // These names are used in finch trials and should not be changed.
  switch (task_type) {
    case TaskType::kDeprecatedNone:
      return "None";
    case TaskType::kDOMManipulation:
      return "DOMManipulation";
    case TaskType::kUserInteraction:
      return "UserInteraction";
    case TaskType::kNetworking:
      return "Networking";
    case TaskType::kNetworkingWithURLLoaderAnnotation:
      return "NetworkingWithURLLoaderAnnotation";
    case TaskType::kNetworkingControl:
      return "NetworkingControl";
    case TaskType::kHistoryTraversal:
      return "HistoryTraversal";
    case TaskType::kEmbed:
      return "Embed";
    case TaskType::kMediaElementEvent:
      return "MediaElementEvent";
    case TaskType::kCanvasBlobSerialization:
      return "CanvasBlobSerialization";
    case TaskType::kMicrotask:
      return "Microtask";
    case TaskType::kJavascriptTimer:
      return "JavascriptTimer";
    case TaskType::kRemoteEvent:
      return "RemoteEvent";
    case TaskType::kWebSocket:
      return "WebSocket";
    case TaskType::kPostedMessage:
      return "PostedMessage";
    case TaskType::kUnshippedPortMessage:
      return "UnshippedPortMessage";
    case TaskType::kFileReading:
      return "FileReading";
    case TaskType::kDatabaseAccess:
      return "DatabaseAccess";
    case TaskType::kPresentation:
      return "Presentation";
    case TaskType::kSensor:
      return "Sensor";
    case TaskType::kPerformanceTimeline:
      return "PerformanceTimeline";
    case TaskType::kWebGL:
      return "WebGL";
    case TaskType::kIdleTask:
      return "IdleTask";
    case TaskType::kMiscPlatformAPI:
      return "MiscPlatformAPI";
    case TaskType::kExperimentalWebSchedulingUserInteraction:
      return "ExperimentalWebSchedulingUserInteraction";
    case TaskType::kExperimentalWebSchedulingBestEffort:
      return "ExperimentalWebSchedulingBackground";
    case TaskType::kInternalDefault:
      return "InternalDefault";
    case TaskType::kInternalLoading:
      return "InternalLoading";
    case TaskType::kInternalTest:
      return "InternalTest";
    case TaskType::kInternalWebCrypto:
      return "InternalWebCrypto";
    case TaskType::kInternalIndexedDB:
      return "InternalIndexedDB";
    case TaskType::kInternalMedia:
      return "InternalMedia";
    case TaskType::kInternalMediaRealTime:
      return "InternalMediaRealTime";
    case TaskType::kInternalIPC:
      return "InternalIPC";
    case TaskType::kInternalUserInteraction:
      return "InternalUserInteraction";
    case TaskType::kInternalInspector:
      return "InternalInspector";
    case TaskType::kInternalWorker:
      return "InternalWorker";
    case TaskType::kMainThreadTaskQueueV8:
      return "MainThreadTaskQueueV8";
    case TaskType::kMainThreadTaskQueueCompositor:
      return "MainThreadTaskQueueCompositor";
    case TaskType::kMainThreadTaskQueueDefault:
      return "MainThreadTaskQueueDefault";
    case TaskType::kMainThreadTaskQueueInput:
      return "MainThreadTaskQueueInput";
    case TaskType::kMainThreadTaskQueueIdle:
      return "MainThreadTaskQueueIdle";
    case TaskType::kMainThreadTaskQueueIPC:
      return "MainThreadTaskQueueIPC";
    case TaskType::kMainThreadTaskQueueControl:
      return "MainThreadTaskQueueControl";
    case TaskType::kMainThreadTaskQueueCleanup:
      return "MainThreadTaskQueueCleanup";
    case TaskType::kInternalIntersectionObserver:
      return "InternalIntersectionObserver";
    case TaskType::kCompositorThreadTaskQueueDefault:
      return "CompositorThreadTaskQueueDefault";
    case TaskType::kCompositorThreadTaskQueueInput:
      return "CompositorThreadTaskQueueInput";
    case TaskType::kWorkerThreadTaskQueueDefault:
      return "WorkerThreadTaskQueueDefault";
    case TaskType::kWorkerThreadTaskQueueV8:
      return "WorkerThreadTaskQueueV8";
    case TaskType::kWorkerThreadTaskQueueCompositor:
      return "WorkerThreadTaskQueueCompositor";
    case TaskType::kWorkerAnimation:
      return "WorkerAnimation";
    case TaskType::kInternalTranslation:
      return "InternalTranslation";
    case TaskType::kCount:
      return "Count";
  }
  // FrameSchedulerImpl should not call this for invalid TaskTypes.
  NOTREACHED();
  return "";
}

}  // namespace scheduler
}  // namespace blink
