// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"

#include "base/format_macros.h"

namespace blink {
namespace scheduler {

using perfetto::protos::pbzero::RendererMainThreadTaskExecution;

double TimeDeltaToMilliseconds(const base::TimeDelta& value) {
  return value.InMillisecondsF();
}

const char* YesNoStateToString(bool is_yes) {
  if (is_yes) {
    return "yes";
  } else {
    return "no";
  }
}

RendererMainThreadTaskExecution::TaskType TaskTypeToProto(TaskType task_type) {
  switch (task_type) {
    case TaskType::kDeprecatedNone:
      return RendererMainThreadTaskExecution::TASK_TYPE_UNKNOWN;
    case TaskType::kDOMManipulation:
      return RendererMainThreadTaskExecution::TASK_TYPE_DOM_MANIPULATION;
    case TaskType::kUserInteraction:
      return RendererMainThreadTaskExecution::TASK_TYPE_USER_INTERACTION;
    case TaskType::kNetworking:
      return RendererMainThreadTaskExecution::TASK_TYPE_NETWORKING;
    case TaskType::kNetworkingControl:
      return RendererMainThreadTaskExecution::TASK_TYPE_NETWORKING_CONTROL;
    case TaskType::kLowPriorityScriptExecution:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_LOW_PRIORITY_SCRIPT_EXECUTION;
    case TaskType::kHistoryTraversal:
      return RendererMainThreadTaskExecution::TASK_TYPE_HISTORY_TRAVERSAL;
    case TaskType::kEmbed:
      return RendererMainThreadTaskExecution::TASK_TYPE_EMBED;
    case TaskType::kMediaElementEvent:
      return RendererMainThreadTaskExecution::TASK_TYPE_MEDIA_ELEMENT_EVENT;
    case TaskType::kCanvasBlobSerialization:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_CANVAS_BLOB_SERIALIZATION;
    case TaskType::kMicrotask:
      return RendererMainThreadTaskExecution::TASK_TYPE_MICROTASK;
    case TaskType::kJavascriptTimerDelayedHighNesting:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_JAVASCRIPT_TIMER_DELAYED_HIGH_NESTING;
    case TaskType::kRemoteEvent:
      return RendererMainThreadTaskExecution::TASK_TYPE_REMOTE_EVENT;
    case TaskType::kWebSocket:
      return RendererMainThreadTaskExecution::TASK_TYPE_WEB_SOCKET;
    case TaskType::kPostedMessage:
      return RendererMainThreadTaskExecution::TASK_TYPE_POSTED_MESSAGE;
    case TaskType::kUnshippedPortMessage:
      return RendererMainThreadTaskExecution::TASK_TYPE_UNSHIPPED_PORT_MESSAGE;
    case TaskType::kFileReading:
      return RendererMainThreadTaskExecution::TASK_TYPE_FILE_READING;
    case TaskType::kDatabaseAccess:
      return RendererMainThreadTaskExecution::TASK_TYPE_DATABASE_ACCESS;
    case TaskType::kPresentation:
      return RendererMainThreadTaskExecution::TASK_TYPE_PRESENTATION;
    case TaskType::kSensor:
      return RendererMainThreadTaskExecution::TASK_TYPE_SENSOR;
    case TaskType::kPerformanceTimeline:
      return RendererMainThreadTaskExecution::TASK_TYPE_PERFORMANCE_TIMELINE;
    case TaskType::kWebGL:
      return RendererMainThreadTaskExecution::TASK_TYPE_WEB_GL;
    case TaskType::kIdleTask:
      return RendererMainThreadTaskExecution::TASK_TYPE_IDLE_TASK;
    case TaskType::kMiscPlatformAPI:
      return RendererMainThreadTaskExecution::TASK_TYPE_MISC_PLATFORM_API;
    case TaskType::kInternalDefault:
      return RendererMainThreadTaskExecution::TASK_TYPE_INTERNAL_DEFAULT;
    case TaskType::kInternalLoading:
      return RendererMainThreadTaskExecution::TASK_TYPE_INTERNAL_LOADING;
    case TaskType::kInternalTest:
      return RendererMainThreadTaskExecution::TASK_TYPE_INTERNAL_TEST;
    case TaskType::kInternalWebCrypto:
      return RendererMainThreadTaskExecution::TASK_TYPE_INTERNAL_WEB_CRYPTO;
    case TaskType::kInternalMedia:
      return RendererMainThreadTaskExecution::TASK_TYPE_INTERNAL_MEDIA;
    case TaskType::kInternalMediaRealTime:
      return RendererMainThreadTaskExecution::TASK_TYPE_INTERNAL_MEDIA_REALTIME;
    case TaskType::kInternalUserInteraction:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_INTERNAL_USER_INTERACTION;
    case TaskType::kInternalInspector:
      return RendererMainThreadTaskExecution::TASK_TYPE_INTERNAL_INSPECTOR;
    case TaskType::kMainThreadTaskQueueV8:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_MAIN_THREAD_TASK_QUEUE_V8;
    case TaskType::kMainThreadTaskQueueV8UserVisible:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_MAIN_THREAD_TASK_QUEUE_V8_USER_VISIBLE;
    case TaskType::kMainThreadTaskQueueV8BestEffort:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_MAIN_THREAD_TASK_QUEUE_V8_BEST_EFFORT;
    case TaskType::kMainThreadTaskQueueCompositor:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_MAIN_THREAD_TASK_QUEUE_COMPOSITOR;
    case TaskType::kMainThreadTaskQueueDefault:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_MAIN_THREAD_TASK_QUEUE_DEFAULT;
    case TaskType::kMainThreadTaskQueueInput:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_MAIN_THREAD_TASK_QUEUE_INPUT;
    case TaskType::kMainThreadTaskQueueIdle:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_MAIN_THREAD_TASK_QUEUE_IDLE;
    case TaskType::kMainThreadTaskQueueControl:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_MAIN_THREAD_TASK_QUEUE_CONTROL;
    case TaskType::kInternalIntersectionObserver:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_INTERNAL_INTERSECTION_OBSERVER;
    case TaskType::kCompositorThreadTaskQueueDefault:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_COMPOSITOR_THREAD_TASK_QUEUE_DEFAULT;
    case TaskType::kWorkerThreadTaskQueueDefault:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_WORKER_THREAD_TASK_QUEUE_DEFAULT;
    case TaskType::kWorkerThreadTaskQueueV8:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_WORKER_THREAD_TASK_QUEUE_V8;
    case TaskType::kWorkerThreadTaskQueueCompositor:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_WORKER_THREAD_TASK_QUEUE_COMPOSITOR;
    case TaskType::kCompositorThreadTaskQueueInput:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_COMPOSITOR_THREAD_TASK_QUEUE_INPUT;
    case TaskType::kWorkerAnimation:
      return RendererMainThreadTaskExecution::TASK_TYPE_WORKER_ANIMATION;
    case TaskType::kInternalTranslation:
      return RendererMainThreadTaskExecution::TASK_TYPE_INTERNAL_TRANSLATION;
    case TaskType::kFontLoading:
      return RendererMainThreadTaskExecution::TASK_TYPE_FONT_LOADING;
    case TaskType::kApplicationLifeCycle:
      return RendererMainThreadTaskExecution::TASK_TYPE_APPLICATION_LIFECYCLE;
    case TaskType::kBackgroundFetch:
      return RendererMainThreadTaskExecution::TASK_TYPE_BACKGROUND_FETCH;
    case TaskType::kPermission:
      return RendererMainThreadTaskExecution::TASK_TYPE_PERMISSION;
    case TaskType::kServiceWorkerClientMessage:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_SERVICE_WORKER_CLIENT_MESSAGE;
    case TaskType::kInternalContentCapture:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_INTERNAL_CONTENT_CAPTURE;
    case TaskType::kMainThreadTaskQueueMemoryPurge:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_MAIN_THREAD_TASK_QUEUE_MEMORY_PURGE;
    case TaskType::kInternalNavigationAssociated:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_INTERNAL_NAVIGATION_ASSOCIATED;
    case TaskType::kInternalNavigationAssociatedUnfreezable:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_INTERNAL_NAVIGATION_ASSOCIATED_UNFREEZABLE;
    case TaskType::kInternalNavigationCancellation:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_INTERNAL_NAVIGATION_CANCELLATION;
    case TaskType::kInternalContinueScriptLoading:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_INTERNAL_CONTINUE_SCRIPT_LOADING;
    case TaskType::kWebLocks:
      return RendererMainThreadTaskExecution::TASK_TYPE_WEB_LOCKS;
    case TaskType::kStorage:
      return RendererMainThreadTaskExecution::TASK_TYPE_STORAGE;
    case TaskType::kClipboard:
      return RendererMainThreadTaskExecution::TASK_TYPE_CLIPBOARD;
    case TaskType::kMachineLearning:
      return RendererMainThreadTaskExecution::TASK_TYPE_MACHINE_LEARNING;
    case TaskType::kWebSchedulingPostedTask:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_WEB_SCHEDULING_POSTED_TASK;
    case TaskType::kInternalFrameLifecycleControl:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_INTERNAL_FRAME_LIFE_CYCLE_CONTROL;
    case TaskType::kMainThreadTaskQueueNonWaking:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_MAIN_THREAD_TASK_QUEUE_NON_WAKING;
    case TaskType::kInternalFindInPage:
      return RendererMainThreadTaskExecution::TASK_TYPE_INTERNAL_FIND_IN_PAGE;
    case TaskType::kInternalHighPriorityLocalFrame:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_INTERNAL_HIGH_PRIORITY_LOCAL_FRAME;
    case TaskType::kJavascriptTimerImmediate:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_JAVASCRIPT_TIMER_IMMEDIATE;
    case TaskType::kJavascriptTimerDelayedLowNesting:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_JAVASCRIPT_TIMER_DELAYED_LOW_NESTING;
    case TaskType::kMainThreadTaskQueueIPCTracking:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_MAIN_THREAD_TASK_QUEUE_IPC_TRACKING;
    case TaskType::kNetworkingUnfreezable:
      return RendererMainThreadTaskExecution::TASK_TYPE_NETWORKING_UNFREEZABLE;
    case TaskType::kNetworkingUnfreezableRenderBlockingLoading:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_NETWORKING_UNFREEZABLE_RENDER_BLOCKING_LOADING;
    case TaskType::kWakeLock:
      return RendererMainThreadTaskExecution::TASK_TYPE_WAKE_LOCK;
    case TaskType::kInternalInputBlocking:
      return RendererMainThreadTaskExecution::TASK_TYPE_INTERNAL_INPUT_BLOCKING;
    case TaskType::kWebGPU:
      return RendererMainThreadTaskExecution::TASK_TYPE_WEB_GPU;
    case TaskType::kInternalPostMessageForwarding:
      return RendererMainThreadTaskExecution::
          TASK_TYPE_INTERNAL_POST_MESSAGE_FORWARDING;
  }
}

TraceableVariableController::TraceableVariableController() = default;

TraceableVariableController::~TraceableVariableController() {
  // Controller should have very same lifetime as their tracers.
  DCHECK(traceable_variables_.empty());
}

void TraceableVariableController::RegisterTraceableVariable(
    TraceableVariable* traceable_variable) {
  traceable_variables_.insert(traceable_variable);
}

void TraceableVariableController::DeregisterTraceableVariable(
    TraceableVariable* traceable_variable) {
  traceable_variables_.erase(traceable_variable);
}

void TraceableVariableController::OnTraceLogEnabled() {
  for (auto* tracer : traceable_variables_) {
    tracer->OnTraceLogEnabled();
  }
}

}  // namespace scheduler
}  // namespace blink
