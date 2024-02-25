## PasswordProtectionJavaScriptFeature

Password Protection (also known as "PhishGuard") is a feature that warns a user
when they type one of their saved passwords on an unrelated site that is not
known to be safe.

In order to detect these events, this feature needs to detect key presses and
text paste events, so that entered text can be compared to saved passwords.

Embedders of content/ can detect such events using a
RenderWidgetHost::InputEventObserver. Since WKWebView doesn't provide an
equivalent API for its embedders, these events can only be detected by
injecting JavaScript into each page. PasswordProtectionJavaScriptFeature
implements this JavaScript logic.

To ensure that a malicious page cannot interfere with this detection logic,
this JavaScript code is injected into an isolated world, where it is not
visible to page script.
