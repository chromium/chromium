---
name: ios/avoid-unmanaged-high-frequency-listeners-in-web-scripts
rubric: safety
severity: should_fix
---

**Rule.** Do not register permanent, top-level event listeners on `window` or
`document` for high-frequency events (such as `'pointermove'`, `'mousemove'`,
`'touchmove'`, or `'scroll'`) in injected web scripts without managing their
lifecycle or confirming their necessity. If high-frequency tracking is required,
bind the listener to an explicit lifecycle (e.g. via `AbortSignal`,
`removeEventListener`, or transient task scopes) that cleans it up when the
active operation completes.

**Why.** Injected web scripts (e.g. `web::JavaScriptFeature`) execute across web
frames for the entire lifetime of the document. Attaching permanent, passive
listeners for high-frequency input events fires callbacks and retains state on
every touch or cursor movement, introducing continuous main-thread overhead,
input latency, and battery drain on mobile devices.

## Examples

Bad:

```cpp
// BAD: Registering permanent top-level listeners for high-frequency input.
let lastPointerPosition: Point|null = null;
window.addEventListener('pointermove', (event: PointerEvent) => {
  lastPointerPosition = {x: event.pageX, y: event.pageY};
});
```

Good:

```cpp
// GOOD: When high-frequency tracking is necessary, manage its lifecycle explicitly:
function trackPointerDuringOperation(signal: AbortSignal): void {
  window.addEventListener('pointermove', handlePointerMove, {signal});
}
```
