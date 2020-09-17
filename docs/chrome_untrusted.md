# chrome-untrusted:// FAQ

[TOC]

## What is “untrustworthy content”?

In this context, untrustworthy content is content that comes from untrustworthy sources, e.g. an image downloaded from the internet, a PDF file provided by the user, etc. Code is also considered “content” in this case.

In general, content coming from the network is considered untrustworthy, regardless of the source and transport protocol.

Examples of trustworthy content include, the contents of `chrome://version` which are populated entirely within the browser process, the contents of `chrome://about` which is a hardcoded list of URLs, etc.

## What is chrome-untrusted://?

It is a new scheme which can be used to serve resources bundled with Chrome and that process untrustworthy content. It has the usual protections provided to `chrome://`, e.g. process isolation, but it won’t be default-granted extra capabilities that are default-granted to `chrome://`.

The `-untrusted` suffix indicates that the WebUI processes untrustworthy content. For example, rendering an image provided by users, parsing a PDF file, etc.

The `-untrusted` suffix does not mean the web page is designed to do malicious things, or users should not trust it. Instead, the `-untrusted` suffix is to signal to us, Chromium developers, that this page will process untrustworthy content, and should be assumed to be compromised, much like an ordinary renderer process.

## Why do we need chrome-untrusted://?

### Separate fully trusted WebUIs and untrustworthy ones

`chrome-untrusted://` acts as a technical and semantic boundary between fully-trusted WebUIs and untrustworthy WebUIs.

Technical because developers can use `chrome-untrusted://` to separate their WebUIs into two origins e.g. `chrome://media-app` and `chrome-untrusted://media-app` with access to different capabilities, resources, etc.

Semantic because it indicates to chromium developers and security reviewers that a WebUI is meant to process untrustworthy content and shouldn’t be granted dangerous capabilities.

### chrome:// is too powerful to process untrustworthy content

Historically, `chrome://` pages have been built with the assumption that they are an extension to the browser process, so `chrome://` web pages are granted special capabilities not granted to ordinary web pages. For example, all `chrome://` pages can use Web APIs like camera and mic without requesting permission.

Some WebUIs would like to be able to process untrustworthy content, but granting these capabilities to a `chrome://` page would violate the rule of 2:
running in an privileged context:
 * a `chrome://` page is considered an extension to the browser process
 * the renderer is written in an unsafe programming language (C++).

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

We currently use `postMessage()` to expose certain APIs to `chrome-untrusted://`. For example, the Media App uses `postMessage()` to pass a read-only file handle to `chrome-untrusted://media-app` from `chrome-untrusted://`. Teams are encouraged to get a review from someone in [SECURITY_OWNERS](https://source.chromium.org/chromium/chromium/src/+/master:ipc/SECURITY_OWNERS) when exposing capabilities over postMessage.

We are hoping to move to Mojo to improve auditability of these APIs and to make the security review required.

## Can chrome-untrusted:// be the main document or does it need to be embedded in a `chrome://` page?
Yes, `chrome-untrusted://` can be the main document, although the most common case is for `chrome://` to embed a `chrome-untrusted://` page.

That said, the `chrome-untrusted://` scheme is an implementation detail of the WebUI and should never be shown to users. This should be factored into account when deciding whether or not to use `chrome-untrusted://` as the main document.

## How do I use chrome-untrusted://?

(This will be out of date soon)
### Create a URLDataSource and register it.

This will make its resources loadable in Chrome.

```
auto* data_source =
    WebUIDataSource::Create(GURL(“chrome-untrusted://example/”));
// Add resources i.e. data_source->SetRequestFilter
WebUIDataSource::Add(browser_context, data_source);
```

### Embed chrome-untrusted:// in chrome:// WebUIs

Developers can embed `chrome-untrusted://` iframes in `chrome://` pages. [Example CL](https://chromium-review.googlesource.com/c/chromium/src/+/2037186).

The general steps are:
1. Create and register a `URLDataSource` that serves necessary resources for the `chrome-untrusted://` page.
2. Allow the `chrome://` WebUI to embed the corresponding `chrome-untrusted://` WebUI.
```
untrusted_data_source->AddFrameAncestor(kWebUIPageURL)
```
3. Make `chrome-untrusted://` requestable by the main `chrome://` WebUI.
```
web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme)
```
4. Allow the `chrome://` WebUI to embed chrome-untrusted://.
```
trusted_data_source->OverrideContentSecurityPolicy(
    “frame-src ” + kChromeUntrustedPageURL);
```
5. Add communication mechanism to `chrome-untrusted://` frames. For example, `iframe.postMessage()` and `window.onmessage`.
