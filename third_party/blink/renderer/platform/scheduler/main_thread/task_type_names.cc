// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/task_type_names.h"

#include "base/notreached.h"

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
    case TaskType::kNetworkingUnfreezable:
      return "NetworkingUnfreezable";
    case TaskType::kNetworkingUnfreezableRenderBlockingLoading:
      return "NetworkingUnfreezableRenderBlockingLoading";
    case TaskType::kNetworkingControl:
      return "NetworkingControl";
    case TaskType::kLowPriorityScriptExecution:
      return "LowPriorityScriptExecution";
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
    case TaskType::kJavascriptTimerImmediate:
      return "JavascriptTimerImmediate";
    case TaskType::kJavascriptTimerDelayedLowNesting:
      return "JavascriptTimerDelayedLowNesting";
    case TaskType::kJavascriptTimerDelayedHighNesting:
      return "JavascriptTimerDelayedHighNesting";
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
    case TaskType::kFontLoading:
      return "FontLoading";
    case TaskType::kApplicationLifeCycle:
      return "ApplicationLifeCycle";
    case TaskType::kBackgroundFetch:
      return "BackgroundFetch";
    case TaskType::kPermission:
      return "Permission";
    case TaskType::kServiceWorkerClientMessage:
      return "ServiceWorkerClientMessage";
    case TaskType::kWebLocks:
      return "WebLocks";
    case TaskType::kStorage:
      return "Storage";
    case TaskType::kClipboard:
      return "Clipboard";
    case TaskType::kMachineLearning:
      return "MachineLearning";
    case TaskType::kInternalDefault:
      return "InternalDefault";
    case TaskType::kInternalLoading:
      return "InternalLoading";
    case TaskType::kInternalTest:
      return "InternalTest";
    case TaskType::kInternalWebCrypto:
      return "InternalWebCrypto";
    case TaskType::kInternalMedia:
      return "InternalMedia";
    case TaskType::kInternalMediaRealTime:
      return "InternalMediaRealTime";
    case TaskType::kInternalUserInteraction:
      return "InternalUserInteraction";
    case TaskType::kInternalInspector:
      return "InternalInspector";
    case TaskType::kMainThreadTaskQueueV8:
      return "MainThreadTaskQueueV8";
    case TaskType::kMainThreadTaskQueueV8UserVisible:
      return "MainThreadTaskQueueV8UserVisible";
    case TaskType::kMainThreadTaskQueueV8BestEffort:
      return "MainThreadTaskQueueV8BestEffort";
    case TaskType::kMainThreadTaskQueueCompositor:
      return "MainThreadTaskQueueCompositor";
    case TaskType::kMainThreadTaskQueueDefault:
      return "MainThreadTaskQueueDefault";
    case TaskType::kMainThreadTaskQueueInput:
      return "MainThreadTaskQueueInput";
    case TaskType::kMainThreadTaskQueueIdle:
      return "MainThreadTaskQueueIdle";
    case TaskType::kMainThreadTaskQueueControl:
      return "MainThreadTaskQueueControl";
    case TaskType::kMainThreadTaskQueueMemoryPurge:
      return "MainThreadTaskQueueMemoryPurge";
    case TaskType::kMainThreadTaskQueueNonWaking:
      return "MainThreadTaskQueueNonWaking";
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
    case TaskType::kInternalContentCapture:
      return "InternalContentCapture";
    case TaskType::kInternalNavigationAssociated:
      return "InternalNavigationAssociated";
    case TaskType::kInternalNavigationAssociatedUnfreezable:
      return "InternalNavigationAssociatedUnfreezable";
    case TaskType::kInternalNavigationCancellation:
      return "InternalNavigationCancellation";
    case TaskType::kInternalContinueScriptLoading:
      return "InternalContinueScriptLoading";
    case TaskType::kWebSchedulingPostedTask:
      return "WebSchedulingPostedTask";
    case TaskType::kInternalFrameLifecycleControl:
      return "InternalFrameLifecycleControl";
    case TaskType::kInternalFindInPage:
      return "InternalFindInPage";
    case TaskType::kInternalHighPriorityLocalFrame:
      return "InternalHighPriorityLocalFrame";
    case TaskType::kInternalInputBlocking:
      return "InternalInputBlocking";
    case TaskType::kMainThreadTaskQueueIPCTracking:
      return "MainThreadTaskQueueIPCTracking";
    case TaskType::kWakeLock:
      return "WakeLock";
    case TaskType::kWebGPU:
      return "WebGPU";
    case TaskType::kInternalPostMessageForwarding:
      return "InternalPostMessageForwarding";
  }
  // FrameSchedulerImpl should not call this for invalid TaskTypes.
  NOTREACHED_IN_MIGRATION();
  return "";
}

}  // namespace scheduler
}  // namespace blink
