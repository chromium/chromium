# execution_context

[Rendered](https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/core/execution_context/README.md)

The `renderer/core/execution_context` directory contains the implementation of ExecutionContext and SecurityContext.


# ExecutionContext

ExecutionContext is an abstraction of ES global environment and supposed to be inherited by Window and Workers (however, it's inherited by Document instead of DOMWindow/LocalDOMWindow for historical reasons).

HTML defines [environment settings object](https://html.spec.whatwg.org/C/webappapis.html#environment-settings-object), and ExecutionContext is somewhat similar to this object, however, there is unfortunately no exact correspondence between them because ExecutionContext has grown step by step since before environment settings object got defined as today's one.

An ExecutionContext may be [paused or frozen](https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/core/execution_context/PausingAndFreezing.md)


# SecurityContext

SecurityContext represents an environment that supports security origin and CSP.
