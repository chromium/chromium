# Trusted Types on WebUI

[TOC]

## What is Trusted Types?

[Trusted Types](https://web.dev/trusted-types/) is a defense in depth
mitigation for DOM-based Cross Site Scripting attacks. Trusted Types
introduces a runtime type system for dangerous sinks (e.g. `elem.innerHTML`,
`eval`, `scriptElem.src`, etc), and only allows Trusted Types (i.e.
`TrustedHTML`, `TrustedScript`, or `TrustedScriptURL`) as an assignment to those
sinks.

While [CSP](https://developer.mozilla.org/en-US/docs/Web/HTTP/CSP) in general
tries to mitigate the exploitability of an injection by only allowing certain
script to be executed (via allow-list of host, nonce, hash, etc), Trusted Types
provides a way to enforce validation for all injections. This is ideal because
without Trusted Types, any other resources such as CSS, image, video, audio, etc
can be injected by default in WebUI pages, which could cause other types of bugs
in the WebUI renderer such as memory corruption bugs.

## How can I "Trusted Type" my code?

**Note: If your JS code will also run on Chromium for iOS (i.e. WebKit), you
should have an if statement to check the Trusted Types support before using
methods/properties under `window.trustedTypes` :**

```
if (window.trustedTypes) {
  // Trusted Types is supported, let's use Trusted Types 😎
  elem.innerHTML = trustedTypes.emptyHTML;
} else {
  // Trusted Types is NOT supported 😔
  elem.innerHTML = '';
}
```

### Change empty string assignment to dangerous sinks

Example code:

```
document.body.innerHTML = '';
```

This will be a Trusted Types violation because the value we are assigning to
a dangerous sink is not a Trusted Type.
This can be converted to:

```
document.body.innerHTML = trustedTypes.emptyHTML;
```

There is also `trustedTypes.emptyScript` to clear script contents.

### Change text assignment to dangerous sinks

Example code:

```
document.body.innerHTML = 'Hello Guest!';
```

Because this is just a text assignment, this can be converted to:

```
document.body.textContent = 'Hello Guest!';
```

### Change HTML assignment to dangerous sinks

#### Use _template_ element

Example code:

```
document.body.innerHTML = '<div><p>' + loadTimeData.getString('foo') + '</p>
</div>';
```

This can be converted by adding _template_ element to HTML file:

```
<template id="foo-template">
  <div>
    <p></p>
  </div>
</template>
```

And then adding following JS code to JS file:

```
// body might already have some contents, so let's clear those first 😊
document.body.innerHTML = trustedTypes.emptyHTML;

const temp = document.querySelector('#foo-template').cloneNode(true).content;
temp.querySelector('p').textContent = loadTimeData.getString('foo');
document.body.appendChild(temp);
```

#### Use DOM APIs

In cases where you don't have control over the HTML file (e.g. converting common
JS libraries), you can use DOM APIs.
Example code:

```
document.body.innerHTML += '<p>' + loadTimeData.getString('foo') + '</p>';
```

And then adding following JS code to JS file:

```
const p = document.createElement('p');
p.textContent = loadTimeData.getString('foo');
document.body.appendChild(p);
```

#### Use `trustedTypes.createPolicy`

If you don't have control of the HTML file and use of DOM APIs isn't ideal for
readability, you can [use `trustedTypes.createPolicy`]
(https://web.dev/trusted-types/#create-a-trusted-type-policy).
Example code:

```
document.body.innerHTML = '<div class="tree-row">' +
    '<span class="expand-icon"></span>' +
    '<span class="tree-label-icon"></span>' +
    '<span class="tree-label"></span>' +
    '</div>' +
    '<div class="tree-children" role="group"></div>';
```

This can be converted to:

```
const htmlString = '<div class="tree-row">' +
    '<span class="expand-icon"></span>' +
    '<span class="tree-label-icon"></span>' +
    '<span class="tree-label"></span>' +
    '</div>' +
    '<div class="tree-children" role="group"></div>';

const staticHtmlPolicy = trustedTypes.createPolicy(
    'foo-static', {createHTML: () => htmlString});

// Unfortunately, a string argument to createHTML is required.
// https://github.com/w3c/webappsec-trusted-types/issues/278
document.body.innerHTML = staticHtmlPolicy.createHTML('');
```

This case also requires changes in C++, as we need to allow the `foo-static`
Trusted Type policy (created above in `trustedTypes.createPolicy`) in the CSP
header.

```
source->OverrideContentSecurityPolicy(
    network::mojom::CSPDirectiveName::TrustedTypes,
    "trusted-types foo-static;");
```

### Change script URL assignment to dangerous sinks

Example code:

```
const script = document.createElement('script');
script.src = 'chrome://resources/foo.js';
document.body.appendChild(script);
```

This can be converted to:

```
const staticUrlPolicy = trustedTypes.createPolicy(
    'foo-js-static',
    {createScriptURL: () => 'chrome://resources/foo.js'});

const script = document.createElement('script');
// Unfortunately, a string argument to createScriptURL is required.
// https://github.com/w3c/webappsec-trusted-types/issues/278
script.src = staticUrlPolicy.createScriptURL('');
document.body.appendChild(script);
```

This case also requires changes in C++, as we need to allow the `foo-js-static`
Trusted Type policy (created above in `trustedTypes.createPolicy`) in the CSP
header.

```
source->OverrideContentSecurityPolicy(
    network::mojom::CSPDirectiveName::TrustedTypes,
    "trusted-types foo-js-static;");
```

## How to disable Trusted Types?

In case there is no way to support Trusted Types in a WebUI page, you can
disable Trusted Types with following code:

```
source->DisableTrustedTypesCSP();
```

## How to add a test for Trusted Types on a WebUI page?

You can add your WebUI page to [this list][browsertest-list] and it will check
for Trusted Types violations on your WebUI page.

[browsertest-list]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/webui/chrome_url_data_manager_browsertest.cc;l=194;drc=de8ade0753244ff6d1ef20cb2a38fe292fe9ba0a

## Sample CLs

1. [Remove innerHTML usage in chrome://interstitials](https://crrev.com/c/2245937)
2. [Trusted Type various WebUI](https://crrev.com/c/2236992)
3. [Trusted Type WebRTC internals](https://crrev.com/c/2208950)
