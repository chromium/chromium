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

# Creating WebUI Interfaces in `components/`

To create a WebUI interface in `components/` you need to follow different steps from [Creating WebUI Interfaces in `chrome/`](https://www.chromium.org/developers/webui). This guide is specific to creating a WebUI interface in `src/components/`. It is based on the steps I went through to create the WebUI infrastructure for chrome://safe-browsing in 'src/components/safe_browsing/content/browser/web_ui/'.

[TOC]

## Creating the WebUI page

WebUI resources in `components/` will be added in your specific project folder. Create a project folder `src/components/hello_world/`. When creating WebUI resources, follow the [Web Development Style Guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/web/web.md). For a sample WebUI page you could start with the following files:

`src/components/hello_world/hello_world.html:`
```html
<!DOCTYPE HTML>
<html dir="$i18n{textdirection}">
<head>
 <meta charset="utf-8">
 <title>$i18n{helloWorldTitle}</title>
 <link rel="stylesheet" href="hello_world.css">
 <script src="chrome://resources/js/cr.js"></script>
 <script src="chrome://resources/js/load_time_data.js"></script>
 <script src="chrome://resources/js/assert.js"></script>
 <script src="chrome://resources/js/util.js"></script>
 <script src="strings.js"></script>
 <script src="hello_world.js"></script>
</head>
<body style="font-family:$i18n{fontfamily};font-size:$i18n{fontSize}">
  <h1>$i18n{helloWorldTitle}</h1>
  <p id="welcome-message"></p>
</body>
</html>
```

`src/components/hello_world/hello_world.css:`
```css
p {
  white-space: pre-wrap;
}
```

`src/components/hello_world/hello_world.js:`
```js
cr.define('hello_world', function() {
  'use strict';

  /**
   * Be polite and insert translated hello world strings for the user on loading.
   */
  function initialize() {
    $('welcome-message').textContent = loadTimeData.getStringF('welcomeMessage',
        loadTimeData.getString('userName'));
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
  };
});

document.addEventListener('DOMContentLoaded', hello_world.initialize);
```

## Adding the resources

Resource files are specified in a `.grdp` file. Here's our
`components/resources/hello_world_resources.grdp`:

```xml
<?xml version="1.0" encoding="utf-8"?>
<grit-part>
  <include name="IDR_HELLO_WORLD_HTML" file="../../components/hello_world/hello_world.html" type="BINDATA" />
  <include name="IDR_HELLO_WORLD_CSS" file="../../components/hello_world/hello_world.css" type="BINDATA" />
  <include name="IDR_HELLO_WORLD_JS" file="../../components/hello_world/hello_world.js" type="BINDATA" />
</grit-part>
```

Add the created file in `components/resources/dev_ui_components_resources.grd`:

```xml
+<part file="hello_world_resources.grdp" />
```

## Adding URL constants for the new chrome URL

Create the `constants.cc` and `constants.h` files to add the URL constants. This is where you will add the URL or URL's which will be directed to your new resources.

`src/components/hello_world/constants.cc:`
```c++
const char kChromeUIHelloWorldURL[] = "chrome://hello-world/";
const char kChromeUIHelloWorldHost[] = "hello-world";
```

`src/components/hello_world/constants.h:`
```c++
extern const char kChromeUIHelloWorldURL[];
extern const char kChromeUIHelloWorldHost[];
```

## Adding localized strings

We need a few string resources for translated strings to work on the new resource. The welcome message contains a variable with a sample value so that it can be accurately translated. Create a new file `components/hello_world_strings.grdp`. You can add the messages as follow:

```xml
<message name="IDS_HELLO_WORLD_TITLE" desc="A happy message saying hello to the world">
  Hello World!
</message>
<message name="IDS_HELLO_WORLD_WELCOME_TEXT" desc="Message welcoming the user to the hello world page">
  Welcome to this fancy Hello World page <ph name="WELCOME_NAME">$1<ex>Chromium User</ex></ph>!
</message>
```
Add the created file in `components/components_strings.grd`:

```xml
+<part file="hello_world_strings.grdp" />
```

## Adding a WebUI class for handling requests to the chrome://hello-world/ URL

Next we need a class to handle requests to this new resource URL. Typically this will subclass `ChromeWebUI` (but WebUI dialogs should subclass `HtmlDialogUI` instead).

`src/components/hello_world/hello_world_ui.h:`
```c++
#ifndef COMPONENTS_HELLO_WORLD_HELLO_WORLD_UI_H_
#define COMPONENTS_HELLO_WORLD_HELLO_WORLD_UI_H_
#pragma once

#include "content/public/browser/web_ui_controller.h"

// The WebUI for chrome://hello-world
class HelloWorldUI : public content::WebUIController {
 public:
  explicit HelloWorldUI(content::WebUI* web_ui);
  HelloWorldUI(const HelloWorldUI&) = delete;
  HelloWorldUI& operator=(const HelloWorldUI&) = delete;
  ~HelloWorldUI() override;
 private:
};

#endif  // COMPONENTS_HELLO_WORLD_HELLO_WORLD_UI_H_
```

`src/components/hello_world/hello_world_ui.cc:`
```c++
#include "components/hello_world/hello_world_ui.h"

#include "components/grit/components_scaled_resources.h"
#include "components/grit/dev_ui_components_resources.h"
#include "components/hello_world/constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

HelloWorldUI::HelloWorldUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://hello-world source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUIHelloWorldHost);

  // Localized strings.
  static constexpr webui::LocalizedString kStrings[] = {
      {"helloWorldTitle", IDS_HELLO_WORLD_TITLE},
      {"welcomeMessage", IDS_HELLO_WORLD_WELCOME_TEXT},
  };
  html_source->AddLocalizedStrings(kStrings);

  // As a demonstration of passing a variable for JS to use we pass in the name "Bob".
  html_source->AddString("userName", "Bob");
  html_source->UseStringsJs();

  // Add required resources.
  static constexpr webui::ResourcePath kResources[] = {
      {"hello_world.html", IDR_HELLO_WORLD_HTML},
      {"hello_world.css", IDR_HELLO_WORLD_CSS},
      {"hello_world.js", IDR_HELLO_WORLD_JS},
  };
  source->AddResourcePaths(kResources);
  html_source->SetDefaultResource(IDR_HELLO_WORLD_HTML);
}

HelloWorldUI::~HelloWorldUI() {
}
```

## Adding new sources to Chrome

In order for your new class to be built and linked, you need to update the `BUILD.gn` and DEPS files. Create

`src/components/hello_world/BUILD.gn:`
```
sources = [
  "hello_world_ui.cc",
  "hello_world_ui.h",
  ...
```
and `src/components/hello_world/DEPS:`
```
include_rules = [
  "+components/strings/grit/components_strings.h",
  "+components/grit/components_scaled_resources.h"
  "+components/grit/dev_ui_components_resources.h",
]
```

## Adding your WebUI request handler to the Chrome WebUI factory

The Chrome WebUI factory is where you setup your new request handler.

`src/chrome/browser/ui/webui/chrome_web_ui_controller_factory.cc:`
```c++
+ #include "components/hello_world/hello_world_ui.h"
+ #include "components/hello_world/constants.h"
...
+ if (url.host() == chrome::kChromeUIHelloWorldHost)
+   return &NewWebUI<HelloWorldUI>;
```

## Testing

You're done! Assuming no errors (because everyone gets their code perfect the first time) you should be able to compile and run chrome and navigate to `chrome://hello-world/` and see your nifty welcome text!

## Adding a callback handler

You probably want your new WebUI page to be able to do something or get information from the C++ world. For this, we use message callback handlers. Let's say that we don't trust the Javascript engine to be able to add two integers together (since we know that it uses floating point values internally). We could add a callback handler to perform integer arithmetic for us.

`src/components/hello_world/hello_world_ui.h:`
```c++
#include "base/values.h"
#include "content/public/browser/web_ui.h"

// The WebUI for chrome://hello-world
...
    // Set up the chrome://hello-world source.
    content::WebUIDataSource::CreateAndAdd(
        browser_context, hello_world::kChromeUIHelloWorldHost);
+
+   // Register callback handler.
+   RegisterMessageCallback("addNumbers",
+       base::BindRepeating(&HelloWorldUI::AddPositiveNumbers,
+                           base::Unretained(this)));

    // Localized strings.
...
    virtual ~HelloWorldUI();
+
+  private:
+   // Add two positive numbers together using integer arithmetic.
+   void AddPositiveNumbers(base::Value::ConstListView args);
  };
```

`src/components/hello_world/hello_world_ui.cc:`
```c++
  #include "components/hello_world/hello_world_ui.h"
+
+ #include "base/values.h"
  #include "content/public/browser/browser_context.h"
...
  HelloWorldUI::~HelloWorldUI() {
  }
+
+ void HelloWorldUI::AddPositiveNumbers(base::Value::ConstListView args) {
+   // IMPORTANT: Fully validate `args`.
+   CHECK_EQ(3u, args.size());
+   int term1 = args[1].GetInt();
+   CHECK_GT(term1, 0);
+   int term2 = args[2].GetInt();
+   CHECK_GT(term2, 0);
+   base::FundamentalValue result(term1 + term2);
+   AllowJavascript();
+   std::string callback_id = args[0].GetString();
+   ResolveJavascriptCallback(base::Value(callback_id), result);
+ }
```

`src/components/hello_world/hello_world.js:`
```c++
    function initialize() {
+     cr.sendWithPromise('addNumbers', [2, 2]).then((result) =>
+         addResult(result));
    }
+
+   function addResult(result) {
+     alert('The result of our C++ arithmetic: 2 + 2 = ' + result);
+   }

    return {
      initialize: initialize,
    };
```

You'll notice that the call is asynchronous. We must wait for the C++ side to call our Javascript function to get the result.

## Creating a WebUI Dialog

Some pages have many messages or share code that sends messages. To make possible message handling and/or to create a WebUI dialogue `c++->js` and `js->c++`, follow the guide in [WebUI Explainer](https://chromium.googlesource.com/chromium/src/+/main/docs/webui_explainer.md).

## DevUI Pages

DevUI pages are WebUI pages intended for developers, and unlikely used by most users. An example is `chrome://bluetooth-internals`. On Android Chrome, these pages are moved to a separate [Dynamic Feature Module (DFM)](https://chromium.googlesource.com/chromium/src/+/main/docs/android_dynamic_feature_modules.md) to reduce binary size. Most WebUI pages are DevUI. This is why in this doc uses `dev_ui_components_resources.{grd, h}` in its examples.

`components/` resources that are intended for end users are associated with `components_resources.{grd, h}` and `components_scaled_resorces.{grd, h}`. Use these in place of or inadditional to `dev_ui_components_resources.{grd, h}` if needed.

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
