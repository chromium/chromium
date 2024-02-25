# Android Debugging Instructions
Chrome on Android has java and c/c++ code. Each "side" have its own set of tools
for debugging. Here's some tips.

[TOC]

## Instructions for Google Employees

See also
[go/clankium/06-debugging-clank](https://goto.google.com/clankium/06-debugging-clank).

## Launching
You can run the app by using one of the wrappers.

```shell
# Installs, launches, and enters logcat.
out/Default/bin/content_shell_apk run --args='--disable-fre' 'data:text/html;utf-8,<html>Hello World!</html>'
# Launches without first installing. Does not show logcat.
out/Default/bin/chrome_public_apk launch --args='--disable-fre' 'data:text/html;utf-8,<html>Hello World!</html>'
```

## Logging
[Chromium logging from LOG(INFO)](https://chromium.googlesource.com/chromium/src/+/main/docs/android_logging.md)
etc., is directed to the Android logcat logging facility. You can filter the
messages, e.g. view chromium verbose logging, everything else at warning level
with:

```shell
# Shows a coloured & filtered logcat.
out/Default/bin/chrome_public_apk logcat [-v]  # Use -v to show logs for other processes
```

If this doesn't display the logs you're looking for, try `adb logcat` with your system `adb`
or the one in `//third_party/android_sdk/`.

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
    * If this fails, you likely need to follow [these instructions](https://stackoverflow.com/questions/21114066/attach-intellij-idea-debugger-to-a-running-java-process).

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
While the app is running, use the wrapper script's `lldb` command to enter into a
lldb shell.

When running with `lldb` attached, the app runs **extremely slowly**.

```shell
# Attaches to browser process.
out/Default/bin/content_shell_apk lldb
out/Default/bin/chrome_public_apk lldb

# Attaches to gpu process.
out/Default/bin/chrome_public_apk lldb --debug-process-name privileged_process0

# Attach to other processes ("chrome_public_apk ps" to show pids).
out/Default/bin/chrome_public_apk lldb --pid $PID
```

### Using Visual Studio Code

**NOT WORKING**

This used to work with GDB, but the LLDB instructions have not been written. If
you would like to take this on, please use:
[crbug/1266055](https://bugs.chromium.org/p/chromium/issues/detail?id=1266055).

### Waiting for Debugger on Early Startup
```shell
# Install, launch, and wait:
out/Default/bin/chrome_public_apk run --args="--wait-for-debugger"
# Launch, and have GPU process wait rather than Browser process:
out/Default/bin/chrome_public_apk launch --args="--wait-for-debugger-children=gpu-process"
# Or for renderers:
out/Default/bin/chrome_public_apk launch --args="--wait-for-debugger-children=renderer"
```

#### With Command-line LLDB
Once attached, `lldb` will drop into a prompt. Set your breakpoints and run "c" to
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
   `tools/luci-go/isolated` to download the mapping file if it's too big. The
   viewer will provide instructions for this.

**Googlers Only**: For official build mapping files, see
[go/chromejavadeobfuscation](https://goto.google.com/chromejavadeobfuscation).

Once you have a .mapping file:

```shell
# For a file:
build/android/stacktrace/java_deobfuscate.py PROGUARD_MAPPING_FILE.mapping < FILE
# For logcat:
adb logcat | build/android/stacktrace/java_deobfuscate.py PROGUARD_MAPPING_FILE.mapping
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

## Debug unit tests with LLDB

To run unit tests use the following command:

```shell
out/Debug/bin/run_test_name -f <test_filter_if_any> --wait-for-debugger -t 6000
```

That command will cause the test process to wait until a debugger is attached.

To attach a debugger:

```shell
build/android/connect_lldb.sh --output-directory=out/Default --package-name=org.chromium.native_test
```

## Examine app data on a non-rooted device

If you're developing on a non-rooted device such as a retail phone, security restrictions
will prevent directly accessing the application's data. However, as long as the app is
built with debugging enabled, you can use `adb shell run-as PACKAGENAME` to execute
shell commands using the app's authorization, roughly equivalent to `su $user`.

Non-Play-Store builds with `is_official_build=false` will by default set
`android:debuggable="true"` in the app's manifest to allow debugging.

For exammple, for a Chromium build, run the following:

```
adb shell run-as org.chromium.chrome
```

If successful, this will silently wait for input without printing anything.
It acts as a simple shell despite not showing the usual `$ ` shell prompt.
Just type commands and press RETURN to execute them.

The starting directory is the app's user data directory where user preferences and other
profile data are stored.

```
pwd
/data/user/0/org.chromium.chrome

find -type f
./files/rList
./shared_prefs/org.chromium.chrome_preferences.xml
```

If you need to access the app's application data directory, you need to look up the
obfuscated installation path since you don't have read access to the */data/app/* directory.
For example:

```
pm list packages -f org.chromium.chrome
package:/data/app/~~ybTygSP5u72F9GN-3TMKXA==/org.chromium.chrome-zYY5mcB7YgB5pa3vfS3CBQ==/base.apk=org.chromium.chrome

ls -l /data/app/~~ybTygSP5u72F9GN-3TMKXA==/org.chromium.chrome-zYY5mcB7YgB5pa3vfS3CBQ==/
total 389079
-rw-r--r-- 1 system system 369634375 2022-11-05 01:49 base.apk
drwxr-xr-x 3 system system      3452 2022-11-05 01:49 lib
-rw-r--r-- 1 system system    786666 2022-11-05 01:49 split_cablev2_authenticator.apk
-rw-r--r-- 1 system system  21258500 2022-11-05 01:49 split_chrome.apk
-rw-r--r-- 1 system system   1298934 2022-11-05 01:49 split_config.en.apk
-rw-r--r-- 1 system system    413913 2022-11-05 01:49 split_dev_ui.apk
-rw-r--r-- 1 system system     12432 2022-11-05 01:49 split_weblayer.apk
```
