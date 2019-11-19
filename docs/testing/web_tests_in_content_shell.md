# Running web tests using the content shell

## Compiling

If you want to run web tests,
[build the target `blink_tests`](web_tests.md); this includes all the other
binaries required to run the tests.

## Running

### Using `run_web_tests.py`

You can run web tests using `run_web_tests.py` (in
`src/third_party/blink/tools`).

```bash
python third_party/blink/tools/run_web_tests.py storage/indexeddb
```
To see a complete list of arguments supported, run with `--help`.

***promo
You can add `<path>/third_party/blink/tools` into `PATH` so that you can
run it from anywhere without the full path.
***

### Run Web Tests Directly with Content Shell

In some cases (e.g. for debugging), you can run web tests directly with
Content Shell executable, with the `--run-web-tests` flag:

```bash
out/Default/content_shell --run-web-tests <url>|<full_test_source_path>|<relative_test_path>
```

`<relative_test_path>` is relative to the [web_tests](../../third_party/blink/web_tests)
directory, regardless of the current directory.

For example:

```bash
out/Default/content_shell --run-web-tests fast/forms/001.html
```
or

```bash
out/Default/content_shell --run-web-tests \
    /home/user/chrome/src/third_party/blink/web_tests/fast/forms/001.html
```
or

```bash
out/Default/content_shell --run-web-tests ~/test/temp-test.html
```

By default, it dumps the text result only (as the dump of pixels and audio
binary data is not human readable) and quits. This can meet the requirement of
most debugging requirements. If you need to interactively debug the test page
(e.g. using devtools), you'll need to run Content Shell [as a simple
browser](#As-a-simple-browser).

In rare cases, to run Content Shell in the exact same way as
`run_web_tests.py` runs it, you need to run it in the
[protocol mode](../../content_shell/browser/web_test/test_info_extractor.h).

*** note
On the Mac, use `Content Shell.app`, not `content_shell`.

```bash
out/Default/Content\ Shell.app/Contents/MacOS/Content\ Shell ...
```
On Windows, use `content_shell.exe`.
***

#### Running HTTP Tests in Content Shell

HTTP tests reside under [web_tests/http/tests](../../third_party/blink/web_tests/http/tests).
You need to start a web server first:

```bash
python third_party/blink/tools/run_blink_httpd.py
```
Then run the test with a localhost URL:

```bash
out/Default/content_shell --run-web-tests http://localhost:8000/<test>
```

#### Running WPT Tests in Content Shell

Similar to HTTP tests, many WPT (a.k.a. web-platform-tests under
[web_tests/external/wpt](../../third_party/blink/web_tests/external/wpt) directory)
tests require some setup before running in Content Shell:

```bash
python third_party/blink/tools/run_blink_wptserve.py
```
Then run the test:

```bash
out/Default/content_shell --run-web-tests http://localhost:8001/<test>
```

If the test requires HTTPS (e.g. the file name contains ".https."), use the
following command instead:

```bash
out/Default/content_shell --run-web-tests https://localhost:8444/<test>
```

### As a simple browser

You can run the shell directly as a simple browser:

```bash
out/Default/content_shell
```

This allows you see how your changes look in Chromium. You can inspect the page
by right clicking and selecting 'Inspect Element'.

You can also use `--remote-debugging-port`

```bash
out/Default/content_shell --remote-debugging-port=9222
```
and open `http://localhost:9222` from another browser to inspect the page.
This is useful when you don't want DevTools to run in the same Content Shell,
e.g. when you are logging a lot and don't want the log from DevTools
or when DevTools is unstable in the current revision due to some bugs.

#### Debug WPT

If you want to debug WPT with devtools in Content Shell, you will first need to
start the server:

```bash
python third_party/blink/tools/run_blink_wptserve.py
```

Then start Content Shell with some additional flags:

```bash
out/Default/content_shell --enable-experimental-web-platform-features --ignore-certificate-errors --host-resolver-rules="MAP nonexistent.*.test ~NOTFOUND, MAP *.test. 127.0.0.1, MAP *.test 127.0.0.1"
```

## Debugging

### `--single-process`

The command line switch `--single-process` is useful for starting
content_shell in gdb. In most cases, `--single-process` is good for debugging
except when you want to debug the multiple process behavior or when we have
some bug breaking `--single-process` in particular cases.

### Web tests

See [Run Web Tests Directly with Content Shell](#Run-Web-Tests-Directly-with-Content-Shell).
In most cases you don't need `--single-process` because `content_shell` is
in single process mode when running most web tests.

See [DevTools frontend](../../third_party/devtools-frontend/src/README.md#basics)
for the commands that are useful for debugging devtools web tests.

### In The Default Multiple Process Mode

In rare cases, you need to debug Content Shell in multiple process mode.
You can ask Content Shell to wait for you to attach a debugger once it spawns a
renderer process by adding the `--renderer-startup-dialog` flag:

```bash
out/Default/content_shell --renderer-startup-dialog --no-sandbox
```

Debugging workers and other subprocesses is simpler with
`--wait-for-debugger-children`, which can have one of two values: `plugin` or
`renderer`.

## Future Work

### Reusing existing testing objects

To avoid writing (and maintaining!) yet another test controller, it is desirable
to reuse an existing test controller. A possible solution would be to change
DRT's test controller to not depend on DRT's implementation of the Blink
objects, but rather on the Blink interfaces. In addition, we would need to
extract an interface from the test shell object that can be implemented by
content shell. This would allow for directly using DRT's test controller in
content shell.
