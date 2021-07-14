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
webpage JavaScript can not interact with these scripts.

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
