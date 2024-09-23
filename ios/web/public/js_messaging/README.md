# Using TypeScript in Chrome for iOS

tl;dr A high level overview of using TypeScript and `JavaScriptFeature` to
build features which interact with the web page contents on Chrome for iOS.

Chrome for iOS doesn't have access to the renderer code so many browser features
require JavaScript in order to interact with the webpage. Please use TypeScript
for all new feature development.

Slides covering JavaScriptFeature are available [here](https://docs.google.com/presentation/d/1HKdi7CGtNTGhMcCscpX_LVFZ8iLTtXjpSQ-980N7J38/edit?usp=sharing).
Note that this presentation is somewhat outdated, but still provides a good
overview.

[TOC]
## Background

It is important to first understand the following concepts in order to properly
design a feature using JavaScript.

### JavaScript vs TypeScript

Sometimes the terms "JavaScript" and "TypeScript" are used interchangeably. Even
though feature developers write TypeScript, the TypeScript is compiled into
plain JavaScript at compile time. All new scripts should be written as
TypeScript which provides more assurances of code correctness and type safety
than plain JavaScript.

### JavaScript Injection

JavaScript is considered to be "injected" when it is added to every webpage by
WebKit.

JavaScript injection is configured at the `BrowserState` level. That is, it is
injected unconditionally for every `WebState` associated with that
`BrowserState`.

This may mean that the JavaScript doesn’t do anything when it is injected, but
instead adds functionality which is exposed (through `__gCrWeb`) for later use.

Note: WebKit uses the "WKUserScript" class to configure injected JavaScript.

### JavaScript Execution

JavaScript is considered to be "executed" when triggered by a feature or user
action at runtime. The script may be generated dynamically or consist of the
contents of a file within the resource bundle. "Executed" JavaScript refers to
all application JavaScript which is not "injected".

This allows the native code to interact with and manipulate the contents of the
webpage in response to feature logic at runtime.

### Script Message Handlers

A script message handler allows JavaScript to securely send messages to the
native code.

### Content Worlds

A "content world"" can be thought of as a JavaScript namespace.

A `JavaScriptFeature` instance can choose to inject its JavaScript into a
particular world. All worlds have access to read and modify the same underlying
webpage DOM, but can not interact with the JavaScript state in different
worlds.

#### Page Content World

The "page content" world contains the webpage JavaScript. Application JavaScript
injected here can interact with the JavaScript of the webpage and expose
functions for the webpage JavaScript to call directly. Only scripts acting as
polyfills or which expect to be called by the scripts of the webpages should run
in the page content world.

#### Isolated Worlds

Application JavaScript can choose to be injected into its own world, separate
from the Page Content World. This is considered an "isolated" world. The
webpage JavaScript can not interact with these scripts under normal
circumstances. (However, a web page that exploits a renderer bug in order to
execute arbitrary code is also able to interact with scripts in isolated worlds,
including the ability to manipulate and send messages from isolated worlds so
care should always be taken when processing messages sent from JavaScript.)

## web::JavaScriptFeature

The `JavaScriptFeature` class encapsules all the above concepts to make it easy
to build and configure a feature which needs to interact with the webpage.

All injected JavaScript files are considered part of a "feature" as it needs to
be specified which world they should be injected into.

A feature may:

*   unconditionally modify the DOM or perform an action with no native<->JS
    communication
    *   Ex: Perform an action based on an event listener
*   add JavaScript functions to the world
    *   Ex: `common.js` adds the `__gCrWeb.common.*` functions
*   expose native C++ functions which call injected JavaScript or executes other
    JavaScript
*   add a script message handler and handle messages received from the
    JavaScript in the associated content world

### FeatureScript

A `FeatureScript` represents a single JavaScript file from the application
bundle which is injected.

Although the script runs immediately upon injection, the feature logic is
generally run later triggered by event listeners or a function call from the
native application.

#### FeatureScript::InjectionTime

The InjectionTime is when the script should be ran during the loading of the
webpage's DOM.

Scripts configured with `kDocumentStart` can expose functions, but the DOM will
not yet be available. Overrides of functions in the page content world which
the webpage may call should be done at this time.

Scripts configured with `kDocumentEnd` will run after the DOM is available. This
can be useful for scripts which may make a pass over the DOM to manipulate it
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

### JavaScriptFeature::CallJavaScriptFunction

`JavaScriptFeature` exposes protected `CallJavaScriptFunction` APIs. These are
to be used within specialized `JavaScriptFeature` subclasses in order to call
into the JavaScript injected by the feature. This is necessary in order to
ensure that the JavaScript execution occurs in the same content world which the
feature scripts were injected into.

JavaScript injected by a `JavaScriptFeature` should not be called outside of the
specialized `JavaScriptFeature` subclass.

### Script Message Handlers

Script message handlers are configured by creating a `JavaScriptFeature`
subclass and returning a handler name in `GetScriptMessageHandlerName`.

Any responses will be routed to `ScriptMessageReceived`.

From the JavaScript side, these messages are sent using
`__gCrWeb.common.sendWebKitMessage` in older scripts and `sendWebKitMessage`
from `//ios/web/public/js_messaging/resources/utils.js` in modern scripts.

Note that the handler name must be unique application-wide. WebKit will throw
an exception while setting up the handler if a name is already registered.

### JavaScriptFeature Lifecycle

`JavaScriptFeature`s conceptually live at the `BrowserState` layer.

Simple features which hold no state can live statically within the application
and may be shared across the normal and incognito `BrowserState`s.
`base::NoDestructor` and a static `GetInstance()` method can be used for these
features.

More complex features can be owned by the `BrowserState`s themselves as
UserData. For example, see `ContextMenuJavaScriptFeature`.

In order to inject JavaScript, features which are implemented in `//ios/chrome`
must be registered at application launch time in
`ChromeWebClient::GetJavaScriptFeatures`.

## Writing TypeScript

All new scripts used on iOS should be written in TypeScript. TypeScript is
written in a very similar manner to JavaScript, but additionally allows for
error detection at compile time.

Note that ES6 import/export statements are supported so your feature TypeScript
can be split across multiple files. Reference other files using the full path,
similar to other imports in native code. For example:

    import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

Note that the import uses the ".js" extension, even if the file is written as
TypeScript with a ".ts" file extension.

Additionally, it is important to realize that import/export statements are also
stripped away during compile time using [Rollup]. This may have unexpected side
effects like variables defined in a common file not being shared as expected.
State should be stored globally if it needs to be maintained across multiple
files.

[Rollup]: rollupjs.org

### compile_ts

Use [compile_ts] to compile TypeScript into JavaScript which is imported by
many other scripts. Use `compile_ts` directly when your feature has multiple
files or you are creating a script library to be used as a dep for many other
script targets. See also `optimize_ts` below which might be a better fit.

[compile_ts]: https://source.chromium.org/chromium/chromium/src/+/main:ios/web/public/js_messaging/compile_ts.gni

### optimize_js

[optimize_js] bundles one or more JavaScript files into a single output file in
the application resources directory. These targets can depend on any number of
dependent compiled script targets by listed `compile_ts` targets in `deps`.

By default, it also minimizes the scripts.
However, minimization can be disabled for debugging by setting `minify_scripts`
at the top of `optimize_js.gni` to `false`.

Use optimize_js directly if your
feature has multiple script files, otherwise, see `optimize_ts` below handles
the most common case.

[optimize_js]: https://source.chromium.org/chromium/chromium/src/+/main:ios/web/public/js_messaging/optimize_js.gni

### optimize_ts

[optimize_ts] compiles a single TypeScript or JavaScript file and copies it to
the application resources directory. This handles the common case where a
feature has a single TypeScript file which needs to be compiled into JavaScript
and copied into the application bundle for use by a JavaScriptFeature instance.

[optimize_ts]: https://source.chromium.org/chromium/chromium/src/+/main:ios/web/public/js_messaging/optimize_ts.gni

### Guidelines

*   Adhere to the [TypeScript styleguide].
*   Use [existing TypeScript] as reference.
*   Split your code into multiple files using export/import to improve code
    organization and readability.

[TypeScript styleguide]: https://google.github.io/styleguide/tsguide.html
[existing TypeScript]: https://source.chromium.org/search?q=file:%5Eios%20file:%22.ts%22&sq=

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
the message. For example, use `web::ScriptMessage::request_url()` or
`WKScriptMessage.frameInfo.request.URL`. Do not rely on url information in the
message itself as it can be easily spoofed.

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

  const base::Value::Dict& dict = message.body()->GetDict();

  const std::string* event_type = dict.FindString("eventType");
  if (!event_type || event_type->empty()) {
    return;
  }

  std::string* text = dict.FindString("text");
  if (!text || text->empty()) {
    return;
  }

  // Now, you can *carefully* use `event_type` and `text` as non-empty strings.
}
```

### Safely handle known-bad input

When validating a message, don’t  use `CHECK`. We do not want the input
validation mechanism to become an easy way for malicious JavaScript to kill the
browser process. It is usually better to ignore the bad input, or when possible,
to immediately destroy and re-create the WKWebView that sent invalid input.
Destroying and re-creating a WKWebView in this situation is non-trivial so this
is not currently done on iOS; however, receiving an invalid input that purports
to be from an isolated world is a strong indication of a compromised renderer.
If there is no graceful way to handle a particular invalid message, a
`CHECK`-induced crash is still better than allowing the browser process to
become compromised. Importantly, a `DCHECK` is not appropriate in this
situation, since it will not protect users on official builds.

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
and when receiving results from `WebFrame::CallJavaScriptFunction*` and
`WebFrame::ExecuteJavaScript`. Using functions on the `WebFrame` to execute
JavaScript ensures that the message will only be sent to the expected webpage.
Sending JavaScript to the WKWebView directly is not bound to any particular
webpage or domain and may execute on a different page than expected
if a navigation occurs between the sending and execution of the message.
However, messages sent to a WebFrame will be bound to that webpage and be
dropped if the webpage goes away before the JavaScript is executed.

[legacy IPC docs]: https://www.chromium.org/Home/chromium-security/education/security-tips-for-ipc
[Mojo IPC docs]: https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/security/mojo.md
[JS ObjC conversions]: https://developer.apple.com/documentation/javascriptcore/jsvalue?language=objc
[ValueResultFromWKResult]: https://source.chromium.org/chromium/chromium/src/+/main:ios/web/public/js_messaging/web_view_js_utils.h;l=36
