# ChromeVox (for developers)

ChromeVox is the built-in screen reader on Chrome OS. It was originally
developed as a separate extension but now the code lives inside of the Chromium
tree and it's built as part of Chrome OS.

NOTE: ChromeVox ships also as an extension on the Chrome webstore. This version
of ChromeVox is known as ChromeVox Classic and is loosely related to ChromeVox
(on Chrome OS). All references to ChromeVox relate only to ChromeVox on Chrome
OS.

To start or stop ChromeVox, press Ctrl+Alt+Z at any time.

## Developer Info

Code location: ```chrome/browser/resources/chromeos/chromevox```

Ninja target: it's built as part of "chrome", but you can build and run
browser_tests to test it (Chrome OS target only - you must have target_os =
"chromeos" in your GN args first).

## Developing On Linux

ChromeVox for Chrome OS development is done on Linux.

See [ChromeVox on Desktop Linux](chromevox_on_desktop_linux.md)
for more information.

## Debugging ChromeVox

There are options available that may assist in debugging ChromeVox. Here are a
few use cases.

### Feature development

When developing a new feature, it may be helpful to save time by not having to
go through a compile cycle. This can be achieved by setting
```chromevox_compress_js``` to 0 in
chrome/browser/resources/chromeos/chromevox/BUILD.gn, or by using a debug build.

In a debug build or with chromevox_compress_js off, the unflattened files in the
Chrome out directory (e.g. out/Release/resources/chromeos/chromevox/). Now you
can hack directly on the copy of ChromeVox in out/ and toggle ChromeVox to pick
up your changes (via Ctrl+Alt+Z).

### Fixing bugs

The easiest way to debug ChromeVox is from an external browser. Start Chrome
with this command-line flag:

```out/Release/chrome --remote-debugging-port=9222```

Now open http://localhost:9222 in a separate instance of the browser, and debug the ChromeVox extension background page from there.

Another option is to use emacs indium (available through M-x
package-list-packages).

It also talks to localhost:9222 but integrates more tightly into emacs instead.

Another option is to use the built-in developer console. Go to the
ChromeVox options page with Search+Shift+o, o; then, substitute the
“options.html” path with “background.html”, and then open up the
inspector.

### Running tests

Build the browser_tests target. To run lots of tests in parallel, run it like
this:

```
out/Release/browser_tests --test-launcher-jobs=20 --gtest_filter=ChromeVox*
```

Use a narrower test filter if you only want to run some of the tests. For
example, most of the ChromeVox Next tests have "E2E" in them (for "end-to-end"),
so to only run those:

```out/Release/browser_tests --test-launcher-jobs=20 --gtest_filter="*E2E*"```
