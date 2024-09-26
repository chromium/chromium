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
browser process lifecycle. Before the first `WebUIConfig` is registered, the
`WebUIConfigMap` instance is created. This map creates and registers a
factory (`WebUIConfigMapWebUIControllerFactory`) in its constructor.
This factory looks at the global `WebUIConfigMap`, which maps hosts to
`WebUIConfig`s, to see if any of the configs handle the requested URL. It calls
the method on the config to create the corresponding controller if it finds a
config to handle the URL.

```c++
// ChromeBrowserMainParts::PreMainMessageLoopRunImpl():

// Legacy WebUIControllerFactory registration
content::WebUIControllerFactory::RegisterFactory(
   ChromeWebUIControllerFactory::GetInstance());

// Factory for all WebUIs using WebUIConfig will be created here.
RegisterChromeWebUIConfigs();
RegisterChromeUntrustedWebUIConfigs();
```

When a URL is requested, a new renderer is created to load the URL, and a
corresponding class in the browser is set up to handle messages from the
renderer to the browser (a `RenderFrameHost`).

```c++
auto* config = config_map_->GetConfig(browser_context, url);
if (!config)
  return nullptr;  // Not a known host; no special access.

return config->CreateWebUIController(web_ui, url);
```

Configs can be registered with the map by calling `map.AddWebUIConfig()` in
`chrome_web_ui_configs.cc`:
```c++
map.AddWebUIConfig(std::make_unique<donuts::DonutsUIConfig>());

```

If a factory knows how to handle a host (returns a `WebUIFactoryFunction`),
the navigation machinery [grants the renderer process WebUI
bindings](#bindings) via the child security policy.

```c++
// RenderFrameHostImpl::AllowBindings():
if (bindings_flags.Has(BindingsPolicyValue::kWebUi)) {
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantWebUIBindings(
      GetProcess()->GetID());
}
```

The factory creates a [`WebUIController`](#WebUIController) for a tab using
the WebUIConfig.

Here's an example using the DefaultWebUIConfig:

```c++
class DonutsUI;

// This would go in chrome/common/webui_url_constants.cc
namespace chrome {
const char kChromeUIDonutsHost[] = "donuts";
}  // namespace chrome

// Config for chrome://donuts
class DonutsUIConfig : public content::DefaultWebUIConfig<DonutsUI> {
 public:
  DonutsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIDonutsHost) {}
};

// Controller for chrome://donuts.
class DonutsUI : public content::WebUIController {
 public:
  DonutsUI(content::WebUI* web_ui) : content::WebUIController(web_ui) {
    content::WebUIDataSource* source =
        content::WebUIDataSource::CreateAndAdd(
            web_ui->GetWebContents()->GetBrowserContext(),
            "donuts");  // "donuts" == hostname
    source->AddString("mmmDonuts", "Mmm, donuts!");  // Translations.
    source->AddResourcePath("", IDR_DONUTS_HTML);  // Home page.

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

### WebUIConfig
A `WebUIConfig` contains minimal possible logic and information for determining
whether a certain subclass of `WebUIController` should be created for a given
URL.

A `WebUIConfig` holds information about the host and scheme (`chrome://` or
[`chrome-untrusted://`](chrome_untrusted.md)) that the controller serves.

A `WebUIConfig` may contain logic to check if the WebUI is enabled for a given
`BrowserContext` and url (e.g., if relevant feature flags are enabled/disabled,
if the url path is valid, etc).

A `WebUIConfig` can invoke the `WebUIController`'s constructor in its
`CreateWebUIControllerForURL` method.

`WebUIConfig`s are created at startup when factories are registered, so should
be lightweight.

### WebUIController

A `WebUIController` is the brains of the operation, and is responsible for
application-specific logic, setting up translations and resources, creating
message handlers, and potentially responding to requests dynamically. In complex
pages, logic is often split across multiple
[`WebUIMessageHandler`s](#WebUIMessageHandler) instead of solely in the
controller for organizational benefits.

A `WebUIController` is owned by a [`WebUI`](#WebUI), and is created and set on
an existing [`WebUI`](#WebUI) when the corresponding `WebUIConfig` is found in
the map matching the URL, or when the correct controller is determined via URL
inspection in `ChromeWebUIControllerFactory`. (i.e. chrome://settings creates
a generic [`WebUI`](#WebUI) with a settings-specific `WebUIController`).

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
content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
    Profile::FromWebUI(web_ui), "history");

source->AddResourcePath("sign_in_promo.svg", IDR_HISTORY_SIGN_IN_PROMO_SVG);
source->AddResourcePath("synced_tabs.html", IDR_HISTORY_SYNCED_TABS_HTML);

source->AddString("title", IDS_HISTORY_TITLE);
source->AddString("moreFromThisSite", IDS_HISTORY_MORE_FROM_THIS_SITE);

source->AddBoolean("showDateRanges",
    base::FeatureList::IsEnabled(features::kHistoryShowDateRanges));

webui::SetupWebUIDataSource(
    source, base::make_span(kHistoryResources, kHistoryResourcesSize),
    kGeneratedPath, IDR_HISTORY_HISTORY_HTML);
```

For more about each of the methods called on `WebUIDataSource` and the utility
method that performs additional configuration, see [DataSources](#DataSources)
and [WebUIDataSourceUtils](#WebUIDataSourceUtils)

### WebUIMessageHandler

Because some pages have many messages or share code that sends messages, message
handling is often split into discrete classes called `WebUIMessageHandler`s.
These handlers respond to specific invocations from JavaScript.

So, the given C++ code:

```c++
void OvenHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "bakeDonuts",
      base::BindRepeating(&OvenHandler::HandleBakeDonuts,
                          base::Unretained(this)));
}

void OvenHandler::HandleBakeDonuts(const base::Value::List& args) {
  AllowJavascript();

  // IMPORTANT: Fully validate `args`.
  CHECK_EQ(1u, args.size());
  int num_donuts = args[0].GetInt();
  CHECK_GT(num_donuts, 0);
  GetOven()->BakeDonuts(num_donuts);
}
```

Can be triggered in JavaScript with this example code:

```js
$('bakeDonutsButton').onclick = function() {
  chrome.send('bakeDonuts', [5]);  // bake 5 donuts!
};
```

## Data Sources

### WebUIDataSource::CreateAndAdd()

This is a factory method required to create and add a WebUIDataSource. The first
argument to `Create()` is the browser context. The second argument is typically
the host name of the page. The caller does not own the result.

Additionally, calling `CreateAndAdd()` will overwrite any existing data source
with the same name.

<div class="note">
It's unsafe to keep references to a <code>WebUIDataSource</code> after calling
<code>Add()</code>. Don't do this.
</div>

### WebUIDataSource::AddLocalizedString()

Using an int reference to a grit string (starts with "IDS" and lives in a .grd
or .grdp file), adding a string with a key name will be possible to reference
via the `$i18n{}` syntax (and will be replaced when requested) or later
dynamically in JavaScript via `loadTimeData.getString()` (or `getStringF`).

### WebUIDataSource::AddLocalizedStrings()

Many Web UI data sources need to be set up with a large number of localized
strings. Instead of repeatedly calling <code>AddLocalizedString()</code>, create
an array of all the strings and use <code>AddLocalizedStrings()</code>:

```c++
  static constexpr webui::LocalizedString kStrings[] = {
      // Localized strings (alphabetical order).
      {"actionMenuDescription", IDS_HISTORY_ACTION_MENU_DESCRIPTION},
      {"ariaRoleDescription", IDS_HISTORY_ARIA_ROLE_DESCRIPTION},
      {"bookmarked", IDS_HISTORY_ENTRY_BOOKMARKED},
  };
  source->AddLocalizedStrings(kStrings);
```

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

### WebUIDataSource::AddResourcePaths()

Similar to the localized strings, many Web UIs need to add a large number of
resource paths. In this case, use <code>AddResourcePaths()</code> to
replace repeated calls to <code>AddResourcePath()</code>.

```c++
  static constexpr webui::ResourcePath kResources[] = {
      {"browser_api.js", IDR_BROWSER_API_JS},
      {"constants.js", IDR_CONSTANTS_JS},
      {"controller.js", IDR_CONTROLLER_JS},
  };
  source->AddResourcePaths(kResources);
```

The same method can be leveraged for cases that directly use constants defined
by autogenerated grit resources map header files. For example, the autogenerated
print\_preview\_resources\_map.h header defines a
<code>webui::ResourcePath</code> array named <code>kPrintPreviewResources</code>
and a <code>size\_t kPrintPreviewResourcesSize</code>. All the resources in this
resource map can be added as follows:

```c++
  source->AddResourcePaths(
      base::make_span(kPrintPreviewResources, kPrintPreviewResourcesSize));
```

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
be preferable to use <code>sendWithPromise()</code> to initialize dynamic
values and call <code>FireWebUIListener()</code> to update them.

If you really want or need to use <code>AddBoolean()</code> for a dynamic value,
make sure to call <code>WebUIDataSource::Update()</code> when the value changes.
</div>

## WebUI utils for working with data sources

chrome/browser/ui/webui/webui\_util.\* contains a number of methods to simplify
common configuration tasks.

### webui::SetupWebUIDataSource()

This method performs common configuration tasks on a data source for a Web UI
that uses JS modules. When creating a Web UI that uses JS modules, use this
utility instead of duplicating the configuration steps it performs elsewhere.
Specific setup steps include:

* Setting the content security policy to allow the data source to load only
  resources from its own host (e.g. chrome://history), chrome://resources, and
  chrome://webui-test (used to serve test files).
* Enabling i18n template replacements by calling <code>UseStringsJs()</code> and
  <code>EnableReplaceI18nInJS()</code> on the data source.
* Adding the test loader files to the data source, so that test files can be
  loaded as JS modules.
* Setting the resource to load for the empty path.
* Adding all resources from a GritResourceMap.

## Browser (C++) and Renderer (JS) communication

### Mojo

[Mojo](https://chromium.googlesource.com/chromium/src/+/master/mojo/README.md)
is used for IPC throughout Chromium, and should generally be used for new
WebUIs to communicate between the browser (C++) and the renderer (JS/TS). To
use Mojo, you will need to:

* Write an interface definition for the JS/C++ interface in a mojom file
* Add a build target in the BUILD.gn file to autogenerate C++ and TypeScript
  code ("bindings").
* Bind the interface on the C++ side and implement any methods to send or
  receive information from TypeScript.
* Add the TypeScript bindings file to your WebUI's <code>ts_library()</code>
  and use them in your TypeScript code.

#### Mojo Interface Definition
Mojo interfaces are declared in mojom files. For WebUIs, these normally live
alongside the C++ code in chrome/browser/ui/webui. For example:

**chrome/browser/ui/webui/donuts/donuts.mojom**
```
module donuts.mojom;

// Factory ensures that the Page and PageHandler interfaces are always created
// together without requiring an initialization call from the WebUI to the
// handler.
interface PageHandlerFactory {
  CreatePageHandler(pending_remote<Page> page,
                    pending_receiver<PageHandler> handler);
};

// Called from TS side of chrome://donuts (Renderer -> Browser)
interface PageHandler {
  StartPilotLight();

  BakeDonuts(uint32 num_donuts);

  // Expects a response from the browser.
  GetNumberOfDonuts() => (uint32 num_donuts);
};

// Called from C++ side of chrome://donuts. (Browser -> Renderer)
interface Page {
  DonutsBaked(uint32 num_donuts);
};
```

#### BUILD.gn mojo target
mojom() is the build rule used to generate mojo bindings. It can be set up as
follows:

**chrome/browser/ui/webui/donuts/BUILD.gn**
```
import("//mojo/public/tools/bindings/mojom.gni")

mojom("mojo_bindings") {
  sources = [ "donuts.mojom" ]
  webui_module_path = "/"
}
```

#### Setting up C++ bindings
The WebUIController class should inherit from ui::MojoWebUIController and
from the PageHandlerFactory class defined in the mojom file.

**chrome/browser/ui/webui/donuts/donuts_ui.h**
```c++
class DonutsPageHandler;

class DonutsUI : public ui::MojoWebUIController,
                 public donuts::mojom::PageHandlerFactory {
 public:
  explicit DonutsUI(content::WebUI* web_ui);

  DonutsUI(const DonutsUI&) = delete;
  DonutsUI& operator=(const DonutsUI&) = delete;

  ~DonutsUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<donuts::mojom::PageHandlerFactory> receiver);

 private:
  // donuts::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<donuts::mojom::Page> page,
      mojo::PendingReceiver<donuts::mojom::PageHandler> receiver) override;

  std::unique_ptr<DonutsPageHandler> page_handler_;

  mojo::Receiver<donuts::mojom::PageHandlerFactory> page_factory_receiver_{
      this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};
```

**chrome/browser/ui/webui/donuts/donuts_ui.cc**
```c++
DonutsUI::DonutsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
  // Normal constructor steps (e.g. setting up data source) go here.
}

WEB_UI_CONTROLLER_TYPE_IMPL(DonutsUI)

DonutsUI::~DonutsUI() = default;

void DonutsUI::BindInterface(
    mojo::PendingReceiver<donuts::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void DonutsUI::CreatePageHandler(
    mojo::PendingRemote<donuts::mojom::Page> page,
    mojo::PendingReceiver<donuts::mojom::PageHandler> receiver) {
  DCHECK(page);
  page_handler_ = std::make_unique<DonutsPageHandler>(
      std::move(receiver), std::move(page));
}
```

You also need to register the PageHandlerFactory to your controller in
**chrome/browser/chrome_browser_interface_binders.cc**:
```c++
RegisterWebUIControllerInterfaceBinder<donuts::mojom::PageHandlerFactory,
                                       DonutsUI>(map);
```

#### Using C++ bindings for communication
The WebUI message handler should inherit from the Mojo PageHandler class.

**chrome/browser/ui/webui/donuts/donuts_page_handler.h**
```c++
#include "chrome/browser/ui/webui/donuts/donuts.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class DonutsPageHandler : public donuts::mojom::PageHandler {
 public:
  DonutsPageHandler(
      mojo::PendingReceiver<donuts::mojom::PageHandler> receiver,
      mojo::PendingRemote<donuts::mojom::Page> page);

  DonutsPageHandler(const DonutsPageHandler&) = delete;
  DonutsPageHandler& operator=(const DonutsPageHandler&) = delete;

  ~DonutsPageHandler() override;

  // Triggered by some outside event
  void OnBakingDonutsFinished(uint32_t num_donuts);

  // donuts::mojom::PageHandler:
  void StartPilotLight() override;
  void BakeDonuts(uint32_t num_donuts) override;
  void GetNumberOfDonuts(GetNumberOfDonutsCallback callback) override;

 private:
  mojo::Receiver<donuts::mojom::PageHandler> receiver_;
  mojo::Remote<donuts::mojom::Page> page_;
};
```

The message handler needs to implement all the methods on the PageHandler
interface.

**chrome/browser/ui/webui/donuts/donuts_page_handler.cc**
```c++
DonutsPageHandler::DonutsPageHandler(
    mojo::PendingReceiver<donuts::mojom::PageHandler> receiver,
    mojo::PendingRemote<donuts::mojom::Page> page)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
}

DonutsPageHandler::~DonutsPageHandler() {
  GetOven()->TurnOffGas();
}

// Triggered by outside asynchronous event; sends information to the renderer.
void DonutsPageHandler::OnBakingDonutsFinished(uint32_t num_donuts) {
  page_->DonutsBaked(num_donuts);
}

// Triggered by startPilotLight() call in TS.
void DonutsPageHandler::StartPilotLight() {
  GetOven()->StartPilotLight();
}

// Triggered by bakeDonuts() call in TS.
void DonutsPageHandler::BakeDonuts(uint32_t num_donuts) {
  GetOven()->BakeDonuts();
}

// Triggered by getNumberOfDonuts() call in TS; sends a response back to the
// renderer.
void DonutsPageHandler::GetNumberOfDonuts(GetNumberOfDonutsCallback callback) {
  uint32_t result = GetOven()->GetNumberOfDonuts();
  std::move(callback).Run(result);
}
```

#### Setting Up TypeScript bindings

For WebUIs using the `build_webui()` rule, the TypeScript mojo bindings can be
added to the build and served from the root (e.g.
`chrome://donuts/donuts.mojom-webui.js`) by adding the following arguments to
`build_webui()`:

**chrome/browser/resources/donuts/BUILD.gn**
```
import("//ui/webui/resources/tools/build_webui.gni")

build_webui("build") {
  grd_prefix = "donuts"

  # You will add these files in the next step:
  non_web_component_files = [
    "donuts.ts",
    "browser_proxy.ts",
  ]

  ts_deps = [ "//ui/webui/resources/mojo:build_ts" ]

  mojo_files_deps =
      [ "//chrome/browser/ui/webui/donuts:mojo_bindings_ts__generator" ]
  mojo_files = [
    "$root_gen_dir/chrome/browser/ui/webui/donuts/donuts.mojom-webui.ts",
  ]
}
```

It is often helpful to wrap the TypeScript side of Mojo setup in a BrowserProxy
class:

**chrome/browser/resources/donuts/browser_proxy.ts**
```js
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './donuts.mojom-webui.js';
import type {PageHandlerInterface} from './donuts.mojom-webui.js';

// Exporting the interface helps when creating a TestBrowserProxy wrapper.
export interface BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;
}

export class BrowserProxyImpl implements BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  private constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();
    PageHandlerFactory.getRemote().createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }

  static setInstance(proxy: BrowserProxy) {
    instance = proxy;
  }
}

let instance: BrowserProxy|null = null;
```

#### Using TypeScript bindings for communication
The `callbackRouter` (`PageCallbackRouter`) can be used to add listeners for
asynchronous events sent from the browser.

The `handler` (`PageHandlerRemote`) can be used to send messages from the
renderer to the browser. For interface methods that require a browser response,
calling the method returns a promise. The promise will be resolved with the
response from the browser.

**chrome/browser/resources/donuts/donuts.ts**
```js
import {BrowserProxyImpl} from './browser_proxy.js';

let numDonutsBaked: number = 0;

window.onload = function() {
  // Other page initialization steps go here
  const proxy = BrowserProxyImpl.getInstance();
  // Tells the browser to start the pilot light.
  proxy.handler.startPilotLight();
  // Adds a listener for the asynchronous "donutsBaked" event.
  proxy.callbackRouter.donutsBaked.addListener(
    (numDonuts: number) => {
      numDonutsBaked += numDonuts;
    });
};

function CheckNumberOfDonuts() {
  // Requests the number of donuts from the browser, and alerts with the
  // response.
  BrowserProxyImpl.getInstance().handler.getNumberOfDonuts().then(
      (numDonuts: number) => {
        alert('Yay, there are ' + numDonuts + ' delicious donuts left!');
      });
}

function BakeDonuts(numDonuts: number) {
  // Tells the browser to bake |numDonuts| donuts.
  BrowserProxyImpl.getInstance().handler.bakeDonuts(numDonuts);
}
```

### Pre-Mojo alternative: chrome.send()/WebUIMessageHandler
Most Chrome WebUIs were added before the introduction of Mojo, and use the
older style WebUIMessageHandler + chrome.send() pattern. The following sections
detail the methods in WebUIMessageHandler and the corresponding communication
methods in TypeScript/JavaScript and how to use them.

#### WebUIMessageHandler::AllowJavascript()

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
void OvenHandler::HandleStartPilotLight(const base::Value::List& /*args*/) {
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

#### WebUIMessageHandler::CallJavascriptFunction()

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
  [`addWebUiListener`](#addWebUiListener).
* [`ResolveJavascriptCallback`](#ResolveJavascriptCallback) and
  [`RejectJavascriptCallback`](#RejectJavascriptCallback) are useful
  when Javascript requires a response to an inquiry about C++-canonical state
  (i.e. "Is Autofill enabled?", "Is the user incognito?")

#### WebUIMessageHandler::FireWebUIListener()

`FireWebUIListener()` is used to notify a registered set of listeners that an
event has occurred. This is generally used for events that are not guaranteed to
happen in timely manner, or may be caused to happen by unpredictable events
(i.e. user actions).

Here's some example to detect a change to Chrome's theme:

```js
addWebUiListener("theme-changed", refreshThemeStyles);
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
[`sendWithPromise()`](#sendWithPromise) and
[`ResolveJavascriptCallback`](#ResolveJavascriptCallback).

#### WebUIMessageHandler::OnJavascriptAllowed()

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
  void OnJavascriptAllowed() override {
    observation_.Observe(GetGlobalService());  // <-- DO THIS.
  }
  void OnJavascriptDisallowed() override {
    observation_.Reset();  // <-- AND THIS.
  }
  base::ScopedObservation<MyHandler, GlobalService> observation_{this};  // <-- ALSO HANDY.
```
when a renderer has been created and the
document has loaded enough to signal to the C++ that it's ready to respond to
messages.

#### WebUIMessageHandler::OnJavascriptDisallowed()

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
  scoped_oven_observation_.Reset()
}
```

Because `OnJavascriptDisallowed()` is not guaranteed to be called before a
`WebUIMessageHandler`'s destructor, it is often advisable to use some form of
scoped observer that automatically unsubscribes on destruction but can also
imperatively unsubscribe in `OnJavascriptDisallowed()`.

#### WebUIMessageHandler::RejectJavascriptCallback()

This method is called in response to
[`sendWithPromise()`](#sendWithPromise) to reject the issued Promise. This
runs the rejection (second) callback in the [Promise's
executor](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)
and any
[`catch()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/catch)
callbacks in the chain.

```c++
void OvenHandler::HandleBakeDonuts(const base::Value::List& args) {
  AllowJavascript();
  if (!GetOven()->HasGas()) {
    RejectJavascriptCallback(args[0],
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

#### WebUIMessageHandler::ResolveJavascriptCallback()

This method is called in response to
[`sendWithPromise()`](#sendWithPromise) to fulfill an issued Promise,
often with a value. This results in runnings any fulfillment (first) callbacks
in the associate Promise executor and any registered
[`then()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/then)
callbacks.

So, given this TypeScript code:

```js
sendWithPromise('bakeDonuts', [5]).then(function(numDonutsBaked: number) {
  shop.donuts += numDonutsBaked;
});
```

Some handling C++ might do this:

```c++
void OvenHandler::HandleBakeDonuts(const base::Value::List& args) {
  AllowJavascript();
  double num_donuts_baked = GetOven()->BakeDonuts();
  ResolveJavascriptCallback(args[0], base::Value(num_donuts_baked));
}
```

#### chrome.send()

When the JavaScript `window` object is created, a renderer is checked for [WebUI
bindings](#bindings).

```c++
// RenderFrameImpl::DidClearWindowObject():
if (enabled_bindings_.Has(BindingsPolicyValue::kWebUi))
  WebUIExtension::Install(frame_);
```

If the bindings exist, a global `chrome.send()` function is exposed to the
renderer:

```c++
// WebUIExtension::Install():
v8::Local<v8::Object> chrome = GetOrCreateChromeObject(isolate, context);
chrome->Set(gin::StringToSymbol(isolate, "send"),
            gin::CreateFunctionTemplate(
                isolate,
                base::BindRepeating(&WebUIExtension::Send))->GetFunction());
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

#### addWebUiListener()

WebUI listeners are a convenient way for C++ to inform JavaScript of events.

Older WebUI code exposed public methods for event notification, similar to how
responses to [chrome.send()](#chrome_send) used to work. They both
resulted in global namespace pollution, but it was additionally hard to stop
listening for events in some cases. **addWebUiListener** is preferred in new
code.

Adding WebUI listeners creates and inserts a unique ID into a map in JavaScript,
just like [sendWithPromise()](#sendWithPromise).

addWebUiListener can be imported from 'chrome://resources/js/cr.js'.

```js
// addWebUiListener():
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
  FireWebUIListener("donuts-baked", base::Value(num_donuts));
}
```

TypeScript can listen for WebUI events via:

```js
let donutsReady: number = 0;
addWebUiListener('donuts-baked', function(numFreshlyBakedDonuts: number) {
  donutsReady += numFreshlyBakedDonuts;
});
```

#### sendWithPromise()

`sendWithPromise()` is a wrapper around `chrome.send()`. It's used when
triggering a message requires a response:

```js
chrome.send('getNumberOfDonuts');  // No easy way to get response!
```

In older WebUI pages, global methods were exposed simply so responses could be
sent. **This is discouraged** as it pollutes the global namespace and is harder
to make request specific or do from deeply nested code.

In newer WebUI pages, you see code like this:

```js
sendWithPromise('getNumberOfDonuts').then(function(numDonuts: number) {
  alert('Yay, there are ' + numDonuts + ' delicious donuts left!');
});
```

Note that sendWithPromise can be imported from 'chrome://resources/js/cr.js';

On the C++ side, the message registration is similar to
[`chrome.send()`](#chrome_send) except that the first argument in the
message handler's list is a callback ID. That ID is passed to
`ResolveJavascriptCallback()`, which ends up resolving the `Promise` in
JavaScript/TypeScript and calling the `then()` function.

```c++
void DonutHandler::HandleGetNumberOfDonuts(const base::Value::List& args) {
  AllowJavascript();

  const base::Value& callback_id = args[0];
  size_t num_donuts = GetOven()->GetNumberOfDonuts();
  ResolveJavascriptCallback(callback_id, base::Value(num_donuts));
}
```

Under the covers, a map of `Promise`s are kept in JavaScript.

The callback ID is just a namespaced, ever-increasing number. It's used to
insert a `Promise` into the JS-side map when created.

```js
// sendWithPromise():
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

* WebUI pages cannot embed http/https resources
* WebUI pages cannot issue http/https fetches

In the rare case that a WebUI page really needs to include web content, the safe
way to do this is by using an `<iframe>` tag. Chrome's security model gives
process isolation between the WebUI and the web content. However, some extra
precautions need to be taken, because there are properties of the page that are
accessible cross-origin and malicious code can take advantage of such data to
attack the WebUI. Here are some things to keep in mind:

* The WebUI page can receive postMessage payloads from the web and should
  ensure it verifies any messages as they are not trustworthy.
* The entire frame tree is visible to the embedded web content, including
  ancestor origins.
* The web content runs in the same StoragePartition and Profile as the WebUI,
  which reflect where the WebUI page was loaded (e.g., the default profile,
  Incognito, etc). The corresponding user credentials will thus be available to
  the web content inside the WebUI, possibly showing the user as signed in.

Note: WebUIs have a default Content Security Policy which disallows embedding
any frames. If you want to include any web content in an <iframe> you will need
to update the policy for your WebUI. When doing so, allow only known origins and
avoid making the policy more permissive than strictly necessary.

Alternatively, a `<webview>` tag can be used, which runs in a separate
StoragePartition, a separate frame tree, and restricts postMessage communication
by default. Note that `<webview>` is only available on desktop platforms.

## JavaScript Error Reporting

By default, errors in the JavaScript or TypeScript of a WebUI page will generate
error reports which appear in Google's internal [go/crash](http://go/crash)
reports page. These error reports will only be generated for Google Chrome
builds, not Chromium or other Chromium-based browsers.

Specifically, an error report will be generated when the JavaScript or
TypeScript for a WebUI-based chrome:// page does one of the following:
* Generates an uncaught exception,
* Has a promise which is rejected, and no rejection handler is provided, or
* Calls `console.error()`.

Such errors will appear alongside other crashes in the
`product_name=Chrome_ChromeOS` or `product_name=Chrome_Linux` lists on
[go/crash](http://go/crash).

The signature of the error is the error message followed by the URL on which the
error appeared. For example, if chrome://settings/lazy_load.js throws a
TypeError with a message `Cannot read properties of null (reading 'select')` and
does not catch it, the magic signature would be
```
Uncaught TypeError: Cannot read properties of null (reading 'select') (chrome://settings)
```
To avoid spamming the system, only one error report with a given message will be
generated per hour.

If you are getting error reports for an expected condition, you can turn off the
reports simply by changing `console.error()` into `console.warn()`. For
instance, if JavaScript is calling `console.error()` when the user tries to
connect to an unavailable WiFi network at the same time the page shows the user
an error message, the `console.error()` should be replaced with a
`console.warn()`.

If you wish to get more control of the JavaScript error messages, for example
to change the product name or to add additional data, you may wish to switch to
using `CrashReportPrivate.reportError()`. If you do so, be sure to override
`WebUIController::IsJavascriptErrorReportingEnabled()` to return false for your
page; this will avoid generating redundant error reports.

### Are JavaScript errors actually crashes?
JavaScript errors are not "crashes" in the C++ sense. They do not stop a process
from running, they do not cause a "sad tab" page. Some tooling refers to them as
crashes because they are going through the same pipeline as the C++ crashes, and
that pipeline was originally designed to handle crashes.

### How much impact does this JavaScript error have?
That depends on the JavaScript error. In some cases, the errors have no user
impact; for instance, the "unavailable WiFi network calling `console.error()`"
example above. In other cases, JavaScript errors may be serious errors that
block the user from completing critical user journeys. For example, if the
JavaScript is supposed to un-hide one of several variants of settings page, but
the JavaScript has an unhandled exception before un-hiding any of them, then
the user will see a blank page and be unable to change that setting.

Because it is difficult to automatically determine the severity of a given
error, JavaScript errors are currently all classified as "WARNING" level when
computing stability metrics.

### Known issues
1. Error reporting is currently enabled only on ChromeOS and Linux.
2. Errors are only reported for chrome:// URLs.
3. Unhandled promise rejections do not have a good stack.
4. The line numbers and column numbers in the stacks are for the minified
   JavaScript and do not correspond to the line and column numbers of the
   original source files.
5. Error messages with variable strings do not group well. For example, if the
   error message includes the name of a network, each network name will be its
   own signature.

## See also

* WebUI's C++ code follows the [Chromium C++ styleguide](../styleguide/c++/c++.md).
* WebUI's HTML/CSS/JS code follows the [Chromium Web
  Development Style Guide](../styleguide/web/web.md)
* Adding tests for WebUI pages: [Testing WebUI](./testing_webui.md)
* Demo WebUI widgets at `chrome://webui-gallery` (and source at
  [chrome/browser/resources/webui_gallery/](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/webui_gallery/))


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
