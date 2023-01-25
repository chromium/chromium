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

# Creating WebUI Interfaces outside components/
This guide is based on
[Creating WebUI Interfaces in components](webui_in_components.md).

[TOC]

A WebUI page is made of a Polymer single-page application, which communicates
with a C++ UI controller, as explained [here](webui_explainer.md).

WebUI pages live in `chrome/browser/resources` and their native counterpart in
`chrome/browser/ui/webui/`. We will start by creating folders for the new page
in `chrome/browser/[resources|ui/webui]/hello_world`. When creating WebUI
resources, follow the
[Web Development Style Guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/web/web.md).

## Making a basic WebUI page

For a sample WebUI page you could start with the following files:

`chrome/browser/resources/hello_world/hello_world.html`
```html
<!DOCTYPE HTML>
<html>
  <meta charset="utf-8">
  <link rel="stylesheet" href="hello_world.css">
  <hello-world-app></hello-world-app>
  <script type="module" src="app.js"></script>
</html>
```

`chrome/browser/resources/hello_world/hello_world.css`
```css
body {
  margin: 0;
}
```

`chrome/browser/resources/hello_world/app.html`
```html
<h1>Hello World</h1>
<div id="example-div">[[message_]]</div>
```

`chrome/browser/resources/hello_world/app.ts`
```js
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './app.html.js';

export class HelloWorldAppElement extends PolymerElement {
  static get is() {
    return 'hello-world-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      message_: {
        type: String,
        value: () => loadTimeData.getString('message'),
      },
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'hello-world-app': HelloWorldAppElement;
  }
}

customElements.define(HelloWorldAppElement.is, HelloWorldAppElement);
```

Add a `tsconfig_base.json` file to configure TypeScript options. Typical
options needed by Polymer UIs include:
- disabling `noUncheckedIndexAccess`
- disabling `noUnusedLocals`: private members from Elements are accessed from
  their HTML template
- disabling `strictPropertyInitialization`: Element properties can be
  initialized via the `property` map.

`chrome/browser/resources/hello_world/tsconfig_base.json`
```json
{
  "extends": "../../../../tools/typescript/tsconfig_base.json",
  "compilerOptions": {
    "noUncheckedIndexedAccess": false,
    "noUnusedLocals": false,
    "strictPropertyInitialization": false
  }
}
```

Add a `BUILD.gn` file to get TypeScript compilation and to generate the JS file
from which the template will be imported.

`chrome/browser/resources/hello_world/BUILD.gn`
```py
import("//chrome/browser/resources/tools/build_webui.gni")

build_webui("build") {
  grd_prefix = "hello_world"

  static_files = [ "hello_world.html", "hello_world.css" ]

  web_component_files = [ "app.ts" ]

  non_web_component_files = [
    # For example the BrowserProxy file would go here.
  ]

  ts_deps = [
    "//third_party/polymer/v3_0:library",
    "//ui/webui/resources:library",
  ]
}
```

> Note: See [the build config docs for more examples](webui_build_configuration.md#example-build-configurations)
of how the build could be configured.

Finally, create an `OWNERS` file for the new folder.

### Adding the resources

The `build_webui` target in `BUILD.gn` autogenerates some targets and files
that need to be linked from the binary-wide resource targets:

Add the new resource target to `chrome/browser/resources/BUILD.gn`

```py
group("resources") {
  public_deps += [
    ...
    "hello_world:resources"
    ...
  ]
}
```

Add an entry to resource_ids.spec

This file is for automatically generating resource ids. Ensure that your entry
has a unique ID and preserves numerical ordering.

`tools/gritsettings/resource_ids.spec`

```
  # START chrome/ WebUI resources section
  ... (lots)
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/hello_world/resources.grd": {
    "META": {"sizes": {"includes": [5]}},
    "includes": [2085],
  },
```

Also add to `chrome/chrome_paks.gni`

```py
template("chrome_extra_paks") {
  ... (lots)
  sources += [
    ...
    "$root_gen_dir/chrome/hello_world_resources.pak",
    ...
  ]
  deps += [
    ...
    "//chrome/browser/resources/hello_world:resources",
    ...
  ]
}
```

### Adding URL constants for the new chrome URL

`chrome/common/webui_url_constants.cc:`
```c++
const char kChromeUIHelloWorldURL[] = "chrome://hello-world/";
const char kChromeUIHelloWorldHost[] = "hello-world";
```

`chrome/common/webui_url_constants.h:`
```c++
extern const char kChromeUIHelloWorldURL[];
extern const char kChromeUIHelloWorldHost[];
```

### Adding a WebUI class for handling requests to the `chrome://hello-world/` URL

Next we need a class to handle requests to this new resource URL. Typically this will subclass `WebUIController` (WebUI
dialogs will also need another class which will subclass `WebDialogDelegate`, this is shown later).

`chrome/browser/ui/webui/hello_world/hello_world_ui.h`
```c++
#ifndef CHROME_BROWSER_UI_WEBUI_HELLO_WORLD_HELLO_WORLD_H_
#define CHROME_BROWSER_UI_WEBUI_HELLO_WORLD_HELLO_WORLD_H_

#include "content/public/browser/web_ui_controller.h"

// The WebUI for chrome://hello-world
class HelloWorldUI : public content::WebUIController {
 public:
  explicit HelloWorldUI(content::WebUI* web_ui);
  ~HelloWorldUI() override;
};

#endif // CHROME_BROWSER_UI_WEBUI_HELLO_WORLD_HELLO_WORLD_H_
```

`chrome/browser/ui/webui/hello_world/hello_world_ui.cc`
```c++
#include "chrome/browser/ui/webui/hello_world_ui.h"

#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "chrome/grit/hello_world_resources.h"
#include "chrome/grit/hello_world_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"


HelloWorldUI::HelloWorldUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://hello-world source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIHelloWorldHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kHelloWorldResources, kHelloWorldResourcesSize),
      IDR_HELLO_WORLD_HELLO_WORLD_CONTAINER_HTML);

  // As a demonstration of passing a variable for JS to use we pass in some
  // a simple message.
  source->AddString("message", "Hello World!");
}

HelloWorldUI::~HelloWorldUI() = default;
```

To ensure that your code actually gets compiled, you need to add it to `chrome/browser/ui/BUILD.gn`:

```py
static_library("ui") {
  sources = [
    ... (lots)
    "webui/hello_world/hello_world_ui.cc",
    "webui/hello_world/hello_world_ui.h",
    ...
  ]
}
```

### Adding your WebUI request handler to the Chrome WebUI factory

The Chrome WebUI factory is where you setup your new request handler.

`chrome/browser/ui/webui/chrome_web_ui_controller_factory.cc:`
```c++
+ #include "chrome/browser/ui/webui/hello_world/hello_world_ui.h"
...
+ if (url.host() == chrome::kChromeUIHelloWorldHost)
+   return &NewWebUI<HelloWorldUI>;
```

### Check everything works

You're done! Assuming no errors (because everyone gets their code perfect the first time) you should be able to compile
and run chrome and navigate to `chrome://hello-world/` and see your nifty welcome text!


## Making a WebUI Dialog

Instead of having a full page for your WebUI, you might want a dialog in order to have a fully independent window.  To
do that, some small changes are needed to your code.  First, we need to add a new class which inherits from
`ui::WebDialogDelegate`.  The easiest way to do that is to edit the `hello_world_ui.*` files


`chrome/browser/ui/webui/hello_world/hello_world_ui.h`
```c++
 // Leave the old content, but add this new code
 class HelloWorldDialog : public ui::WebDialogDelegate {
 public:
  static void Show();
  ~HelloWorldDialog() override;
  HelloWorldDialog(const HelloWorldDialog&) = delete;
  HelloWorldDialog& operator=(const HelloWorldDialog&) = delete;

 private:
  HelloWorldDialog();
  // ui::WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override;
  std::u16string GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;

  content::WebUI* webui_ = nullptr;
};
```

`chrome/browser/ui/webui/hello_world/hello_world_ui.cc`
```c++
 // Leave the old content, but add this new stuff

HelloWorldDialog::HelloWorldDialog() = default;

void HelloWorldDialog::Show() {
  // HelloWorldDialog is self-deleting via OnDialogClosed().
  chrome::ShowWebDialog(nullptr, ProfileManager::GetActiveUserProfile(),
                        new HelloWorldDialog());
}

ui::ModalType HelloWorldDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_NONE;
}

std::u16string HelloWorldDialog::GetDialogTitle() const {
  return u"Hello world";
}

GURL HelloWorldDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIHelloWorldURL[);
}

void HelloWorldDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void HelloWorldDialog::GetDialogSize(gfx::Size* size) const {
  const int kDefaultWidth = 544;
  const int kDefaultHeight = 628;
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string HelloWorldDialog::GetDialogArgs() const {
  return "";
}

void HelloWorldDialog::OnDialogShown(content::WebUI* webui) {
  webui_ = webui;
}

void HelloWorldDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void HelloWorldDialog::OnCloseContents(content::WebContents* source,
                                        bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool HelloWorldDialog::ShouldShowDialogTitle() const {
  return true;
}

HelloWorldDialog::~HelloWorldDialog() = default;
```

Finally, you will need to do something to actually show your dialog, which can be done by calling `HelloWorldDialog::Show()`.

## More elaborate configurations

### Referencing resources from another webui page

There are already mechanisms to make resources available chrome-wide, by
publishing them under `chrome://resources`. If this is not appropriate, there
are some ways to serve a file from some other webui page directly through
another host.

First, a few explanations. The configuration based on the `build_webui()` BUILD
target as presented above generates a few helpers that hide the complexity of
the page's configuration. For example, considering the snippet below:

```cpp
//...
#include "chrome/grit/hello_world_resources.h"
#include "chrome/grit/hello_world_resources_map.h"

HelloWorldUI::HelloWorldUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // ...
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kHelloWorldResources, kHelloWorldResourcesSize),
      IDR_HELLO_WORLD_HELLO_WORLD_CONTAINER_HTML);
}
```

`kHelloWorldResources` and `kHelloWorldResourcesSize` come from from the
imported grit-generated files, as configured by the build target, and reference
the files listed in it so they can be served out of the given host name.
For example, they would contain values like:

```cpp
const webui::ResourcePath kHelloWorldResources[] = {
  {"hello_world.html", IDR_CHROME_BROWSER_RESOURCES_HELLO_WORLD_HELLO_WORLD_HTML},
  {"hello_world.css", IDR_CHROME_BROWSER_RESOURCES_HELLO_WORLD_HELLO_WORLD_CSS},
};
```

Using `WebUIDataSource::AddResourcePaths()` we can add other resources,
looking for the right way to declare them by looking through the generated
grit files (e.g. via codesearch), or manual registrations if they exist.

```cpp
#include "chrome/grit/signin_resources.h"
// ...
HelloWorldUI::HelloWorldUI(content::WebUI* web_ui) {
  // ...
  static constexpr webui::ResourcePath kResources[] = {
      {"signin_shared.css.js", IDR_SIGNIN_SIGNIN_SHARED_CSS_JS},
      {"signin_vars.css.js", IDR_SIGNIN_SIGNIN_VARS_CSS_JS},
  };
  source->AddResourcePaths(kResources);
}
```
