<style>
.note::before {
  content: 'Note: ';
  font-variant: small-caps;
  font-style: italic;
}

.doc h1 {
  margin: 0;
}
</style>

# WebUI Explainer

[TOC]

<a name="What_is_webui"></a>
## What is "WebUI"?

"WebUI" is a term used to loosely describe **parts of Chrome's UI
implemented with web technologies** (i.e. HTML, CSS, JavaScript).

Examples of WebUI in Chromium:

* Settings (chrome://settings)
* History (chrome://history)
* Downloads (chrome://downloads)

<div class="note">
Not all web-based UIs in Chrome have chrome:// URLs.
</div>

This document explains how WebUI works.

<a name="bindings"></a>
## What's different from a web page?

WebUIs are granted super powers so that they can manage Chrome itself. For
example, it'd be very hard to implement the Settings UI without access to many
different privacy and security sensitive services. Access to these services are
not granted by default.

Only special URLs are granted WebUI "bindings" via the child security process.

Specifically, these bindings:

* give a renderer access to load [`chrome:`](#chrome_urls) URLS
  * this is helpful for shared libraries, i.e. `chrome://resources/`
* allow the browser to execute arbitrary JavaScript in that renderer via
  [`CallJavascriptFunction()`](#CallJavascriptFunction)
* allow communicating from the renderer to the browser with
  [`chrome.send()`](#chrome_send) and friends
* ignore content settings regarding showing images or executing JavaScript

<a name="chrome_urls"></a>
## How `chrome:` URLs work

<div class="note">
A URL is of the format &lt;protocol&gt;://&lt;host&gt;/&lt;path&gt;.
</div>

A `chrome:` URL loads a file from disk, memory, or can respond dynamically.

Because Chrome UIs generally need access to the browser (not just the current
tab), much of the C++ that handles requests or takes actions lives in the
browser process. The browser has many more privileges than a renderer (which is
sandboxed and doesn't have file access), so access is only granted for certain
URLs.

### `chrome:` protocol

Chrome recognizes a list of special protocols, which it registers while starting
up.

Examples:

* devtools:
* chrome-extensions:
* chrome:
* file:
* view-source:

This document mainly cares about the **chrome:** protocol, but others can also
be granted [WebUI bindings](#bindings) or have special
properties.

### `chrome:` hosts

After registering the `chrome:` protocol, a set of factories are created. These
factories contain a list of valid host names. A valid hostname generates a
controller.

In the case of `chrome:` URLs, these factories are registered early in the
browser process lifecycle.

```c++
// ChromeBrowserMainParts::PreMainMessageLoopRunImpl():
content::WebUIControllerFactory::RegisterFactory(
   ChromeWebUIControllerFactory::GetInstance());
```

When a URL is requested, a new renderer is created to load the URL, and a
corresponding class in the browser is set up to handle messages from the
renderer to the browser (a `RenderFrameHost`).

The URL of the request is inspected:

```c++
if (url.SchemeIs("chrome") && url.host_piece() == "donuts")  // chrome://donuts
  return &NewWebUI<DonutsUI>;
return nullptr;  // Not a known host; no special access.
```

and if a factory knows how to handle a host (returns a `WebUIFactoryFunction`),
the navigation machinery [grants the renderer process WebUI
bindings](#bindings) via the child security policy.

```c++
// RenderFrameHostImpl::AllowBindings():
if (bindings_flags & BINDINGS_POLICY_WEB_UI) {
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantWebUIBindings(
      GetProcess()->GetID());
}
```

The factory creates a [`WebUIController`](#WebUIController) for a tab.
Here's an example:

```c++
// Controller for chrome://donuts.
class DonutsUI : public content::WebUIController {
 public:
  DonutsUI(content::WebUI* web_ui) : content::WebUIController(web_ui) {
    content::WebUIDataSource* source =
        content::WebUIDataSource::Create("donuts");  // "donuts" == hostname
    source->AddString("mmmDonuts", "Mmm, donuts!");  // Translations.
    source->AddResourcePath("", IDR_DONUTS_HTML);  // Home page.
    content::WebUIDataSource::Add(source);

    // Handles messages from JavaScript to C++ via chrome.send().
    web_ui->AddMessageHandler(std::make_unique<OvenHandler>());
  }
};
```

If we assume the contents of `IDR_DONUTS_HTML` yields:

```html
<h1>$i18n{mmmDonuts}</h1>
```

Visiting `chrome://donuts` should show in something like:

<div style="border: 1px solid black; padding: 10px;">
<h1>Mmmm, donuts!</h1>
</div>

Delicious success.

By default $i18n{} escapes strings for HTML. $i18nRaw{} can be used for
translations that embed HTML, and $i18nPolymer{} can be used for Polymer
bindings. See
[this comment](https://bugs.chromium.org/p/chromium/issues/detail?id=1010815#c1)
for more information.

## C++ classes

### WebUI

`WebUI` is a high-level class and pretty much all HTML-based Chrome UIs have
one. `WebUI` lives in the browser process, and is owned by a `RenderFrameHost`.
`WebUI`s have a concrete implementation (`WebUIImpl`) in `content/` and are
created in response to navigation events.

A `WebUI` knows very little about the page it's showing, and it owns a
[`WebUIController`](#WebUIController) that is set after creation based on the
hostname of a requested URL.

A `WebUI` *can* handle messages itself, but often defers these duties to
separate [`WebUIMessageHandler`](#WebUIMessageHandler)s, which are generally
designed for handling messages on certain topics.

A `WebUI` can be created speculatively, and are generally fairly lightweight.
Heavier duty stuff like hard initialization logic or accessing services that may
have side effects are more commonly done in a
[`WebUIController`](#WebUIController) or
[`WebUIMessageHandler`s](#WebUIMessageHandler).

`WebUI` are created synchronously on the UI thread in response to a URL request,
and are re-used where possible between navigations (i.e. refreshing a page).
Because they run in a separate process and can exist before a corresponding
renderer process has been created, special care is required to communicate with
the renderer if reliable message passing is required.

<a name="WebUIController"></a>
### WebUIController

A `WebUIController` is the brains of the operation, and is responsible for
application-specific logic, setting up translations and resources, creating
message handlers, and potentially responding to requests dynamically. In complex
pages, logic is often split across multiple
[`WebUIMessageHandler`s](#WebUIMessageHandler) instead of solely in the
controller for organizational benefits.

A `WebUIController` is owned by a [`WebUI`](#WebUI), and is created and set on
an existing [`WebUI`](#WebUI) when the correct one is determined via URL
inspection (i.e. chrome://settings creates a generic [`WebUI`](#WebUI) with a
settings-specific `WebUIController`).

<a name="WebUIDataSource"></a>
### WebUIDataSource

The `WebUIDataSource` class provides a place for data to live for WebUI pages.

Examples types of data stored in this class are:

* static resources (i.e. .html files packed into bundles and pulled off of disk)
* translations
* dynamic feature values (i.e. whether a feature is enabled)

Data sources are set up in the browser process (in C++) and are accessed by
loading URLs from the renderer.

Below is an example of a simple data source (in this case, Chrome's history
page):

```c++
content::WebUIDataSource* source = content::WebUIDataSource::Create("history");

source->AddResourcePath("sign_in_promo.svg", IDR_HISTORY_SIGN_IN_PROMO_SVG);
source->AddResourcePath("synced_tabs.html", IDR_HISTORY_SYNCED_TABS_HTML);

source->AddString("title", IDS_HISTORY_TITLE);
source->AddString("moreFromThisSite", IDS_HISTORY_MORE_FROM_THIS_SITE);

source->AddBoolean("showDateRanges",
    base::FeatureList::IsEnabled(features::kHistoryShowDateRanges));

webui::SetupWebUIDataSource(
    source, base::make_span(kHistoryResources, kHistoryResourcesSize),
    kGeneratedPath, IDR_HISTORY_HISTORY_HTML);

content::WebUIDataSource::Add(source);
```

For more about each of the methods called on `WebUIDataSource` and the utility
method that performs additional configuration, see [DataSources](#DataSources)
and [WebUIDataSourceUtils](#WebUIDataSourceUtils)

<a name="WebUIMessageHandler"></a>
### WebUIMessageHandler

Because some pages have many messages or share code that sends messages, message
handling is often split into discrete classes called `WebUIMessageHandler`s.
These handlers respond to specific invocations from JavaScript.

So, the given C++ code:

```c++
void OvenHandler::RegisterMessages() {
  web_ui()->RegisterMessageHandler("bakeDonuts",
      base::Bind(&OvenHandler::HandleBakeDonuts, base::Unretained(this)));
}

void OvenHandler::HandleBakeDonuts(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1u, args->GetSize());
  // JavaScript numbers are doubles.
  double num_donuts = args->GetList()[0].GetDouble();
  GetOven()->BakeDonuts(static_cast<int>(num_donuts));
}
```

Can be triggered in JavaScript with this example code:

```js
$('bakeDonutsButton').onclick = function() {
  chrome.send('bakeDonuts', [5]);  // bake 5 donuts!
};
```

<a name="DataSources">
## Data Sources

<a name="Create"></a>
### WebUIDataSource::Create()

This is a factory method required to create a WebUIDataSource instance. The
argument to `Create()` is typically the host name of the page. Caller owns the
result.

<a name="Add"></a>
### WebUIDataSource::Add()

Once you've created and added some things to a data source, it'll need to be
"added". This means transferring ownership. In practice, the data source is
created in the browser process on the UI thread and transferred to the IO
thread. Additionally, calling `Add()` will overwrite any existing data source
with the same name.

<div class="note">
It's unsafe to keep references to a <code>WebUIDataSource</code> after calling
<code>Add()</code>. Don't do this.
</div>

<a name="AddLocalizedString"></a>
### WebUIDataSource::AddLocalizedString()

Using an int reference to a grit string (starts with "IDS" and lives in a .grd
or .grdp file), adding a string with a key name will be possible to reference
via the `$i18n{}` syntax (and will be replaced when requested) or later
dynamically in JavaScript via `loadTimeData.getString()` (or `getStringF`).

<a name="AddResourcePath"></a>
### WebUIDataSource::AddResourcePath()

Using an int reference to a grit resource (starts with "IDR" and lives in a .grd
or .grdp file), adds a resource to the UI with the specified path.

It's generally a good idea to call <code>AddResourcePath()</code> with the empty
path and a resource ID that should be served as the "catch all" resource to
respond with. This resource will be served for requests like "chrome://history",
or "chrome://history/pathThatDoesNotExist". It will not be served for requests
that look like they are attempting to fetch a specific file, like
"chrome://history/file\_that\_does\_not\_exist.js". This is so that if a user
enters a typo when trying to load a subpage like "chrome://history/syncedTabs"
they will be redirected to the main history page, instead of seeing an error,
but incorrect imports in the source code will fail, so that they can be more
easily found and corrected.

<a name="AddBoolean"></a>
### WebUIDataSource::AddBoolean()

Often a page needs to know whether a feature is enabled. This is a good use case
for `WebUIDataSource::AddBoolean()`.  Then, in the Javascript, one can write
code like this:

```js
if (loadTimeData.getBoolean('myFeatureIsEnabled')) {
  ...
}
```

<div class="note">
Data sources are not recreated on refresh, and therefore values that are dynamic
(i.e. that can change while Chrome is running) may easily become stale. It may
be preferable to use <code>cr.sendWithPromise()</code> to initialize dynamic
values and call <code>FireWebUIListener()</code> to update them.

If you really want or need to use <code>AddBoolean()</code> for a dynamic value,
make sure to call <code>WebUIDataSource::Update()</code> when the value changes.
</div>

<a name="WebUIDataSourceUtils"></a>
## WebUI utils for working with data sources

chrome/browser/ui/webui/webui\_util.\* contains a number of methods to simplify
common configuration tasks.

<a name="AddLocalizedStringsBulk"></a>
### webui::AddLocalizedStringsBulk()

Many Web UI data sources need to be set up with a large number of localized
strings. Instead of repeatedly calling <code>AddLocalizedString()</code>, create
an array of all the strings and use <code>AddLocalizedStringsBulk()</code>:

```c++
  static constexpr webui::LocalizedString kStrings[] = {
      // Localized strings (alphabetical order).
      {"actionMenuDescription", IDS_HISTORY_ACTION_MENU_DESCRIPTION},
      {"ariaRoleDescription", IDS_HISTORY_ARIA_ROLE_DESCRIPTION},
      {"bookmarked", IDS_HISTORY_ENTRY_BOOKMARKED},
  };
  AddLocalizedStringsBulk(source, kStrings);
```

<a name="AddResourcePathsBulk"></a>
### webui::AddResourcePathsBulk()

Similar to the localized strings, many Web UIs need to add a large number of
resource paths. In this case, use <code>AddResourcePathsBulk()</code> to
replace repeated calls to <code>AddResourcePath()</code>. There are two
versions. One works almost identically to the strings case:

```c++
  static constexpr webui::ResourcePath kPdfResources[] = {
      {"pdf/browser_api.js", IDR_PDF_BROWSER_API_JS},
      {"pdf/constants.js", IDR_PDF_CONSTANTS_JS},
      {"pdf/controller.js", IDR_PDF_CONTROLLER_JS},
  };
  webui::AddResourcePathsBulk(source, kStrings);
```

The second version instead accepts a span of <code>GritResourceMap</code> so
that it can directly use constants defined by autogenerated grit resources map
header files. For example, the autogenerated print\_preview\_resources\_map.h
header defines a <code>GritResourceMap</code> named
<code>kPrintPreviewResources</code> and a
<code>size\_t kPrintPreviewResourcesSize</code>. All the resources in this
resource map can be added as follows:

```c++
  webui::AddResourcePathsBulk(
      source,
      base::make_span(kPrintPreviewResources, kPrintPreviewResourcesSize));
```

<a name="SetupWebUIDataSource"></a>
### webui::SetupWebUIDataSource() and webui::SetupBundledWebUIDataSource()

These methods perform common configuration tasks on a data source for a Web UI
that uses JS modules. When creating a Web UI that uses JS modules, use these
utilities instead of duplicating the configuration steps they perform elsewhere.
Specific setup steps performed by these utilities include:

* Setting the content security policy to allow the data source to load only
  resources from its own host (e.g. chrome://history), chrome://resources, and
  chrome://test (used to load test files).
* Enabling i18n template replacements by calling <code>UseStringsJs()</code> and
  <code>EnableReplaceI18nInJS()</code> on the data source.
* Adding the test loader files to the data source, so that test files can be
  loaded as JS modules.
* Setting the resource to load for the empty path.

The version for non-bundled UIs (<code>SetupWebUIDataSource()</code>) also adds
all resources in a GritResourceMap.

The version for bundled UIs (<code>SetupBundledWebUIDataSource()</code>) adds
a single specified bundled resource. Note that this version is only defined when
the optimize_webui build flag is enabled.

## Browser (C++) &rarr; Renderer (JS)

<a name="AllowJavascript"></a>
### WebUIMessageHandler::AllowJavascript()

A tab that has been used for settings UI may be reloaded, or may navigate to an
external origin. In both cases, one does not want callbacks from C++ to
Javascript to run. In the former case, the callbacks will occur when the
Javascript doesn't expect them. In the latter case, sensitive information may be
delivered to an untrusted origin.

Therefore each message handler maintains
[a boolean](https://cs.chromium.org/search/?q=WebUIMessageHandler::javascript_allowed_)
that describes whether delivering callbacks to Javascript is currently
appropriate. This boolean is set by calling `AllowJavascript`, which should be
done when handling a call from Javascript, because that indicates that the page
is ready for the subsequent callback. (See
[design doc](https://drive.google.com/open?id=1z1diKvwgMmn4YFzlW1kss0yHmo8yy68TN_FUhUzRz7Q).)
If the tab navigates or reloads,
[`DisallowJavascript`](https://cs.chromium.org/search/?q=WebUIMessageHandler::DisallowJavascript)
is called to clear the flag.

Therefore, before each callback from C++ to Javascript, the flag must be tested
by calling
[`IsJavascriptAllowed`](https://cs.chromium.org/search/?q=WebUIMessageHandler::IsJavascriptAllowed).
If false, then the callback must be dropped. (When the flag is false, calling
[`ResolveJavascriptCallback`](https://cs.chromium.org/search/?q=WebUIMessageHandler::ResolveJavascriptCallback)
will crash. See
[design doc](https://docs.google.com/document/d/1udXoW3aJL0-l5wrbsOg5bpYWB0qOCW5K7yXpv4tFeA8).)

Also beware of [ABA](https://en.wikipedia.org/wiki/ABA_problem) issues: Consider
the case where an asynchronous operation is started, the settings page is
reloaded, and the user triggers another operation using the original message
handler. The `javascript_allowed_` boolean will be true, but the original
callback should still be dropped because it relates to a operation that was
discarded by the reload. (Reloading settings UI does _not_ cause message handler
objects to be deleted.)

Thus a message handler may override
[`OnJavascriptDisallowed`](https://cs.chromium.org/search/?q=WebUIMessageHandler::OnJavascriptDisallowed)
to learn when pending callbacks should be canceled.

In the JS:

```js
window.onload = function() {
  app.initialize();
  chrome.send('startPilotLight');
};
```

In the C++:

```c++
void OvenHandler::HandleStartPilotLight(cont base::ListValue* /*args*/) {
  AllowJavascript();
  // CallJavascriptFunction() and FireWebUIListener() are now safe to do.
  GetOven()->StartPilotLight();
}
```

<div class="note">
Relying on the <code>'load'</code> event or browser-side navigation callbacks to
detect page readiness omits <i>application-specific</i> initialization, and a
custom <code>'initialized'</code> message is often necessary.
</div>

<a name="CallJavascriptFunction"></a>
### WebUIMessageHandler::CallJavascriptFunction()

When the browser process needs to tell the renderer/JS of an event or otherwise
execute code, it can use `CallJavascriptFunction()`.

<div class="note">
Javascript must be <a href="#AllowJavascript">allowed</a> to use
<code>CallJavscriptFunction()</code>.
</div>

```c++
void OvenHandler::OnPilotLightExtinguished() {
  CallJavascriptFunction("app.pilotLightExtinguished");
}
```

This works by crafting a string to be evaluated in the renderer. Any arguments
to the call are serialized to JSON and the parameter list is wrapped with

```
// See WebUI::GetJavascriptCall() for specifics:
"functionCallName(" + argumentsAsJson + ")"
```

and sent to the renderer via a `FrameMsg_JavaScriptExecuteRequest` IPC message.

While this works, it implies that:

* a global method must exist to successfully run the Javascript request
* any method can be called with any parameter (far more access than required in
  practice)

^ These factors have resulted in less use of `CallJavascriptFunction()` in the
webui codebase. This functionality can easily be accomplished with the following
alternatives:

* [`FireWebUIListener()`](#FireWebUIListener) allows easily notifying the page
  when an event occurs in C++ and is more loosely coupled (nothing blows up if
  the event dispatch is ignored). JS subscribes to notifications via
  [`cr.addWebUIListener`](#cr_addWebUIListener).
* [`ResolveJavascriptCallback`](#ResolveJavascriptCallback) and
  [`RejectJavascriptCallback`](#RejectJavascriptCallback) are useful
  when Javascript requires a response to an inquiry about C++-canonical state
  (i.e. "Is Autofill enabled?", "Is the user incognito?")

<a name="FireWebUIListener"></a>
### WebUIMessageHandler::FireWebUIListener()

`FireWebUIListener()` is used to notify a registered set of listeners that an
event has occurred. This is generally used for events that are not guaranteed to
happen in timely manner, or may be caused to happen by unpredictable events
(i.e. user actions).

Here's some example to detect a change to Chrome's theme:

```js
cr.addWebUIListener("theme-changed", refreshThemeStyles);
```

This Javascript event listener can be triggered in C++ via:

```c++
void MyHandler::OnThemeChanged() {
  FireWebUIListener("theme-changed");
}
```

Because it's not clear when a user might want to change their theme nor what
theme they'll choose, this is a good candidate for an event listener.

If you simply need to get a response in Javascript from C++, consider using
[`cr.sendWithPromise()`](#cr_sendWithPromise) and
[`ResolveJavascriptCallback`](#ResolveJavascriptCallback).

<a name="OnJavascriptAllowed"></a>
### WebUIMessageHandler::OnJavascriptAllowed()

`OnJavascriptDisallowed()` is a lifecycle method called in response to
[`AllowJavascript()`](#AllowJavascript). It is a good place to register
observers of global services or other callbacks that might call at unpredictable
times.

For example:

```c++
class MyHandler : public content::WebUIMessageHandler {
  MyHandler() {
    GetGlobalService()->AddObserver(this);  // <-- DON'T DO THIS.
  }
  void OnGlobalServiceEvent() {
    FireWebUIListener("global-thing-happened");
  }
};
```

Because browser-side C++ handlers are created before a renderer is ready, the
above code may result in calling [`FireWebUIListener`](#FireWebUIListener)
before the renderer is ready, which may result in dropped updates or
accidentally running Javascript in a renderer that has navigated to a new URL.

A safer way to set up communication is:

```c++
class MyHandler : public content::WebUIMessageHandler {
 public:
  MyHandler() : observer_(this) {}
  void OnJavascriptAllowed() override {
    observer_.Add(GetGlobalService());  // <-- DO THIS.
  }
  void OnJavascriptDisallowed() override {
    observer_.RemoveAll();  // <-- AND THIS.
  }
  ScopedObserver<MyHandler, GlobalService> observer_;  // <-- ALSO HANDY.
```
when a renderer has been created and the
document has loaded enough to signal to the C++ that it's ready to respond to
messages.

<a name="OnJavascriptDisallowed"></a>
### WebUIMessageHandler::OnJavascriptDisallowed()

`OnJavascriptDisallowed` is a lifecycle method called when it's unclear whether
it's safe to send JavaScript messsages to the renderer.

There's a number of situations that result in this method being called:

* renderer doesn't exist yet
* renderer exists but isn't ready
* renderer is ready but application-specific JS isn't ready yet
* tab refresh
* renderer crash

Though it's possible to programmatically disable Javascript, it's uncommon to
need to do so.

Because there's no single strategy that works for all cases of a renderer's
state (i.e. queueing vs dropping messages), these lifecycle methods were
introduced so a WebUI application can implement these decisions itself.

Often, it makes sense to disconnect from observers in
`OnJavascriptDisallowed()`:

```c++
void OvenHandler::OnJavascriptDisallowed() {
  scoped_oven_observer_.RemoveAll()
}
```

Because `OnJavascriptDisallowed()` is not guaranteed to be called before a
`WebUIMessageHandler`'s destructor, it is often advisable to use some form of
scoped observer that automatically unsubscribes on destruction but can also
imperatively unsubscribe in `OnJavascriptDisallowed()`.

<a name="RejectJavascriptCallback"></a>
### WebUIMessageHandler::RejectJavascriptCallback()

This method is called in response to
[`cr.sendWithPromise()`](#cr_sendWithPromise) to reject the issued Promise. This
runs the rejection (second) callback in the [Promise's
executor](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)
and any
[`catch()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/catch)
callbacks in the chain.

```c++
void OvenHandler::HandleBakeDonuts(const base::ListValue* args) {
  AllowJavascript();
  if (!GetOven()->HasGas()) {
    RejectJavascriptCallback(args->GetList()[0],
                             base::StringValue("need gas to cook the donuts!"));
  }
```

This method is basically just a
[`CallJavascriptFunction()`](#CallJavascriptFunction) wrapper that calls a
global "cr.webUIResponse" method with a success value of false.

```c++
// WebUIMessageHandler::RejectJavascriptCallback():
CallJavascriptFunction("cr.webUIResponse", callback_id, base::Value(false),
                       response);
```

See also: [`ResolveJavascriptCallback`](#ResolveJavascriptCallback)

<a name="ResolveJavascriptCallback"></a>
### WebUIMessageHandler::ResolveJavascriptCallback()

This method is called in response to
[`cr.sendWithPromise()`](#cr_sendWithPromise) to fulfill an issued Promise,
often with a value. This results in runnings any fulfillment (first) callbacks
in the associate Promise executor and any registered
[`then()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/then)
callbacks.

So, given this JS code:

```js
cr.sendWithPromise('bakeDonuts').then(function(numDonutsBaked) {
  shop.donuts += numDonutsBaked;
});
```

Some handling C++ might do this:

```c++
void OvenHandler::HandleBakeDonuts(const base::ListValue* args) {
  AllowJavascript();
  double num_donuts_baked = GetOven()->BakeDonuts();
  ResolveJavascriptCallback(args->GetList()[0], num_donuts_baked);
}
```

## Renderer (JS) &rarr; Browser (C++)

<a name="chrome_send"></a>
### chrome.send()

When the JavaScript `window` object is created, a renderer is checked for [WebUI
bindings](#bindings).

```c++
// RenderFrameImpl::DidClearWindowObject():
if (enabled_bindings_ & BINDINGS_POLICY_WEB_UI)
  WebUIExtension::Install(frame_);
```

If the bindings exist, a global `chrome.send()` function is exposed to the
renderer:

```c++
// WebUIExtension::Install():
v8::Local<v8::Object> chrome = GetOrCreateChromeObject(isolate, context);
chrome->Set(gin::StringToSymbol(isolate, "send"),
            gin::CreateFunctionTemplate(
                isolate, base::Bind(&WebUIExtension::Send))->GetFunction());
```

The `chrome.send()` method takes a message name and argument list.

```js
chrome.send('messageName', [arg1, arg2, ...]);
```

The message name and argument list are serialized to JSON and sent via the
`FrameHostMsg_WebUISend` IPC message from the renderer to the browser.

```c++
// In the renderer (WebUIExtension::Send()):
render_frame->Send(new FrameHostMsg_WebUISend(render_frame->GetRoutingID(),
                                              frame->GetDocument().Url(),
                                              message, *content));
```
```c++
// In the browser (WebUIImpl::OnMessageReceived()):
IPC_MESSAGE_HANDLER(FrameHostMsg_WebUISend, OnWebUISend)
```

The browser-side code does a map lookup for the message name and calls the found
callback with the deserialized arguments:

```c++
// WebUIImpl::ProcessWebUIMessage():
message_callbacks_.find(message)->second.Run(&args);
```

<a name="cr_addWebUIListener">
### cr.addWebUIListener()

WebUI listeners are a convenient way for C++ to inform JavaScript of events.

Older WebUI code exposed public methods for event notification, similar to how
responses to [chrome.send()](#chrome_send) used to work. They both
resulted in global namespace pollution, but it was additionally hard to stop
listening for events in some cases. **cr.addWebUIListener** is preferred in new
code.

Adding WebUI listeners creates and inserts a unique ID into a map in JavaScript,
just like [cr.sendWithPromise()](#cr_sendWithPromise).

```js
// addWebUIListener():
webUIListenerMap[eventName] = webUIListenerMap[eventName] || {};
webUIListenerMap[eventName][createUid()] = callback;
```

The C++ responds to a globally exposed function (`cr.webUIListenerCallback`)
with an event name and a variable number of arguments.

```c++
// WebUIMessageHandler:
template <typename... Values>
void FireWebUIListener(const std::string& event_name, const Values&... values) {
  CallJavascriptFunction("cr.webUIListenerCallback", base::Value(event_name),
                         values...);
}
```

C++ handlers call this `FireWebUIListener` method when an event occurs that
should be communicated to the JavaScript running in a tab.

```c++
void OvenHandler::OnBakingDonutsFinished(size_t num_donuts) {
  FireWebUIListener("donuts-baked", base::FundamentalValue(num_donuts));
}
```

JavaScript can listen for WebUI events via:

```js
var donutsReady = 0;
cr.addWebUIListener('donuts-baked', function(numFreshlyBakedDonuts) {
  donutsReady += numFreshlyBakedDonuts;
});
```

<a name="cr_sendWithPromise"></a>
### cr.sendWithPromise()

`cr.sendWithPromise()` is a wrapper around `chrome.send()`. It's used when
triggering a message requires a response:

```js
chrome.send('getNumberOfDonuts');  // No easy way to get response!
```

In older WebUI pages, global methods were exposed simply so responses could be
sent. **This is discouraged** as it pollutes the global namespace and is harder
to make request specific or do from deeply nested code.

In newer WebUI pages, you see code like this:

```js
cr.sendWithPromise('getNumberOfDonuts').then(function(numDonuts) {
  alert('Yay, there are ' + numDonuts + ' delicious donuts left!');
});
```

On the C++ side, the message registration is similar to
[`chrome.send()`](#chrome_send) except that the first argument in the
message handler's list is a callback ID. That ID is passed to
`ResolveJavascriptCallback()`, which ends up resolving the `Promise` in
JavaScript and calling the `then()` function.

```c++
void DonutHandler::HandleGetNumberOfDonuts(const base::ListValue* args) {
  AllowJavascript();

  const base::Value& callback_id = args->GetList()[0];
  size_t num_donuts = GetOven()->GetNumberOfDonuts();
  ResolveJavascriptCallback(callback_id, base::FundamentalValue(num_donuts));
}
```

Under the covers, a map of `Promise`s are kept in JavaScript.

The callback ID is just a namespaced, ever-increasing number. It's used to
insert a `Promise` into the JS-side map when created.

```js
// cr.sendWithPromise():
var id = methodName + '_' + uidCounter++;
chromeSendResolverMap[id] = new PromiseResolver;
chrome.send(methodName, [id].concat(args));
```

The corresponding number is used to look up a `Promise` and reject or resolve it
when the outcome is known.

```js
// cr.webUIResponse():
var resolver = chromeSendResolverMap[id];
if (success)
  resolver.resolve(response);
else
  resolver.reject(response);
```

This approach still relies on the C++ calling a globally exposed method, but
reduces the surface to only a single global (`cr.webUIResponse`) instead of
many. It also makes per-request responses easier, which is helpful when multiple
are in flight.


## Security considerations

Because WebUI pages are highly privileged, they are often targets for attack,
since taking control of a WebUI page can sometimes be sufficient to escape
Chrome's sandbox.  To make sure that the special powers granted to WebUI pages
are safe, WebUI pages are restricted in what they can do:

* WebUI pages cannot embed http/https resources or frames
* WebUI pages cannot issue http/https fetches

In the rare case that a WebUI page really needs to include web content, the safe
way to do this is by using a `<webview>` tag.  Using a `<webview>` tag is more
secure than using an iframe for multiple reasons, even if Site Isolation and
out-of-process iframes keep the web content out of the privileged WebUI process.

First, the content inside the `<webview>` tag has a much reduced attack surface,
since it does not have a window reference to its embedder or any other frames.
Only postMessage channel is supported, and this needs to be initiated by the
embedder, not the guest.

Second, the content inside the `<webview>` tag is hosted in a separate
StoragePartition. Thus, cookies and other persistent storage for both the WebUI
page and other browser tabs are inaccessible to it.

This greater level of isolation makes it safer to load possibly untrustworthy or
compromised web content, reducing the risk of sandbox escapes.

For an example of switching from iframe to webview tag see
https://crrev.com/c/710738.


## See also

* WebUI's C++ code follows the [Chromium C++ styleguide](../styleguide/c++/c++.md).
* WebUI's HTML/CSS/JS code follows the [Chromium Web
  Development Style Guide](../styleguide/web/web.md)


<script>
let nameEls = Array.from(document.querySelectorAll('[id], a[name]'));
let names = nameEls.map(nameEl => nameEl.name || nameEl.id);

let localLinks = Array.from(document.querySelectorAll('a[href^="#"]'));
let hrefs = localLinks.map(a => a.href.split('#')[1]);

hrefs.forEach(href => {
  if (names.includes(href))
    console.info('found: ' + href);
  else
    console.error('broken href: ' + href);
})
</script>
