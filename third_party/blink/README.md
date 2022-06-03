# Blink

Blink is the [browser engine](https://en.wikipedia.org/wiki/Browser_engine) used
by Chromium. It is located in `src/third_party/blink`.

See also the [Blink](https://www.chromium.org/blink) page on chromium.org.

## Code Policy

Blink follows [`content` guidelines](../../content/README.md): only code that
implements web platform features should live in Blink.

## Directory structure

- [`common/`](common/README.md): code that can run in the browser process
  or renderer process.
- [`public/`](public/README.md): the Blink Public API, used primarily by
  the [content module](../../content/README.md).
- [`renderer/`](renderer/README.md): code that runs in the renderer process
  (most of Blink).
- [`web_tests/`](web_tests/README.md): integration tests called "web tests".
