# ChromeVox (for developers)

ChromeVox is the built-in screen reader on Chrome OS. It was originally
developed as a separate extension but over time it has been incorporated into
the operating system itself. Now the code lives inside of the Chromium
tree and it's built and shipped as part of Chrome OS.

NOTE: ChromeVox ships also as an extension on the Chrome webstore. This version
of ChromeVox is known as ChromeVox Classic and is loosely related to ChromeVox
(on Chrome OS). All references to ChromeVox relate only to ChromeVox on Chrome
OS.

To start or stop ChromeVox, press Ctrl+Alt+Z on your ChromeOS device at any
time.

## Developer Info

Code location: ```chrome/browser/resources/chromeos/accessibility/chromevox```

Ninja target: it's built as part of "chrome", but you can build and run
browser_tests to test it (Chrome OS target only - you must have target_os =
"chromeos" in your GN args first).

## Developing On Linux

ChromeVox for Chrome OS development is done on Linux.

See [ChromeVox on Desktop Linux](chromevox_on_desktop_linux.md)
for more information.

## Code Structure

The `chromevox/` extension directory is broken into subdirectories, based on
what context the code runs in. The different contexts are as follows:

* The background context (`chromevox/background/`) contains the bulk of the
ChromeVox logic, and runs in the background page (soon to be a background
service worker). To group code by its logical function, it has the following
subdirectories:

    - `chromevox/background/braille/`, which contains logic around braille
    input/output.

    - `chromevox/background/editing/`, which contains the logic to handle input
    into text fields.

    - `chromevox/background/event/`, which contains the logic that handles
    events from the various APIs.

    - `chromevox/background/logging/`, which contains logic to generate the
      content shown on the log page.

    - `chromevox/background/output/`, which contains the logic to generate the
      text that is spoken or sent to the braille display. More details are in
      [the README.md file within that directory](/chrome/browser/resources/chromeos/accessibility/chromevox/background/output/).

    - `chromevox/background/panel/`, which contains the logic to support the
      ChromeVox panel.

* The content script context (`chromevox/injected/`) contains the code that is
injected into web pages. At this point, this is only used to support the Google
Docs workaround. When that is resolved, it is anticipated this directory will be
removed.

* The learn mode context (`chromevox/learn_mode/`) contains the code to render
and run the ChromeVox learn mode (which is different from the tutorial).

* The log context (`chromevox/log_page/`) contains the code specific to showing
the ChromeVox log page.

* The options context (`chromevox/options/`) contains the code for the ChromeVox
settings page. There is an ongoing effort to migrate this page into the ChromeOS
settings app, after which this directory will be unneeded.

* The panel context (`chromevox/panel/`) contains the code that renders and
performs the logic of the ChromeVox panel, shown at the top of the screen. When
the onscreen command menus are shown, that is also rendered in this context.

* The tutorial context (`chromevox/tutorial/`) contains resources used
exclusively by the ChromeVox tutorial.

Other subdirectories also have specific purposes:

* The common directory (`chromevox/common/`) contains files that can safely be
shared between multiple contexts. These files must have no global state, as each
context has its own global namespace. To get information between the contexts,
bridge objects are used to pass structured messages. Any data passed through
these bridges loses any and all class information, as it is converted to JSON in
the process of being sent.

    - The subdirectory `chromevox/common/braille/` contains common logic
      specific to braille

* The earcons directory (`chromevox/earcons/`) contains the audio files for any
short indicator sounds (earcons) used by ChromeVox to express information
without words.

* The images directory (`chromevox/images/`) contains any images used in any
context.

* The testing directory (`chromevox/testing/`) contains files that are used
exclusively in testing.

* The third_party directory (`chromevox/third_party`) contains open source code
from other developers that is used in the ChromeVox extension.

* The tools directory (`chromevox/tools`) contains python scrips used for
building ChromeVox. Eventually these should be moved into the common
accessibility directory.

## Debugging ChromeVox

There are options available that may assist in debugging ChromeVox. Here are a
few use cases.

### Feature development

When developing a new feature, it may be helpful to save time by not having to
go through a compile cycle. This can be achieved by setting
```chromevox_compress_js``` to 0 in
chrome/browser/resources/chromeos/accessibility/chromevox/BUILD.gn, or by using
a debug build.

In a debug build or with chromevox_compress_js off, the unflattened files in the
Chrome out directory
(e.g. out/Release/resources/chromeos/accessibility/chromevox/). Now you
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

### Debugging ChromeOS

To debug ChromeVox in ChromeOS, you need to add the command-line flag to the
config file in device under test(DUT) instead of starting chrome from command
line.

```
(dut) $ echo " --remote-debugging-port=9222 " >> /etc/chrome_dev.conf
(dut) $ restart ui
```

This is also written in
[Simple Chrome Workflow Doc](https://chromium.googlesource.com/chromiumos/docs/+/HEAD/simple_chrome_workflow.md#command_line-flags-and-environment-variables).

You need to ssh from your development device into your DUT forwarding port 9222
to open ChromeVox extension background page in your dev device, for example
```
ssh my_crbook -L 3333:localhost:9222
```

Then open the forwarded port in the development device, http://localhost:3333 in
the example.

You may need to remove rootfs verification to write to `/etc/chrome_dev.conf`.

```
(dut) $ crossystem dev_boot_signed_only=0
(dut) $ sudo /usr/share/vboot/bin/make_dev_ssd.sh --remove_rootfs_verification
(dut) $ reboot
```

See
[Chromium OS Doc](https://chromium.googlesource.com/chromiumos/docs/+/HEAD/developer_mode.md#disable-verity)
for more information about removing rootfs verification.

### Running tests

Build the browser_tests target. To run lots of tests in parallel, run it like
this:

```
out/Release/browser_tests --test-launcher-jobs=20 --gtest_filter=ChromeVox*
```

Use a narrower test filter if you only want to run some of the tests. For
example, most of the ChromeVox Next tests have "E2E" in them (for "end-to-end"),
so to only run those:

```out/Release/browser_tests --test-launcher-jobs=20 --gtest_filter="*ChromeVox*E2E*"```
