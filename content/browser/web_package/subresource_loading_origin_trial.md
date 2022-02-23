# Origin Trial for Subresource Loading with Web Bundles

This document is for web developers who want to participate in Origin Trial for
[Subresource Loading with Web Bundles][explainer].

- [Chrome Status]
- [Explainer]
- [Intent to Experiment](https://groups.google.com/a/chromium.org/g/blink-dev/c/9CwkzaF_eQ4/m/kuR07FTTCAAJ)
- [Registration Form](https://developer.chrome.com/origintrials/#/view_trial/-6307291278132379647)

## Origin Trial timeline

- Chrome M90-M101

## How to create a bundle

There are several tools available.

- Go: [gen-bundle](https://github.com/WICG/webpackage/tree/master/go/bundle)
  tool in the WICG/webpackage repository.
- npm: [wbn](https://www.npmjs.com/package/wbn)

## What works in Chrome M97+

### `<script>`-based API

Example:

```html
<script type="webbundle">
{
  "source": "https://example.com/dir/subresources.wbn",
  "credentials": "include",
  "resources": ["a.js", "b.js", "c.png"],
  "scopes": ["css"]
}
</script>
```

### `uuid-in-package` URL

Example:

```html
<script type="webbundle">
{
  "source": "https://example.com/dir/subresources.wbn",
  "resources": ["uuid-in-package:f81d4fae-7dec-11d0-a765-00a0c91e6bf6"]
}
</script>

<iframe src="uuid-in-package:f81d4fae-7dec-11d0-a765-00a0c91e6bf6"></iframe>
```

### Web Bundles format version "b2"

Chrome M97+ supports
[the latest Web Bundles format](https://wpack-wg.github.io/bundled-responses/draft-ietf-wpack-bundled-responses.html)
(called as "b2").

## What works in Chrome M90 or later.

Chrome M90+ supports a `<link>`-based API, a `urn:uuid` URL and the old
WebBundle format
["b1"](https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html).
They are still supported in Chrome M97+, however, we strongly recommend origin
trial participants to use new APIs in Chrome M97+.

This guide no longer covers the old APIs, which we plan to remove eventually.
Please see
[the previous revision of this guide](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/web_package/subresource_loading_origin_trial.md;drc=1454cf984a485a136c4a525ab79f6cf0a3877504)
for the old APIs and [the migration guide](https://docs.google.com/document/d/1hAl7jb-a9WET_mSeHBD9HxIBUwUe65Dbyn6u6LRB61s/edit?usp=sharing).

## Feature detection

You can use
[`HTMLScriptElement.supports(type)`](https://html.spec.whatwg.org/multipage/scripting.html#dom-script-supports)
for feature detection.

```js
if (HTMLScriptElement.supports("webbundle")) {
   // Supported
   ...
} else {
   // Unsupported
   ...
}
```

# How to try this feature locally

Enable _Experimental Web Platform Features_ flag
([chrome://flags/#enable-experimental-web-platform-features](chrome://flags/#enable-experimental-web-platform-features)).
Note that an earlier version of Chrome might not support this feature.

[chrome status]: https://www.chromestatus.com/feature/5710618575241216
[explainer]: https://github.com/WICG/webpackage/blob/main/explainers/subresource-loading.md
