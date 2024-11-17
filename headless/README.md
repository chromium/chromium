# Headless Chromium

Headless Chromium allows running Chromium in a headless/server environment.
Expected use cases include loading web pages, extracting metadata (e.g., the
DOM) and generating bitmaps from page contents -- using all the modern web
platform features provided by Chromium and Blink.

As of M118, precompiled `headless_shell` binaries are available for download
under the name `chrome-headless-shell` via [Chrome for Testing
infrastructure](https://googlechromelabs.github.io/chrome-for-testing/).

There are two ways to use Headless Chromium:

## Usage via the DevTools remote debugging protocol

1. Start a normal Chrome binary with the `--headless=old` command line flag:

```sh
$ chrome --headless=old --remote-debugging-port=9222 https://chromium.org/
```

2. Navigate to `chrome://inspect/` in another instance of Chrome.

## Usage from Node.js

For example, the [chrome-remote-interface](https://github.com/cyrus-and/chrome-remote-interface) Node.js package can be used to
extract a page's DOM like this:

```js
const CDP = require('chrome-remote-interface');

(async () => {
  let client;
  try {
    // Connect to browser
    client = await CDP();

    // Extract used DevTools domains.
    const {Page, Runtime} = client;

    // Enable events on domains we are interested in.
    await Page.enable();
    await Page.navigate({url: 'https://example.com'});
    await Page.loadEventFired();

    // Evaluate outerHTML after page has loaded.
    const expression = {expression: 'document.body.outerHTML'};
    const { result } = await Runtime.evaluate(expression);
    console.log(result.value);

  } catch (err) {
    console.error('Cannot connect to browser:', err);

  } finally {
    if (client) {
      await client.close();
    }
  }
})();
```

Alternatvely, the [Puppeteer](https://pptr.dev/guides/what-is-puppeteer) Node.js package can be used to communicate
with headless, for example:
```js
import puppeteer from 'puppeteer';

(async () => {
  const browser = await puppeteer.launch({headless: 'shell'});

  const page = await browser.newPage();
  await page.goto('https://example.com');

  const title = await page.evaluate(() => document.title);
  console.log(title);

  await browser.close();
})();
```

## Resources and Documentation

Mailing list: [headless-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/headless-dev)

Bug tracker: [Internals>Headless](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=component%3AInternals%3EHeadless)

[File a new bug](https://bugs.chromium.org/p/chromium/issues/entry?components=Internals%3EHeadless) ([bit.ly/2pP6SBb](https://bit.ly/2pP6SBb))

* [Runtime headless mode on Windows OS](https://docs.google.com/document/d/12c3bSEbmpeGevuyFHcvEKw9br6CkFJSS2saQynBjIzE)
* [BeginFrame sequence numbers + acknowledgements](https://docs.google.com/document/d/1nxaunQ0cYWxhtS6Zzfwa99nae74F7gxanbuT5JRpI6Y/edit#)
* [Deterministic page loading for Blink](https://docs.google.com/document/d/19s2g4fPP9p9qmMZvwPX8uDGbb-39rgR9k56B4B-ueG8/edit#)
* [Crash dumps for Headless Chrome](https://docs.google.com/document/d/1l6AGOOBLk99PaAKoZQW_DVhM8FQ6Fut27lD938CRbTM/edit)
* [Runtime headless mode for Chrome](https://docs.google.com/document/d/1aIJUzQr3eougZQp90bp4mqGr5gY6hdUice8UPa-Ys90/edit#)
* [Virtual Time in
  Blink](https://docs.google.com/document/d/1y9KDT_ZEzT7pBeY6uzVt1dgKlwc1OB_vY4NZO1zBQmo/edit?usp=sharing)
* [Headless Chrome architecture (Design Doc)](https://docs.google.com/document/d/11zIkKkLBocofGgoTeeyibB2TZ_k7nR78v7kNelCatUE)
* [Session isolation in Headless Chrome](https://docs.google.com/document/d/1XAKvrxtSEoe65vNghSWC5S3kJ--z2Zpt2UWW1Fi8GiM/edit)
* [Headless Chrome mojo service](https://docs.google.com/document/d/1Fr6_DJH6OK9rG3-ibMvRPTNnHsAXPk0VzxxiuJDSK3M/edit#heading=h.qh0udvlk963d)
* [Controlling BeginFrame through DevTools](https://docs.google.com/document/d/1LVMYDkfjrrX9PNkrD8pJH5-Np_XUTQHIuJ8IEOirQH4/edit?ts=57d96dbd#heading=h.ndv831lc9uf0)
* [Viewport bounds and scale for screenshots](https://docs.google.com/document/d/1VTcYz4q_x0f1O5IVrvRX4u1DVd_K34IVUl1VULLTCWw/edit#heading=h.ndv831lc9uf0)
* [BlinkOn 6 presentation slides](https://docs.google.com/presentation/d/1gqK9F4lGAY3TZudAtdcxzMQNEE7PcuQrGu83No3l0lw/edit#slide=id.p)
