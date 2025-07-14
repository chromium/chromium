# Local Prototyping and Testing Chromium Web Platform Features with Web Platform Tests

This document can be useful if you want to create a local prototype of a new
web feature and test it using WPT before proposing any changes in WebDriver BiDi
or in Chromium.

This guide details the process of configuring your local development environment
for prototyping and testing new web features. It covers the entire workflow for
Chromium browsers, from initial ideation and implementing Chromium DevTools
Protocol (CDP) methods to creating and running Web Platform Tests (WPT).

The following aspects are covered:
1. Chrome Devtools Protocol (CDP)
1. WebDriver BiDi CDDL
1. Chromium BiDi
1. WPT

[TOC]

## CDP
If the required functionality is not yet supported by CDP, begin by implementing
it yourself. Clone the Chromium repository to your machine and add your new
methods and events to CDP.
[Refer to this doc guidance](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/devtools_protocol/README.md).

**Don't forget to add [inspector protocol tests](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/http/tests/inspector-protocol/)
to ensure everything works as expected!**
Refer to [this example CL](https://crrev.com/c/6578504).

### Build Chromium
You will need the following targets to be build:
```shell
autoninja -C out/Default chrome chromedriver headless_shell
```
Let’s assume the build artefacts have paths `${LOCAL_BROWSER_BIN}`,
`${LOCAL_HEADLESS_SHELL_BIN}` and `${LOCAL_CHROMEDRIVER_BIN}`.

## WebDriver BiDi CDDL
WebDriver BiDi uses [CDDL](https://datatracker.ietf.org/doc/html/rfc8610) to
declare the types for commands and events. To prototype changes to the WebDriver
BiDi protocol's CDDL before they are merged into the main repository, follow
these steps.

### Clone repository
Clone the
[WebDriver BiDi repository](https://github.com/w3c/webdriver-bidi), add the new
methods and events to the CDDL schemas. The specification text can be omitted at
this prototyping stage. You can refer to the
[emulation.setLocaleOverride](https://github.com/w3c/webdriver-bidi/blob/a8b68a1e3468ffa90502d2507444b9b1d394b287/index.bs#L5810)
command and to the
[browsingContext.ContextCreated](https://github.com/w3c/webdriver-bidi/blob/a8b68a1e3468ffa90502d2507444b9b1d394b287/index.bs#L3092)
event. The important part here is to add the proper CDDL.

### Verify the change
Verify the change in the spec is correct by running this command:
```shell
sh scripts/test.sh
sh scripts/build.sh
```

### Generate CDDL
Generate the CDDL by running this command:
```shell
scripts/cddl/generate.js
```
This will create a file `all.cddl`. Let its path be `${LOCAL_CDDL}`.

### External Specifications
If the changes are done in an existing external spec, for example
[Permissions](https://www.w3.org/TR/permissions/) or
[Web Bluetooth](https://webbluetoothcg.github.io/web-bluetooth/), the CDDL
should be generated differently. For example, for changes in "Web Bluetooth":
1. Checkout [`web-bluetooth`](https://webbluetoothcg.github.io/web-bluetooth/)
   spec locally.
1. Enter the `web-bluetooth` spec directory.
1. Run providing the proper path to the `webdriver-bidi` spec folder.
```sh
../webdriver-bidi/scripts/cddl/generate.js ./index.bs
mv all.cddl bluetooth.cddl
```
This will generate `bluetooth.cddl` in the `web-bluetooth` folder. Let its path
 be `${LOCAL_CDDL}`.

## Chromium BiDi
[Chromium BiDi](https://github.com/GoogleChromeLabs/chromium-bidi) is an
implementation of the WebDriver BiDi protocol for Chromium.

### Regenerate BiDi types
The TypeScript types and zod schemes are generated based on the WebDriver BiDi
CDDL.
```shell
node tools/generate-bidi-types.mjs --cddl-file ${LOCAL_CDDL}
npm run format
```

### Fix the build
If a new BiDi command is added, add it to the
[`CommandProcessor`](https://github.com/GoogleChromeLabs/chromium-bidi/blob/de72d10875fb77c6908cb116bb46a6b3d49491b7/src/bidiMapper/CommandProcessor.ts#L163).
Verify the build works:
```shell
npm run build
```

### Implement BiDi command parameters parsing
This is another manual step requiring implementation effort. You can refer to
[this example pull request](https://github.com/GoogleChromeLabs/chromium-bidi/pull/3544) as an
example.

### Implement the command and event
Now, implement the logic for your new BiDi command or event. This means calling
the new CDP methods or listening for events you added earlier. Since the
TypeScript types for CDP in "Chromium BiDi" aren't automatically updated with
your local changes, you'll encounter TypeScript errors. As a temporary workaround
for prototyping, you'll need to cast your new CDP calls or event handlers to
`any` to allow compilation.

### Add e2e tests
Add [e2e tests](https://github.com/GoogleChromeLabs/chromium-bidi?tab=readme-ov-file#e2e-tests) verifying the new BiDi command works as expected. This is expected to fail with
the Canary Chromium, as the required CDP changes are not present there, so you
will need to point the tests to your local Chromium built by the `BROWSER_BIN`
and `LOCAL_CHROMEDRIVER_BIN` environment variables. Test it in headless shell,
headless and headful modes:
```shell
BROWSER_BIN=${LOCAL_HEADLESS_SHELL_BIN} CHROMEDRIVER_BIN=${LOCAL_CHROMEDRIVER_BIN} HEADLESS=old CHROMEDRIVER=true \
npm run e2e -- ${YOUR_TEST_PATH}

BROWSER_BIN=${LOCAL_BROWSER_BIN} CHROMEDRIVER_BIN=${LOCAL_CHROMEDRIVER_BIN} HEADLESS=true CHROMEDRIVER=true \
npm run e2e -- ${YOUR_TEST_PATH}

BROWSER_BIN=${LOCAL_BROWSER_BIN} CHROMEDRIVER_BIN=${LOCAL_CHROMEDRIVER_BIN} HEADLESS=false CHROMEDRIVER=true \
npm run e2e -- ${YOUR_TEST_PATH}
```

### Build Chromium BiDi
Build again. The path to the built Chromium BiDi script `lib/iife/mapperTab.js`
will be `${LOCAL_MAPPER_TAB_PATH}`.
```shell
npm run build
```

## WPT
After the previous steps are complete, you can add the required WPT tests and endpoints. To run WPT tests with the locally built browser, use the following command in the WPT root directory:
```shell
./wpt run --manifest MANIFEST.json --no-manifest-download \
    --binary ${LOCAL_BROWSER_BIN} \
    --webdriver-binary ${LOCAL_CHROMEDRIVER_BIN} \
    --webdriver-arg=--bidi-mapper-path=${LOCAL_MAPPER_TAB_PATH} \
    chrome \
    infrastructure/testdriver/bidi/emulation/set_locale_override.https.html
```
Refer to [this documentation](https://docs.google.com/document/d/1uQmNMUzznAH_JvJOTllpL2qNhOEzClTkmZliTnlsNIs/edit?tab=t.0#bookmark=id.lh4ijkuvdxhf)
for instructions on how to add the new commands or events to WPT’s
testdriver.js.
