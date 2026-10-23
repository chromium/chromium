---
name: ios/accept-webframe-in-javascript-feature-apis
description: When designing execution or action APIs in JavaScriptFeature subclasses, accept web::WebFrame* instead of web::WebState* to enforce origin validation and frame binding
rubric: safety
severity: should_fix
citations:
  - path: ios/web/public/js_messaging/README.md
    heading: Be aware of the subtleties of value types returned by WKWebView
    quote: Using functions on the `WebFrame` to execute JavaScript ensures that the message will only be sent to the expected webpage. Sending JavaScript to the WKWebView directly is not bound to any particular webpage or domain and may execute on a different page than expected if a navigation occurs between the sending and execution of the message. However, messages sent to a WebFrame will be bound to that webpage and be dropped if the webpage goes away before the JavaScript is executed.
---

**Rule.** In `web::JavaScriptFeature` subclasses, design public execution and
action APIs to accept a `web::WebFrame*` rather than `web::WebState*`. Avoid
resolving target frames internally within the feature (e.g., via
`JavaScriptFeature::GetWebFramesManager(web_state)->GetMainWebFrame()`).
Requiring callers to supply the target `WebFrame*` allows callers to validate
the frame's security origin (`web_frame->GetSecurityOrigin()`) prior to dispatch
and guarantees that script execution (`CallJavaScriptFunction` or
`CallAsyncJavaScriptFunction`) is bound to that specific frame and cancelled if
the frame navigates or is destroyed.

**Why.** Resolving a frame from `WebState` inside a `JavaScriptFeature` prevents
callers from validating the target frame's security origin beforehand and
introduces a time-of-check to time-of-use (TOCTOU) hazard: if navigation occurs
between caller origin checks and script dispatch, the script executes on an
untrusted or navigated page. In contrast, passing `web::WebFrame*` ensures
script execution is bound strictly to that frame's security context and is
safely cancelled if the frame navigates or is removed.

## Examples

Bad:

```cpp
// BAD: Accepts WebState* and resolves the target frame internally,
// bypassing caller origin checks and risking dispatch to a navigated domain.
void FooJavaScriptFeature::PerformAction(
    web::WebState* web_state,
    base::Value::Dict params,
    base::OnceCallback<void(bool)> callback) {
  web::WebFramesManager* manager =
      JavaScriptFeature::GetWebFramesManager(web_state);
  web::WebFrame* main_frame = manager ? manager->GetMainWebFrame() : nullptr;
  CallJavaScriptFunction(main_frame, "foo.performAction",
                         {std::move(params)}, std::move(callback));
}
```

Good:

```cpp
// GOOD: Accepts WebFrame*, ensuring callers can validate security origin
// and binding execution strictly to that specific frame.
void FooJavaScriptFeature::PerformAction(
    web::WebFrame* web_frame,
    base::Value::Dict params,
    base::OnceCallback<void(bool)> callback) {
  CHECK(web_frame);
  CallJavaScriptFunction(web_frame, "foo.performAction",
                         {std::move(params)}, std::move(callback));
}
```
