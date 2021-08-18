# Origin Trial for Subresource Loading with Web Bundles

This document is for web developers who want to participate in Origin Trial for
[Subresource Loading with Web Bundles][explainer].

- [Chrome Status]
- [Explainer]
- [Intent to Experiment](https://groups.google.com/a/chromium.org/g/blink-dev/c/9CwkzaF_eQ4/m/kuR07FTTCAAJ)
- [Registration Form](https://developer.chrome.com/origintrials/#/view_trial/-6307291278132379647)

## Origin Trial timeline

Chrome M90-M96.

## How to create a bundle

There are several tools available.

- Go: [gen-bundle](https://github.com/WICG/webpackage/tree/master/go/bundle)
  tool in the WICG/webpackage repository.
- npm: [wbn](https://www.npmjs.com/package/wbn)

## What works in Chrome M90 or later.

Chrome M90 or later supports `<link>`-based API explained in the [Explainer]. In
addition to `resources` attribute, `scopes` attribute is also supported.

### Examples

Using `resources` attribute:

```html
<link
  rel="webbundle"
  href="https://example.com/dir/subresources.wbn"
  resources="https://example.com/dir/a.js https://example.com/dir/b.js https://example.com/dir/c.png"
/>
```

Using `scopes` attribute:

```html
<link
  rel="webbundle"
  href="https://example.com/dir/subresources.wbn"
  scopes="https://example.com/dir/js/
          https://example.com/dir/img/
          https://example.com/dir/css/"
/>
```

Using both `resources` and `scopes` attribute also works:

```html
<link
  rel="webbundle"
  href="https://example.com/dir/subresources.wbn"
  resources="https://example.com/dir/a.js https://example.com/dir/b.js"
  scopes="https://example.com/dir/js/
          https://example.com/dir/img/
          https://example.com/dir/css/"
/>
```

A `urn:uuid` URL is also supported:

```html
<link
  rel="webbundle"
  href="https://example.com/dir/subresources.wbn"
  resources="urn:uuid:f81d4fae-7dec-11d0-a765-00a0c91e6bf6"
/>

<iframe src="urn:uuid:f81d4fae-7dec-11d0-a765-00a0c91e6bf6"></iframe>
```

## Feature detection

You can use
[`HTMLLinkElement.relList`](https://html.spec.whatwg.org/multipage/semantics.html#dom-link-rellist)
for feature detection.

```js
const link = document.createElement("link");
if (link.relList.supports("webbundle")) {
   // Supported
   ...
} else {
   // Unsupported
   ...
}
```

## Feature detection for `scopes` (which is available in M90 or later)

If you want to make sure you can use `scopes` attribute, the following should
work:

```js
if ("scopes" in HTMLLinkElement.prototype) {
  // `scopes` attribute is supported. Chrome is in M90 or later.
  ...
} else {
  // `scopes` attribute is not supported. Chrome is in M89 or earlier.
  ...
}
```

A `resources` attribute is always supported if
`link.relList.supports("webbundle")` is true.

Chrome M89 will show `ExternalProtocolDialog` for a iframe loading with
`urn:uuid` URL. You should check Chrome is M90 or later to avoid that.

# How to try this feature locally

Enable _Experimental Web Platform Features_ flag
([chrome://flags/#enable-experimental-web-platform-features](chrome://flags/#enable-experimental-web-platform-features)).
Note that an earlier version of Chrome might not support this feature.

[chrome status]: https://www.chromestatus.com/feature/5710618575241216
[explainer]:
  https://github.com/WICG/webpackage/blob/main/explainers/subresource-loading.md
