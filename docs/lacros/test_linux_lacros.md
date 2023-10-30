# Test Lacros using linux-chromeos workflow

## How to run a test?

Lacros gtest unit tests, browser tests
This instructions is helpful if you want to repro/debug a test failure on
bot(linux-lacros-rel, linux-lacros-tester-rel).

1.  .gclient includes chromeos

Make sure your gclient config has ‘target_os=["chromeos"]’.
```
$ cat ../.gclient
solutions = [
  {
    "name": "src",
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {
    },
  },
]
target_os=["chromeos"]
```

Note: don’t forget to run ‘gclient sync’ after you make changes.

2.  Use the correct gn args

Only 2 sets of gn args are officially supported. Some small changes should
likely work but there’s no guarantee. If you are new to this, highly recommend
you do not change any gn args. For non-Googlers, you can ignore the use_goma arg.

CQ gn args:
```
also_build_ash_chrome = true
chromeos_is_browser_only = true
dcheck_always_on = true
is_component_build = false
is_debug = false
symbol_level = 0
target_os = "chromeos"
use_goma = true
```

CI gn args:
```
also_build_ash_chrome = true
chromeos_is_browser_only = true
dcheck_always_on = false
is_component_build = false
is_debug = false
target_os = "chromeos"
use_goma = true
```

3.  Build your target

Here is an example for browser_tests:
```
autoninja -C out_linux_lacros/Release browser_tests
```

4.  Run your test

```
build/lacros/test_runner.py test out_linux_lacros/Release/browser_tests \
--gtest_filter=BrowserTest.Title \
--ash-chrome-path out_linux_lacros/Release/ash_clang_x64/test_ash_chrome
```
Or
```
out_linux_lacros/Release/bin/run_browser_tests --gtest_filter=BrowserTest.Title
```

You can use this to run Chrome tests, such as browser_tests, unit_tests,
interactive_ui_tests, lacros_chrome_browsertests etc.

Note: Some tests are disabled by filter file. e.g. This
[file](https://source.chromium.org/chromium/chromium/src/+/main:testing/buildbot/filters/linux-lacros.browser_tests.filter)
is for browser_tests.

Note: interactive_ui_tests that rely on weston-test cannot run on release builds
(is_official_build must be false). i.e. BrowserActionInteractiveTest*,
MenuItemViewTest*, etc.

Note: If you're sshing to your desktop, please prefix the command with
./testing/xvfb.py.

For frequent Lacros developers:

This can help increase developer productivity. Don’t use this if you’re trying
to repro a bot failure.

1.  Use a prebulit ash chrome

By default, //build/lacros/test_runner.py downloads a prebuilt test_ash_chrome.
If you only change Lacros, this would save your time to not build ash.
```
./build/lacros/test_runner.py test out_linux_lacros/Release/lacros_chrome_browsertests --gtest_filter=ScreenManagerLacrosBrowserTest.*
```

2.  Build linux Ash in a separate folder

Build test_ash_chrome in out_linux_ash and pass in it using –ash-chrome-path.
```
./build/lacros/test_runner.py test \
--ash-chrome-path=out_linux_ash/Release/test_ash_chrome \
out_linux_lacros/Release/lacros_chrome_browsertests \
--gtest_filter=ScreenManagerLacrosBrowserTest.*
```

## Linux version skew testing

If you see a test step name like “lacros_chrome_browsertests_Lacros version skew
testing ash 101.0.4951.1 on Ubuntu-18.04”, this means it’s version skew testing
that “lacros_chrome_browsertests” target is running against a pre-built ash with
version 101.0.4951.13.

There are two ways to run Linux based version skew testing:

1. Use a prebuilt ash (recommended)

First follow the previous section to build your target. Then download ash.
Assuming you want to test against ash 92.0.4515.130.
```
version=92.0.4515.130
cipd auth-login
echo "chromium/testing/linux-ash-chromium/x86_64/ash.zip version:$version" > /tmp/ensure-file.txt
cipd ensure -ensure-file /tmp/ensure-file.txt -root lacros_version_skew_tests_v$version
```

Then you can use
```
./build/lacros/test_runner.py test \
out_linux_lacros_lacros/Release/lacros_chrome_browsertests \
--ash-chrome-path-override=lacros_version_skew_tests_v$version/test_ash_chrome
```
to run the test against that version of ash.

2. Build ash locally

Follow [working with release branches](https://www.chromium.org/developers/how-tos/get-the-code/working-with-release-branches/)
to first build ash test_ash_chrome.

Assuming the target is at /absolute/path/out/ashdesktop/test_ash_chrome. Then
you can pass --ash-chrome-path-override=/absolute/path/out/ashdesktop/test_ash_chrome

If you’re debugging, we suggest you have 2 checkouts, one for (older) ash and
the other for (newer) lacros.

### Ash browser tests require Lacros

Use the following gn args((this is the bot config, other args would likely to
work as well) to build Ash browser tests, and the alternate toolchain for
building Lacros in a subfolder:
```
also_build_lacros_chrome = true
dcheck_always_on = false
ffmpeg_branding = "ChromeOS"
is_component_build = false
is_debug = false
proprietary_codecs = true
target_os = "chromeos"
use_goma = true
```
Run the demo test with:
```
out/ashdesktop/browser_tests --lacros-chrome-path=out/ashdesktop/lacros_clang_x64/test_lacros_chrome --gtest_filter=DemoAshRequiresLacrosTest*
```
Demo test is at
[demo_ash_requires_lacros_browsertest.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/base/chromeos/demo_ash_requires_lacros_browsertest.cc)


## How to write a new test?

### Lacros browser tests

Writing a browser test for Lacros is similar to that on other platforms.

If you need to fake some components in ash, you can add it in
[test_ash_chrome_browser_main_extra_parts.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/base/chromeos/test_ash_chrome_browser_main_extra_parts.cc).

If you need Lacros to control Ash behavior, you can modify
[TestControllerAsh](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/crosapi/test_controller_ash.h?q=TestControllerAsh&ss=chromium%2Fchromium%2Fsrc).

### Ash browser tests require Lacros

See
[demo_ash_requires_lacros_browsertest.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/base/chromeos/demo_ash_requires_lacros_browsertest.cc)
 for how to write/run ash browser
tests. We are using a positive test filter. So if you’re adding a new test to
browser_tests_require_lacros, you need to add your test in the filter file.
