# Frame Associated Task Types

Blink uses a series of task types defined in
[task_type.h](https://cs.chromium.org/chromium/src/third_party/blink/public/platform/task_type.h).
For each task type that can be frame-attributed, the table below indicates
whether the task queue associated with this task type can be paused, throttled,
frozen or deferred. All specified (in W3C, HTML, DOM, etc) task types are
pausable. Some internal task queues are not.

| Queue Type                   | Throttlable | Deferrable | Freezable | Pausable | Virtual time |
|------------------------------|-------------|------------|-----------|----------|--------------|
| DOMManipulation              | No          | Yes        | Yes       | Yes      | Yes          |
| UserInteraction              | No          | No         | Yes       | Yes      | Yes          |
| Networking                   | No          | Yes        | Yes       | Yes      | No           |
| NetworkingWithURLLoaderAnnot | No          | Yes        | Yes       | Yes      | No           |
| NetworkingControl            | No          | Yes        | Yes       | Yes      | No           |
| HistoryTraversal             | No          | Yes        | Yes       | Yes      | Yes          |
| Embed                        | No          | Yes        | Yes       | Yes      | Yes          |
| MediaElementEvent            | No          | No         | Yes       | Yes      | Yes          |
| CanvasBlobSerialization      | No          | Yes        | Yes       | Yes      | Yes          |
| Microtask                    | No          | Yes        | Yes       | Yes      | Yes          |
| JavascriptTimer              | Yes         | Yes        | Yes       | Yes      | Yes          |
| RemoteEvent                  | No          | Yes        | Yes       | Yes      | Yes          |
| WebSocket                    | No          | Yes        | Yes       | Yes      | Yes          |
| PostedMessage                | No          | No         | Yes       | Yes      | Yes          |
| UnshippedPortMessage         | No          | Yes        | Yes       | Yes      | Yes          |
| FileReading                  | No          | Yes        | Yes       | Yes      | Yes          |
| DatabaseAccess               | No          | No         | Yes       | Yes      | Yes          |
| Presentation                 | No          | Yes        | Yes       | Yes      | Yes          |
| Sensor                       | No          | Yes        | Yes       | Yes      | Yes          |
| PerformanceTimeline          | No          | Yes        | Yes       | Yes      | Yes          |
| WebGL                        | No          | Yes        | Yes       | Yes      | Yes          |
| IdleTask                     | No          | Yes        | Yes       | Yes      | Yes          |
| MiscPlatformAPI              | No          | Yes        | Yes       | Yes      | Yes          |
| WorkerAnimation              | No          | No         | Yes       | Yes      | Yes          |
| FontLoading                  | No          | Yes        | Yes       | Yes      | Yes          |
| ApplicationLifeCycle         | No          | Yes        | Yes       | Yes      | Yes          |
| BackgroundFetch              | No          | Yes        | Yes       | Yes      | Yes          |
| Permission                   | No          | Yes        | Yes       | Yes      | Yes          |
| ServiceWorkerClientMessage   | No          | No         | Yes       | Yes      | Yes          |
| WebLocks                     | No          | No         | No        | No       | Yes          |
| InternalDefault              | No          | Yes        | Yes       | Yes      | Yes          |
| InternalLoading              | No          | Yes        | Yes       | Yes      | No           |
| InternalTest                 | No          | No         | No        | No       | Yes          |
| InternalWebCrypto            | No          | No         | Yes       | Yes      | Yes          |
| InternalMedia                | No          | No         | Yes       | Yes      | Yes          |
| InternalMediaRealTime        | No          | No         | Yes       | Yes      | Yes          |
| InternalIPC                  | No          | No         | No        | No       | Yes          |
| InternalUserInteraction      | No          | No         | Yes       | Yes      | Yes          |
| InternalInspector            | No          | No         | No        | No       | No           |
| InternalTranslation          | Yes         | Yes        | Yes       | Yes      | Yes          |
| InternalIntersectionObserver | No          | No         | Yes       | Yes      | Yes          |
| InternalContentCapture       | Yes         | Yes        | Yes       | Yes      | Yes          |
| InternalNavigationAssociated | No          | No         | No        | No       | No           |
| InternalFreezableIPC         | No          | No         | Yes       | No       | No           |
| InternalContinueScriptLoadin | No          | No         | Yes       | Yes      | Yes          |

Internal Translation queue supports concept of it running only in the foreground. It is disabled if the page that owns it goes in background.
