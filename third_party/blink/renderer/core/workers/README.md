This directory contains the base implementation of all worker and worklet types. Also, this contains the implementation of the [Web Workers API](https://html.spec.whatwg.org/multipage/workers.html#workers) (Dedicated Worker and Shared Worker) and [Worklets API](https://drafts.css-houdini.org/worklets/).

# Worker / Worklet types

- Workers are divided into 2 types:
  - **In-process workers (Dedicated Workers)**: A worker of this type always runs in the same renderer process with a parent document that starts the worker.
  - **Out-of-process workers (Shared Workers and Service Workers)**: A worker of this type may run in a different renderer process with a parent document that starts the worker.
- Worklets are divided into 2 types:
  - **Main thread worklets (Paint Worklets and Layout Worklets)**: A worklet of this type runs on the main thread.
  - **Threaded worklets (Audio Worklets and Animation Worklets)**: A worklet of this type runs on a worker thread.

Worklets always run in the same renderer process with a parent document that starts the worklet like the in-process-workers.

# Naming conventions

Classes in this directory are named with the following conventions, there're still some exceptions though.

- `WorkerOrWorklet` prefix: Classes commonly used for workers and worklets (e.g., `WorkerOrWorkletGlobalScope`).
- `Worker` / `Worklet` prefix: Classes used for workers or worklets (e.g., `WorkerGlobalScope`).
- `Threaded` prefix: Classes used for workers and threaded worklets (e.g., `ThreadedMessagingProxyBase`).
- `MainThreadWorklet` prefix: Classes used for main thread worklets (e.g., `MainThreadWorkletReportingProxy`).

Thread hopping between the main (parent) thread and a worker thread is handled by proxy classes.

- `MessagingProxy` is the main (parent) thread side proxy that communicates to the worker thread.
- `ObjectProxy` is the worker thread side proxy that communicates to the main (parent) thread. `Object` indicates a worker/worklet JavaScript object on the parent execution context.

# References

- [Worker / Worklet Internals](https://docs.google.com/presentation/d/1GZJ3VnLIO_Pw0jr9nRw6_-trg68ol-AkliMxJ6jo6Bo/edit?usp=sharing) (April 19, 2018)
