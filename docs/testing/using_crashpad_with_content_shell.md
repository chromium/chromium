# Using crashpad with content shell

When running web tests, it is possible to use
[crashpad](../third_party/crashpad/)/[breakpad](../../third_party/breakpad/) to
capture stack traces on crashes while running without a debugger attached and
with the sandbox enabled.

## Setup

On all platforms, build the target `blink_tests`.

*** note
**Mac:** Add `enable_dsyms = 1` to your [gn build
arguments](https://gn.googlesource.com/gn/+/main/docs/quick_start.md) before
building. This slows down linking several minutes, so don't just always set it
by default.
***

Then, create a directory where the crash dumps will be stored:

* Linux/Mac:
  ```bash
  mkdir /tmp/crashes
  ```
* Android:
  ```bash
  adb root
  adb shell mkdir /data/data/org.chromium.content_shell_apk/cache
  ```
* Windows:
  ```bash
  mkdir %TEMP%\crashes
  ```

## Running content shell with crashpad

Crashpad can be enabled by passing `--enable-crash-reporter` and
`--crash-dumps-dir` to content shell:

* Linux:
  ```bash
  out/Debug/content_shell --enable-crash-reporter \
      --crash-dumps-dir=/tmp/crashes chrome://crash
  ```
* Mac:
  ```bash
  out/Debug/Content\ Shell.app/Contents/MacOS/Content\ Shell \
      --enable-crash-reporter --crash-dumps-dir=/tmp/crashes chrome://crash
  ```
* Windows:
  ```bash
  out\Default\content_shell.exe --enable-crash-reporter ^
      --crash-dumps-dir=%TEMP%\crashes chrome://crash
  ```
* Android:
  ```bash
  out/Default/bin/content_shell_apk install
  out/Default/bin/content_shell_apk launch chrome://crash
  --args="--enable-crash-reporter --crash-dumps-dir=/data/data/org.chromium.content_shell_apk/cache"
  ```

## Retrieving the crash dump

On Android, we first have to retrieve the crash dump. On other platforms, this
step can be skipped.

* Android:
  ```bash
  adb pull $(adb shell ls /data/data/org.chromium.content_shell_apk/cache/pending/*.dmp) /tmp/chromium-renderer-minidump.dmp
  ```

## Symbolizing the crash dump

On all platforms except for Windows, we need to convert the debug symbols to a
format that breakpad can understand.

* Linux:
  ```bash
  components/crash/content/tools/generate_breakpad_symbols.py \
      --build-dir=out/Default --binary=out/Default/content_shell \
      --symbols-dir=out/Default/content_shell.breakpad.syms --clear --jobs=16
  ```
* Mac:
  ```bash
  components/crash/content/tools/generate_breakpad_symbols.py \
      --build-dir=out/Default \
      --binary=out/Default/Content\ Shell.app/Contents/MacOS/Content\ Shell \
      --symbols-dir=out/Default/content_shell.breakpad.syms --clear --jobs=16
  ```
* Android:
  ```bash
  components/crash/content/tools/generate_breakpad_symbols.py \
      --build-dir=out/Default \
      --binary=out/Default/lib/libcontent_shell_content_view.so \
      --symbols-dir=out/Default/content_shell.breakpad.syms --clear \
      --platform=android
  ```

Now we can generate a stack trace from the crash dump. Assuming the crash dump
is in minidump.dmp:

* Linux/Android/Mac:
  ```bash
  out/Default/minidump_stackwalk minidump.dmp out/Debug/content_shell.breakpad.syms
  ```
* Windows:
  ```bash
  "c:\Program Files (x86)\Windows Kits\8.0\Debuggers\x64\cdb.exe" ^
      -y out\Default -c ".ecxr;k30;q" -z minidump.dmp
  ```
