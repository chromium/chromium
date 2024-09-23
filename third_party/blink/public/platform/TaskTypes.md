# Frame Associated Task Types

Blink uses a series of task types defined in
[task_type.h](https://cs.chromium.org/chromium/src/third_party/blink/public/platform/task_type.h).
For each task type that can be frame-attributed, the table below indicates
whether the task queue associated with this task type can be paused, throttled,
frozen or deferred. All specified (in W3C, HTML, DOM, etc) task types are
pausable. Some internal task queues are not.

| Queue Type                        | Throttlable | Throttlable (intensive) | Deferrable | Freezable | Pausable | Virtual time |
|-----------------------------------|-------------|-------------------------|------------|-----------|----------|--------------|
| DOMManipulation                   | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| UserInteraction                   | No          | No                      |  No        | Yes       | Yes      | Yes          |
| Networking                        | No          | No                      |  Yes       | Yes       | Yes      | No           |
| NetworkingWithURLLoaderAnnotation | No          | No                      |  Yes       | Yes       | Yes      | No           |
| NetworkingControl                 | No          | No                      |  Yes       | Yes       | Yes      | No           |
| LowPriorityScriptExecution        | No          | No                      |  Yes       | Yes       | Yes      | No           |
| HistoryTraversal                  | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| Embed                             | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| MediaElementEvent                 | No          | No                      |  No        | Yes       | Yes      | Yes          |
| CanvasBlobSerialization           | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| Microtask                         | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| JavascriptTimerDelayedLowNesting  | Yes         | No [^1]                 |  Yes       | Yes       | Yes      | Yes          |
| JavascriptTimerDelayedHighNesting | Yes         | Yes [^2]                |  Yes       | Yes       | Yes      | Yes          |
| JavascriptTimerImmediate          | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| RemoteEvent                       | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| WebSocket                         | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| PostedMessage                     | No          | No                      |  No        | Yes       | Yes      | Yes          |
| UnshippedPortMessage              | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| FileReading                       | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| DatabaseAccess                    | No          | No                      |  No        | Yes       | Yes      | Yes          |
| Presentation                      | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| Sensor                            | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| PerformanceTimeline               | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| WebGL                             | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| WebGPU                            | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| IdleTask                          | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| MiscPlatformAPI                   | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| WorkerAnimation                   | No          | No                      |  No        | Yes       | Yes      | Yes          |
| FontLoading                       | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| ApplicationLifeCycle              | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| BackgroundFetch                   | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| Permission                        | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| ServiceWorkerClientMessage        | No          | No                      |  No        | Yes       | Yes      | Yes          |
| WebLocks                          | No          | No                      |  No        | No        | No       | Yes          |
| WakeLock                          | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| Storage                           | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| MachineLearning                   | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| WebSchedulingPostedTask           | Yes [^3]    | Yes [^3]                |  Yes       | Yes       | Yes      | Yes          |
| InternalDefault                   | No          | No                      |  Yes       | Yes       | Yes      | Yes          |
| InternalLoading                   | No          | No                      |  Yes       | Yes       | Yes      | No           |
| InternalTest                      | No          | No                      |  No        | No        | No       | Yes          |
| InternalWebCrypto                 | No          | No                      |  No        | Yes       | Yes      | Yes          |
| InternalMedia                     | No          | No                      |  No        | Yes       | Yes      | Yes          |
| InternalMediaRealTime             | No          | No                      |  No        | Yes       | Yes      | Yes          |
| InternalIPC                       | No          | No                      |  No        | No        | No       | Yes          |
| InternalUserInteraction           | No          | No                      |  No        | Yes       | Yes      | Yes          |
| InternalInspector                 | No          | No                      |  No        | No        | No       | No           |
| InternalTranslation               | Yes         | No                      |  Yes       | Yes       | Yes      | Yes          |
| InternalIntersectionObserver      | No          | No                      |  No        | Yes       | Yes      | Yes          |
| InternalContentCapture            | Yes         | No                      |  Yes       | Yes       | Yes      | Yes          |
| InternalNavigationAssociated      | No          | No                      |  No        | No        | No       | No           |
| InternalFreezableIPC              | No          | No                      |  No        | Yes       | No       | No           |
| InternalContinueScriptLoading     | No          | No                      |  No        | Yes       | Yes      | Yes          |
| InternalPostMessageForwarding     | No          | No                      |  No        | No        | Yes      | Yes          |

Internal Translation queue supports concept of it running only in the foreground. It is disabled if the page that owns it goes in background.

"Throttlable (Intensive)": Wake ups are limited to 1 per minute when the page
has been backgrounded for 5 minutes. See
[Chrome Platform Status entry](https://www.chromestatus.com/feature/4718288976216064).

[^1] "Yes" if the "IntensiveWakeUpThrottling" feature is enabled and the
"can_intensively_throttle_low_nesting_level" param is "true".

[^2] "No" if the "IntensiveWakeUpThrottling" feature is disabled.

[^3] "Yes" only for `scheduler.postTask()` tasks where delay > 0.
