# Using JavaScript in Chrome for iOS Features

tl;dr A high level overview of using `JavaScriptFeature` in ``//ios/chrome`` to
build features which interact with the web page contents.

Chrome for iOS doesn't have access to the renderer code so many browser features
require JavaScript in order to interact with the webpage.

Slides covering this topic are available [here](https://docs.google.com/presentation/d/1HKdi7CGtNTGhMcCscpX_LVFZ8iLTtXjpSQ-980N7J38/edit?usp=sharing).

## Background

It is important to first understand the following concepts in order to properly
design a feature using JavaScript.

### JavaScript Injection

JavaScript is considered to be "injected" when it is added to every webpage by
WebKit.

JavaScript injection is configured at the BrowserState level. That is, it is
injected unconditionally for every `WebState` associated with that
`BrowserState`.

This may mean that the JavaScript doesn’t do anything when it is injected, but
instead it may add functions which are exposed (through `__gCrWeb`) for later
use.

Note that WebKit uses the terminology UserScript for this injected JavaScript.

### JavaScript Execution

JavaScript is considered to be "executed" when triggered by a feature or user
action at runtime. The script may be generated dynamically or consist of the
contents of a file within the resource bundle. "Executed" JavaScript refers to
all application JavaScript which is not "injected".

This allows the native code to interact with and manipulate the contents of the
webpage in response to feature logic.

### Script Message Handlers

A script message handler allows JavaScript to securely send messages to the
native code.

### Content Worlds

A content world can be thought of as a JavaScript namespace.

The application can choose to inject its JavaScript into any world. All worlds
have access to read and modify the same underlying webpage DOM.

#### Page Content World

The "page content" world contains the webpage JavaScript. Application JavaScript
injected here can interact with the JavaScript of the webpage and expose
functions for the webpage JavaScript to call directly.

#### Isolated Worlds

Application JavaScript can choose to be injected into its own world, separate
from the Page Content World. This is considered an "isolated" world. The
webpage JavaScript can not interact with these scripts under normal
circumstances. However, a web page that exploits a renderer bug in order to
execute arbitrary code is also able to interact with scripts in isolated worlds,
including the ability to manipulate and send messages from isolated worlds. 

NOTE: Isolated worlds are only supported on iOS 14 and later.

## web::JavaScriptFeature

The `JavaScriptFeature` class encapsules all the above concepts to make it easy
to build and configure a feature which needs to interact with the webpage.

All injected JavaScript files are considered a "feature" as they need to specify
which world they should be injected into. But, features can also expose much
more functionality and store state as necessary.

A feature may:

*   unconditionally modify the DOM or perform an action with no native<->JS
    communication
    *   Ex: Perform an action based on an event listener
*   simply add JavaScript functions to the world
    *   Ex: `common.js` adds the `__gCrWeb.common` functions
*   expose native C++ functions which call injected JavaScript or executes other
    JavaScript
*   add a script message handler and handle

### FeatureScript

A `FeatureScript` represents a JavaScript file from the application bundle which
is injected.

The code within the file may run immediately, be tied to event listeners, or
simply make functions available for later execution.

#### FeatureScript::InjectionTime

The InjectionTime is when the script should be ran during the loading of the
webpage's DOM.

Scripts configured with `kDocumentStart` can create functions. Overrides of
functions in the page content world which the webpage may call should be done at
this time.

Scripts configured with `kDocumentEnd` will run after the DOM is available. This
can be useful for scripts which may make pass over the DOM to manipulate it
only once after page load. Or to scan the webpage for some metadata and send it
back to the native application.

#### FeatureScript::ReinjectionBehavior

To prevent lost event listeners, they must be setup in a `FeatureScript` which
specifies `kReinjectOnDocumentRecreation`.

If a feature also stores global state at the window object level, splitting the
event listener JavaScript into its own file is a good approach.

#### FeatureScript Constructor Replacements

The `replacements` map can be used to replace constants in the JavaScript
resource file with values at runtime. (For example, a localized string based on
the current language of the user.)

NOTE: Since `JavaScriptFeature`s live at the `BrowserState` level, these
replacements only have `BrowserState` granularity. In order to replace a
constant with `WebState` granularity, do so by exposing a function on a
`JavaScriptFeature` subclass to call into the injected JavaScript and set the
proper replacement value.

### JavaScriptFeature::ExecuteJavaScript

`JavaScriptFeature` exposes `protected CallJavaScriptFunction` APIs. These are
to be used within specialized `JavaScriptFeature` subclasses in order to call
into the injected JavaScript.

This is necessary in order to ensure that the JavaScript execution occurs in the
same content world which the features scripts were injected into.

JavaScript injected by a `JavaScriptFeature` should not be called outside of the
specialized `JavaScriptFeature` subclass.

### Script Message Handlers

Script message handlers are configured by creating a `JavaScriptFeature`
subclass and returning a handler name in `GetScriptMessageHandlerName`.

Any responses will be routed to `ScriptMessageReceived`.

From the JavaScript side, these messages are sent using
`__gCrWeb.common.sendWebKitMessage`

Note that the handler name must be application wide unique. WebKit will thrown
an exception if a name is already registered.

### JavaScriptFeature Lifecycle

`JavaScriptFeature`s conceptually live at the `BrowserState` layer.

Simple features which hold no state can live statically within the application
and be shared across the normal and incognito `BrowserState`s. 
`base::NoDestructor` and a static `GetInstance()` method can be used for these
features.

More complex features can be owned by the `BrowserState`s themselves as
UserData.

In order to inject JavaScript, //ios/chrome features must be registered at
application launch time in `ChromeWebClient::GetJavaScriptFeatures`.

## Best Practices for JavaScript Messaging

Messaging between JavaScript code in a renderer process and native code in
the browser process is a form of inter-process communication (IPC). Since
renderers are responsible for processing untrustworthy input provided by
web pages, they are more vulnerable to security bugs that lead to arbitrary
code execution. Messaging between renderers and browsers can provide a pathway
for compromised renderers to compromise the browser process as well.

Following the best practices in this section is one way to reduce the risk of
introducing security bugs. These best practices are inspired by the security
guidelines for [Chromium legacy IPC][legacy IPC docs] and
[Mojo IPC][Mojo IPC docs].

### Documentation

Message handlers should include comments describing their purpose and the
arguments they take, including the expected type of each argument.

For example:
```
void MyJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  // This message is sent whenever a password field is selected.
  // Expected arguments:
  // numImages: (double) The number of <img> tags on the page.
  // passwordLength: (double) The length of the currently entered password.
  // elementId: (string) The selected password field’s id attribute.

}
```

### Trust only the browser process

JavaScript messages are sent from WebContent (WebKit renderer) processes, which
may have been compromised. Messages should be treated as if they might have been
sent by an attacker. These messages are untrustworthy input.

Determine the origin of a message using trusted data rather than the content of
the message. For example, use `web::ScriptMessage`’s `request_url`_ field, or
`WKScriptMessage.frameInfo.request.URL`.

### Sanitize and validate messages

Do not assume that any part of the message is formatted in a particular way or
is of a particular type. Instead, validate the format of each part of the
message.

For example:
```
void MyJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!message.body() || !message.body()->is_dict()) {
    return;
  }

  std::string* event_type = message.body()->FindStringKey("eventType");
  if (!event_type || event_type->empty()) {
    return;
  }

  std::string* text = message.body()->FindStringKey("text");
  if (!text || text->empty()) {
    return;
  }

  // Now use |eventType| and |text|.
}
```

### Safely handle known-bad input

When validating a message, don’t simply use `CHECK`. We do not want the input
validation mechanism to become an easy way for malicious JavaScript to kill the
browser process. It is usually better to ignore the bad input, or when possible,
to immediately destroy and re-create the WKWebView that sent invalid input.
Destroying and re-creating a WKWebView in this situation is non-trivial so this
is not currently done on iOS; however, receiving an invalid input that purports
to be from an isolated world is a strong indication of a compromised renderer.
If there is no graceful way to handle a particular invalid message, a
`CHECK`-induced crash is still better than allowing the browser process to
become compromised. Importantly, a `DCHECK` is not appropriate in this situation,
since it will not protect users on official builds.

### Be aware of the subtleties of value types returned by WKWebView

In JavaScript messaging, all numbers are represented as doubles, even if they
have no fractional part. Make sure parsing logic handles all possible double
values, including `NaN` and infinity.

JavaScript’s `BigInt` type is not supported, and gets converted to `nil`. A
`null` value in JavaScript messaging gets converted to `[NSNull null]`. Dates in
JavaScript are converted to `NSDate`.

While conversions between JavaScript types and Objective-C types are documented
[here][JS ObjC conversions], prefer using
[`ValueResultFromWKResult`][ValueResultFromWKResult] to convert values received
from WKWebView to `base::Value`, rather than directly working with
WKWebView-provided values. This already happens when working with
`web::ScriptMessage` in overrides of `JavaScriptFeature::ScriptMessageReceived`,
and when using `WebStateImpl::ExecuteJavaScript`. Always use
`WebFrame::CallJavaScriptFunction` rather than directly calling
`[WKWebView evaluateJavaScript]`, to avoid having to manually handle conversion.

[legacy IPC docs]: https://www.chromium.org/Home/chromium-security/education/security-tips-for-ipc
[Mojo IPC docs]: https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/security/mojo.md
[JS ObjC conversions]: https://developer.apple.com/documentation/javascriptcore/jsvalue?language=objc
[ValueResultFromWKResult]: https://source.chromium.org/chromium/chromium/src/+/main:ios/web/js_messaging/web_view_js_utils.h;l=36
