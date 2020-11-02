# Lacros

## Background

Lacros is an architecture project to decouple the Chrome browser from the Chrome
OS window manager and system UI. The name comes from **L**inux **A**nd
**C**h**R**ome **OS**.

Googlers: [go/lacros](http://go/lacros) has internal docs.

## Technical details

On Chrome OS, the system UI (ash window manager, login screen, etc.) and the web
browser are the same binary. Lacros separates this functionality into two
binaries, henceforth known as ash-chrome (system UI) and lacros-chrome (web
browser).

The basic approach is to rename the existing binary to ash-chrome, with minimal
changes. We then take the linux-chrome binary, improve its Wayland support, make
it act like the web browser on Chrome OS, and ship that as the lacros-chrome
binary. This allows the two binaries to be released independently, with some
performance/resource costs. The API boundary initially will be semi-stable: it
will tolerate 1-2 milestones of version skew. We may allow larger amounts of
skew in the future.

Both binaries are built out of the chromium git repository. However, the
binaries might be built at different versions. For example, the version of
lacros built from the M-101 branch might run on top of the ash version built
from the M-100 branch.

Lacros can be imagined as "Linux chrome with more Wayland support". Lacros uses
[ozone](https://chromium.googlesource.com/chromium/src.git/+/master/ui/ozone)
as an abstraction layer for graphics and event handling. Ozone has a "backend"
with client-side support for the Wayland compositor protocol.

Chrome OS has a Wayland server implementation called
[exosphere](https://chromium.googlesource.com/chromium/src.git/+/master/components/exo).
It is used by ARC (to run Android apps) and Crostini (to run Linux apps).

Lacros will use exo as the Wayland server for graphics and event handling. Where
possible we use stable Wayland protocols. We also share Wayland protocol
extensions with ARC and Crostini (e.g.
[zaura-shell](https://chromium.googlesource.com/chromium/src.git/+/master/components/exo/wayland/protocol/aura-shell.xml).
Higher-level features (e.g. file picking) use Mojo IPC.

We call the new Mojo API surface "crosapi". It's similar in concept to Win32 or
Cocoa, but much smaller. It's also mostly asynchronous for performance reasons.
The API lives in
[//chromeos/crosapi](https://chromium.googlesource.com/chromium/src.git/+/master/chromeos/crosapi).
The ash-side implementation lives in
[//chrome/browser/chromeos/crosapi](https://chromium.googlesource.com/chromium/src.git/+/master/chrome/browser/chromeos/crosapi).

Code can be conditionally compiled into lacros via
BUILDFLAG(IS_CHROMEOS_LACROS).

Lacros bugs can be filed under component: OS>LaCrOs.

## GN var and C++ macros for Lacros

### Desired state

- defined(OS_CHROMEOS) is true in C++ for both ash-chrome and lacros-chrome.
- BUILDFLAG(IS_CHROMEOS_ASH) in C++ is used for ash-chrome specific part.
- BUILDFLAG(IS_CHROMEOS_LACROS) in C++ is used for lacros-chrome specific part.
- GN variable is_chromeos is true for both ash-chrome and lacros-chrome.
- GN variable is_chromeos is equivalent to is_chromeos_ash || is_chromeos_lacros.
- GN variable is_chormeos_ash is used for ash-chrome specific part.
- GN variable is_chromeos_lacros is used for lacros-chrome specific part.

### Current state

OS_CHROMEOS is defined and is_chromeos is set only for ash-chrome at the moment.
We are currently migrating defined(OS_CHROMEOS) to BUILDFLAG(IS_CHROMEOS_ASH),
see [crbug.com/1052397](https://crbug.com/1052397). Until the migration is
complete, for parts used by both ash-chrome and lacros-chrome, use
`BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)` in C++ files and
`is_chromeos_ash || is_chromeos_lacros` in GN files. After the migration, the
macros and GN variables should be used according to the above desired state.

Googlers:
- [go/lacros-porting](http://go/lacros-porting) has tips on which binary the code should live in.
- [go/lacros-macros](http://go/lacros-macros) describes the steps for the migration.
- [go/lacros-build-config](http://go/lacros-build-config) is the original design doc.

## Testing

Most test suites require ash-chrome to be running in order to provide a basic
Wayland server. This requires a special test runner:

`./build/lacros/test_runner.py test out/lacros/browser_tests --gtest_filter=BrowserTest.Title`

Some test suites require ash-chrome to provide both a Wayland server and a valid
mojo crosapi connection. This requires the test target
`lacros_chrome_browsertests`:

`./build/lacros/test_runner.py test out/lacros/lacros_chrome_browsertests --gtest_filter=ScreenManagerLacrosBrowserTest.*`

By default, the test runner downloads a prebuilt ash-chrome, add the
`--ash-chrome-path` command line argument to run the test against a locally
built version of Ash:

`./build/lacros/test_runner.py test --ash-chrome-path=out/ash/chrome out/lacros/lacros_chrome_browsertests --gtest_filter=ScreenManagerLacrosBrowserTest.*`

If you're sshing to your desktop, please prefix the command with
`./testing/xvfb.py`.

For sheriffs: Test failures that should have been caught by the CQ should be
treated like test failures on any other platform: investigated and fixed or
disabled. Use BUILDFLAG(IS_LACROS) to disable a test just for lacros. See the
[sheriffing how-to](http://go/chrome-sheriffing-how-to#test-failed).
