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

# Creating WebUI Interfaces
[TOC]

A WebUI page is made of a single-page application, which communicates
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

`chrome/browser/resources/hello_world/app.css`
```css
/* Copyright 2024 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/* #css_wrapper_metadata_start
 * #type=style-lit
 * #scheme=relative
 * #css_wrapper_metadata_end */

#example-div {
  color: blue;
}
```

`chrome/browser/resources/hello_world/app.html.ts`
```js
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {HelloWorldAppElement} from './app.js';

export function getHtml(this: HelloWorldAppElement) {
  return html`
<h1>Hello World</h1>
<div id="example-div">${this.message_}</div>`;
}
```

`chrome/browser/resources/hello_world/app.ts`
```js
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export class HelloWorldAppElement extends CrLitElement {
  static get is() {
    return 'hello-world-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      message_: {type: String},
    };
  }

  protected message_: string = loadTimeData.getString('message');
}

declare global {
  interface HTMLElementTagNameMap {
    'hello-world-app': HelloWorldAppElement;
  }
}

customElements.define(HelloWorldAppElement.is, HelloWorldAppElement);
```

Add a `BUILD.gn` file to get TypeScript compilation and to generate the JS file
from which the template will be imported.

`chrome/browser/resources/hello_world/BUILD.gn`
```py
import("//ui/webui/resources/tools/build_webui.gni")

build_webui("build") {
  grd_prefix = "hello_world"

  static_files = [ "hello_world.html", "hello_world.css" ]

  non_web_component_files = [ "app.ts", "app.html.ts" ]
  css_files = [ "app.css" ]

  # Enable the proper webui_context_type depending on whether implementing
  # a chrome:// or chrome-untrusted:// page.
  webui_context_type = "trusted"

  ts_deps = [
    "//third_party/lit/v3_0:build_ts",
    "//ui/webui/resources/js:build_ts",
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
has a unique ID and preserves numerical ordering. If you see an error like "ValueError: Cannot jump to unvisited", please check the numeric order of your resource ids.

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
#include "chrome/browser/ui/webui/hello_world/hello_world_ui.h"

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
      IDR_HELLO_WORLD_HELLO_WORLD_HTML);

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

### Preferred method: Add a WebUIConfig class and put it in the WebUIConfigMap
`WebUIConfig`s contain minimal information about the host and scheme served
by the `WebUIController` subclass. It also can enable or disable the UI for
different conditions (e.g. feature flag status). You can create a
`WebUIConfig` subclass and register it in the `WebUIConfigMap` to ensure your
request handler is instantiated and used to handle any requests to the desired
scheme + host. If you don't need to pass any arguments to your controller
class, inherit from `DefaultWebUIConfig` to reduce the amount of code required:

`chrome/browser/ui/webui/hello_world/hello_world_ui.h`
```c++
// Forward declaration so that config definition can come before controller.
class HelloWorldUI;

class HelloWorldUIConfig : public content::DefaultWebUIConfig<HelloWorldUI> {
 public:
  HelloWorldUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIHelloWorldHost) {}
};
```

Register your config in `chrome_web_ui_configs.cc`, for trusted UIs, or
`chrome_untrusted_web_ui_configs.cc` for untrusted UIs.

`chrome/browser/ui/webui/chrome_web_ui_configs.cc`
```c++
+ #include "chrome/browser/ui/webui/hello_world/hello_world_ui.h"
...
+map.AddWebUIConfig(std::make_unique<hello_world::HelloWorldUIConfig>());
```

### Old method: Add your WebUI request handler to the Chrome WebUI factory

The Chrome WebUI factory is another way to setup your new request handler. This
is how many older WebUIs in Chrome are registered, since not all UIs have been
migrated to use the newer `WebUIConfig` (see
[migration bug](https://crbug.com/1317510)). Only use this method for a new UI
if the approach above using `WebUIConfig` does not work, and notify WebUI
`PLATFORM_OWNERS`.

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
  ui::mojom::ModalType GetDialogModalType() const override;
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

ui::mojom::ModalType HelloWorldDialog::GetDialogModalType() const {
  return ui::mojom::ModalType::kNone;
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
Any code that is located in `ui/webui/resources` and served from
`chrome://resources` and `chrome-untrusted://resources` can be used from any
WebUI page. If you want to share some additional code from another WebUI page
that is not in the shared resources, first see
[Sharing Code in WebUI](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/webui_code_sharing.md) to determine the best approach.

If you determine that the code should be narrowly shared, the following
explains how to add the narrowly shared resources to your WebUI data source.

In the snippet below:

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

Using `WebUIDataSource::AddResourcePaths()` we can add the resources from grit
files that are generated by limited sharing `build_webui()` targets as follows:

```cpp
#include "chrome/grit/foo_shared_resources.h"
#include "chrome/grit/bar_shared_resources.h"
// ...
HelloWorldUI::HelloWorldUI(content::WebUI* web_ui) {
  // ...
  // Add selected resources from foo_shared
  static constexpr webui::ResourcePath kResources[] = {
      {"foo_shared/foo_shared.css.js", IDR_FOO_SHARED_FOO_SHARED_CSS_JS},
      {"foo_shared/foo_shared_vars.css.js",
       IDR_FOO_SHARED_FOO_SHARED_VARS_CSS_JS},
  };
  source->AddResourcePaths(kResources);

  // Add all shared resources from bar_shared
  source->AddResourcePaths(
      base::make_span(kBarSharedResources, kBarSharedResourcesSize));
}
```
