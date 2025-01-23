# chrome-untrusted:// FAQ

[TOC]

## What is “untrustworthy content”?

In this context, untrustworthy content is content that comes from untrustworthy sources, e.g. an image downloaded from the internet, a PDF file provided by the user, etc. Code is also considered “content” in this case.

In general, content coming from the network is considered untrustworthy, regardless of the source and transport protocol.

Examples of trustworthy content include, the contents of `chrome://version` which are populated entirely within the browser process, the contents of `chrome://about` which is a hardcoded list of URLs, etc.

## What is chrome-untrusted://?

It is a new scheme which can be used to serve resources bundled with Chrome and that process untrustworthy content. It has the usual protections provided to `chrome://`, e.g. process isolation, but it won’t be default-granted extra capabilities that are default-granted to `chrome://`.

The `-untrusted` suffix indicates that the [WebUI](webui_explainer.md) processes untrustworthy content. For example, rendering an image provided by users, parsing a PDF file, etc.

The `-untrusted` suffix does not mean the web page is designed to do malicious things, or users should not trust it. Instead, the `-untrusted` suffix is to signal to us, Chromium developers, that this page will process untrustworthy content, and should be assumed to be compromised, much like an ordinary renderer process.

## Why do we need chrome-untrusted://?

### Separate fully trusted WebUIs and untrustworthy ones

`chrome-untrusted://` acts as a technical and semantic boundary between fully-trusted WebUIs and untrustworthy WebUIs.

Technical because developers can use `chrome-untrusted://` to separate their WebUIs into two origins e.g. `chrome://media-app` and `chrome-untrusted://media-app` with access to different capabilities, resources, etc.

Semantic because it indicates to chromium developers and security reviewers that a WebUI is meant to process untrustworthy content and shouldn’t be granted dangerous capabilities.

### chrome:// is too powerful to process untrustworthy content

Historically, `chrome://` pages have been built with the assumption that they are an extension to the browser process, so `chrome://` web pages are granted special capabilities not granted to ordinary web pages. For example, all `chrome://` pages can use Web APIs like camera and mic without requesting permission.

Some WebUIs would like to be able to process untrustworthy content, but granting these capabilities to a `chrome://` page would violate the [rule of 2](security/rule-of-2.md):

 * a `chrome://` page is considered an extension to the browser process
 * the renderer is written in an unsafe programming language (C++).
 * running in an privileged context:

By using `chrome-untrusted://` we put the untrustworthy content into a sandboxed and non-privileged environment (an ordinary renderer, with no dangerous capabilities). This brings us back to safety, a compromised `chrome-untrusted://` page is no worse than an ordinary web page.

`chrome-untrusted://` re-uses a lot of the code that backs `chrome://` pages, so it doesn’t impose a big maintenance burden; even then, our hope is to one day remove all default granted capabilities based on the `chrome://` scheme to the point that the difference between `chrome://` and `chrome-untrusted://` WebUIs is just a semantic one (see previous point).

## When is it appropriate to use chrome-untrusted://?

`chrome-untrusted://` is usually used for implementing privilege separation so that processing untrustworthy content e.g. parsing JSON, displaying an image, running code from the network, etc. is done in an unprivileged context.

Today, the main use case is when we want to have code that ships with Chrome work with untrustworthy content that comes over the network.

## Can I use $js\_library\_from\_url?

Yes. “Content” in this context also includes code.

## Do we grant any extra capabilities to chrome-untrusted://?

Yes, but not by default and with some caveats.

Any team that requires extra capabilities granted to `chrome-untrusted://` should consult with the security team to ensure they are non-dangerous. In this context, we consider non-dangerous any API that we would expose to the renderer process, e.g. UMA.

We recommend using Mojo to expose APIs to `chrome-untrusted://`. Mojo for `chrome-untrusted://` works similarly to how it works for `chrome://` with a few key differences:

* Unlike `chrome://` pages, `chrome-untrusted://` pages don't get access to all renderer exposed Mojo interfaces by default. Use `PopulateChromeWebUIFrameInterfaceBrokers` to expose WebUI specific interfaces to your WebUI. See [this CL](https://crrev.com/c/3138688/5/chrome/browser/chrome_browser_interface_binders.cc) for example.
* The exposed interface has a different threat model: a compromised `chrome-untrusted://` page could try to exploit the interface (e.g. sending malformed messages, closing the Mojo pipe).

When exposing extra capabilities to `chrome-untrusted://`, keep in mind:

* Don't grant any capabilities that we wouldn't grant to a regular renderer. For example, don't expose unrestricted access to Bluetooth devices, but expose a method that opens a browser-controlled dialog where the user chooses a device.
* What you received (from the WebUI page) is untrustworthy. You must sanitize and verify its content before processing.
* What you send (to the WebUI page) could be exfiltrated to the Web. Don't send sensitive information (e.g. user credentials). Only send what you actually need.
* The difference in Mojo interface lifetimes could lead to use-after-free bugs (e.g. a page reloads itself when it shouldn't). We recommend you create and reinitialize the interface on each page load (using [WebUIPrimaryPageChanged](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/web_ui_controller.h;l=54?q=WebUIPrimaryPageChanged&ss=chromium)), and have the JavaScript bind the interface on page load.

We also recommend using Mojo to communicate between parent and child frames whenever possible. See [this CL](https://crrev.com/c/3222406) for example.

You should only use `postMessage()` when transferring objects unsupported by Mojo. For example, Media App uses `postMessage()` to pass a read-only [`FileSystemHandle`](https://developer.mozilla.org/en-US/docs/Web/API/File_System_Access_API) file handle to `chrome-untrusted://media-app` from its parent `chrome://media-app`.

We encourage teams to engage with [SECURITY_OWNERS](https://source.chromium.org/chromium/chromium/src/+/main:ipc/SECURITY_OWNERS) early and get the reviews required.

## Can chrome-untrusted:// be the main document or does it need to be embedded in a `chrome://` page?
Yes, `chrome-untrusted://` can be the main document, although the most common case is for a `chrome://` page to embed a `chrome-untrusted://` subframe.

That said, the `chrome-untrusted://` scheme is an implementation detail of the WebUI and should never be shown to users. This should be factored into account when deciding whether or not to use `chrome-untrusted://` as the main document.

## How do I use chrome-untrusted://?

### Create a standalone chrome-untrusted:// WebUI

1. Create a class overriding `ui::WebUIConfig` and another one overriding `ui::UntrustedWebUIController`

`WebUIConfig` contains properties for the `chrome-untrusted://` page i.e. the host and scheme. In the future, this might also contain other properties like permissions or resources.

`UntrustedWebUIController` register the resources for the page.

```cpp
const char kUntrustedExampleHost[] = "untrusted-example";
const char kUntrustedExampleURL[] = "chrome-untrusted://untrusted-example";

class UntrustedExampleUIConfig : public content::WebUIConfig {
 public:
  UntrustedExampleUIConfig()
    // Set scheme and host.
    : WebUIConfig(content::kChromeUIUntrustedScheme, kUntrustedExampleHost) {}
  ~UntrustedExampleUIConfig() override = default;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override {
    return std::make_unique<UntrustedExampleUI>(web_ui);
  }
};

class UntrustedExampleUI : public ui::UntrustedWebUIController {
 public:
  UntrustedExampleUI::UntrustedExampleUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {

    // Create a URLDataSource and add resources.
    auto* untrusted_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(), kUntrustedExampleURL);
    untrusted_source->AddResourcePath(...);
  }

  UntrustedExampleUI(const UntrustedExampleUI&) = delete;
  UntrustedExampleUI& operator=(const UntrustedExampleUI&) = delete;

  UntrustedExampleUI::~UntrustedExampleUI() = default;
};

```

2. Register the WebUIConfig

Add the `WebUIConfig` to the appropriate list of WebUIConfigs in [`chrome_untrusted_web_ui_configs`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/webui/chrome_untrusted_web_ui_configs.cc).

```cpp
register_config(std::make_unique<chromeos::UntrustedExampleUIConfig>());
```

3. If needed, implement and register the necessary Mojo interfaces. See [this CL](https://crrev.com/c/3138688/5/chrome/browser/chrome_browser_interface_binders.cc) for example.

### Embed chrome-untrusted:// in chrome:// WebUIs

Developers can embed `chrome-untrusted://` iframes in `chrome://` pages. [Example CL](https://chromium-review.googlesource.com/c/chromium/src/+/2037186).

The general steps are:
1. Create a WebUIConfig and UntrustedWebUIController to register the resources for the `chrome-untrusted://` page.
2. Allow the `chrome://` WebUI to embed the corresponding `chrome-untrusted://` WebUI.
```cpp
untrusted_data_source->AddFrameAncestor(kWebUIPageURL)
```
3. Make `chrome-untrusted://` requestable by the main `chrome://` WebUI.
```cpp
web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme)
```
4. Allow the `chrome://` WebUI to embed chrome-untrusted://.
```cpp
trusted_data_source->OverrideContentSecurityPolicy(
    “frame-src ” + kUntrustedExampleURL);
```
5. Add communication mechanism to `chrome-untrusted://` frames. For example, [using Mojo](https://crrev.com/c/3222406), or `postMessage` the JavaScript object is not supported by Mojo.
