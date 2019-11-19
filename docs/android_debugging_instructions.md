# Android Debugging Instructions
Chrome on Android has java and c/c++ code. Each "side" have its own set of tools
for debugging. Here's some tips.

[TOC]

## Launching
You can run the app by using one of the wrappers.

```shell
# Installs, launches, and enters logcat.
out/Default/bin/content_shell_apk run --args='--disable-fre' 'data:text/html;utf-8,<html>Hello World!</html>'
# Launches without first installing. Does not show logcat.
out/Default/bin/chrome_public_apk launch --args='--disable-fre' 'data:text/html;utf-8,<html>Hello World!</html>'
```

## Logging
[Chromium logging from LOG(INFO)](https://chromium.googlesource.com/chromium/src/+/master/docs/android_logging.md)
etc., is directed to the Android logcat logging facility. You can filter the
messages, e.g. view chromium verbose logging, everything else at warning level
with:

```shell
# Shows a coloured & filtered logcat.
out/Default/bin/chrome_public_apk logcat [-v]  # Use -v to show logs for other processes
```

### Warnings for Blink developers
*   **Do not use fprintf or printf debugging!** This does not
    redirect to logcat.

*   Redirecting stdio to logcat, as documented
    [here](https://developer.android.com/studio/command-line/logcat.html#viewingStd),
    has a bad side-effect that it breaks `adb_install.py`. See
    [here for details](http://stackoverflow.com/questions/28539676/android-adb-fails-to-install-apk-to-nexus-5-on-windows-8-1).

## Take a Screenshot
```shell
build/android/screenshot.py /tmp/screenshot.png
```

## Inspecting the View Hierarchy
Generate an [Android Studio](android_studio.md) project, and then use
[Layout Inspector](https://developer.android.com/studio/debug/layout-inspector).

## Debugging Java
For both apk and test targets, pass `--wait-for-java-debugger` to the wrapper
scripts.

Examples:

```shell
# Install, launch, and wait:
out/Default/bin/chrome_public_apk run --wait-for-java-debugger

# Launch, and have GPU process wait rather than Browser process:
out/Default/bin/chrome_public_apk launch --wait-for-java-debugger --debug-process-name privileged_process0

# Have Renderers wait:
out/Default/bin/chrome_public_apk launch --args="--renderer-wait-for-java-debugger"

# Have tests wait:
out/Default/bin/run_chrome_public_test_apk --wait-for-java-debugger
out/Default/bin/run_chrome_junit_tests --wait-for-java-debugger  # Specify custom port via --debug-socket=9999
```

### Android Studio
*   Open Android Studio ([instructions](android_studio.md))
*   Click "Run"->"Attach debugger to Android process" (see
[here](https://developer.android.com/studio/debug/index.html) for more).
*   Click "Run"->"Attach to Local Process..." for Robolectric junit tests.

### Eclipse
*   In Eclipse, make a debug configuration of type "Remote Java Application".
    Choose a "Name" and set "Port" to `8700`.

*   Make sure Eclipse Preferences > Run/Debug > Launching > "Build (if required)
    before launching" is unchecked.

*   Run Android Device Monitor:

    ```shell
    third_party/android_sdk/public/tools/monitor
    ```

*   Now select the process you want to debug in Device Monitor (the port column
    should now mention 8700 or xxxx/8700).

*   Run your debug configuration, and switch to the Debug perspective.

## Debugging C/C++
While the app is running, use the wrapper script's `gdb` command to enter into a
gdb shell.

When running with gdb attached, the app runs **extremely slowly**.

```shell
# Attaches to browser process.
out/Default/bin/content_shell_apk gdb
out/Default/bin/chrome_public_apk gdb

# Attaches to gpu process.
out/Default/bin/chrome_public_apk gdb --debug-process-name privileged_process0

# Attach to other processes ("chrome_public_apk ps" to show pids).
out/Default/bin/chrome_public_apk gdb --pid $PID
```

When connecting, gdb will complain of not being able to load a lot of libraries.
This happens because of java code. The following messages are all expected:
```
Connecting to :5039...
warning: Could not load shared library symbols for 211 libraries, e.g. /system/framework/arm/boot.oat.
Use the "info sharedlibrary" command to see the complete listing.
Do you need "set solib-search-path" or "set sysroot"?
Failed to read a valid object file image from memory.
```

If you have ever run an ASAN build of chromium on the device, you may get
an error like the following when you start up gdb:
```
/tmp/<username>-adb-gdb-tmp-<pid>/gdb.init:11: Error in sourced command file:
"/tmp/<username>-adb-gdb-tmp-<pid>/app_process32": not in executable format: file format not recognized
```
If this happens, run the following command and try again:
```shell
$ src/android/asan/third_party/asan_device_setup.sh --revert
```

### Using Visual Studio Code
While the app is running, run the `gdb` command with `--ide`:

```shell
out/Default/bin/content_shell_apk gdb --ide
```

Once the script has done its thing (generally ~1 second after the initial
time its used), open [vscode.md](vscode.md) and ensure you have the
[Android launch entry](vscode.md#Launch-Commands).

Connect via the IDE's launch entry. Connecting takes 30-40 seconds.

When troubleshooting, it's helpful to enable
[engine logging](https://github.com/Microsoft/vscode-cpptools/blob/master/launch.md#enginelogging).

Known Issues:
 * Pretty printers are not working properly.

### Waiting for Debugger on Early Startup
```shell
# Install, launch, and wait:
out/Default/bin/chrome_public_apk run --args="--wait-for-debugger"
# Launch, and have GPU process wait rather than Browser process:
out/Default/bin/chrome_public_apk launch --args="--wait-for-debugger-children=gpu-process"
# Or for renderers:
out/Default/bin/chrome_public_apk launch --args="--wait-for-debugger-children=renderer"
```

#### With an IDE
Once `gdb` attaches, the app will resume execution, so you must set your
breakpoint before attaching.

#### With Command-line GDB
Once attached, gdb will drop into a prompt. Set your breakpoints and run "c" to
continue.

## Symbolizing Crash Stacks and Tombstones (C++)

If a crash has generated a tombstone in your device, use:

```shell
build/android/tombstones.py --output-directory out/Default
```

If you have a stack trace (from `adb logcat`) that needs to be symbolized, copy
it into a text file and symbolize with the following command (run from
`${CHROME_SRC}`):

```shell
third_party/android_platform/development/scripts/stack --output-directory out/Default [tombstone file | dump file]
```

`stack` can also take its input from `stdin`:

```shell
adb logcat -d | third_party/android_platform/development/scripts/stack --output-directory out/Default
```

Example:

```shell
third_party/android_platform/development/scripts/stack --output-directory out/Default ~/crashlogs/tombstone_07-build231.txt
```

## Deobfuscating Stack Traces (Java)

You will need the ProGuard mapping file that was generated when the application
that crashed was built. When building locally, these are found in:

```shell
out/Default/apks/ChromePublic.apk.mapping
out/Default/apks/ChromeModernPublic.apk.mapping
etc.
```

When debugging a failing test on the build waterfall, you can find the mapping
file as follows:

1. Open buildbot page for the failing build (e.g.,
   https://ci.chromium.org/p/chrome/builders/ci/android-go-perf/1234).
2. Open the swarming page for the failing shard (e.g., shard #3).
3. Click on "Isolated Inputs" to locate the files the shard used to run the
   test.
4. Download the `.mapping` file for the APK used by the test (e.g.,
   `ChromePublic.apk.mapping`). Note that you may need to use the
   `tools/swarming_client/isolateserver.py` script to download the mapping
   file if it's too big. The viewer will provide instructions for this.

**Googlers Only**: For official build mapping files, see
[go/chromejavadeobfuscation](https://goto.google.com/chromejavadeobfuscation).

Once you have a .mapping file, build the `java_deobfuscate` tool:

```shell
ninja -C out/Default java_deobfuscate
```

Then run it via:

```shell
# For a file:
out/Default/bin/java_deobfuscate PROGUARD_MAPPING_FILE.mapping < FILE
# For logcat:
adb logcat | out/Default/bin/java_deobfuscate PROGUARD_MAPPING_FILE.mapping
```

## Get WebKit code to output to the adb log

In your build environment:

```shell
adb root
adb shell stop
adb shell setprop log.redirect-stdio true
adb shell start
```

In the source itself, use `fprintf(stderr, "message");` whenever you need to
output a message.

## Debug unit tests with GDB

To run unit tests use the following command:

```shell
out/Debug/bin/run_test_name -f <test_filter_if_any> --wait-for-debugger -t 6000
```

That command will cause the test process to wait until a debugger is attached.

To attach a debugger:

```shell
build/android/adb_gdb --output-directory=out/Default --package-name=org.chromium.native_test
```

After attaching gdb to the process you can use it normally. For example:

```
(gdb) break main
Breakpoint 1 at 0x9750793c: main. (2 locations)
(gdb) continue
```
