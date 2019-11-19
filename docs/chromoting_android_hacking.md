# Chromoting Android Hacking

This guide, which is meant to accompany the
[compilation guide](old_chromoting_build_instructions.md), explains the process of
viewing the logs and debugging the CRD Android client. I'll assume you've
already built the APK as described in the aforementioned guide, that you're in
the `src/` directory, and that your binary is at
`out/Release/apks/Chromoting.apk`. Additionally, you should have installed the
app on at least one (still) connected device.

[TOC]

## Viewing logging output

In order to access LogCat and view the app's logging output, we need to launch
the Android Device Monitor. Run `third_party/android_sdk/public/tools/monitor`
and select the desired device under `Devices`. Using the app as normal will
display log messages to the `LogCat` pane.

## Attaching debuggers for Java code

### Eclipse

1.  Go to https://developer.android.com/sdk/index.html and click "Download the
    SDK ADT Bundle for Linux"
1.  Configure eclipse
    1.  Select General > Workspace from the tree on the left.
        1.  Uncheck Refresh workspace on startup (if present)
        1.  Uncheck Refresh using native hooks or polling (if present)
        1.  Disable build before launching
    1.  Select Run/Debug > Launching
        1.  Uncheck Build (if required) before launching
1. Create a project
    1.  Select File > New > Project... from the main menu.
    1.  Choose Java/Java Project
    1.  Eclipse should have generated .project and perhaps a .classpath file in
        your <project root>/src/ directory.
    1.  Replace/Add the .classpath file with the content from Below. Remember
        that the path field should be the location of the chromium source
        relative to the current directory of your project.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<classpath>
<classpathentry kind="src" path="net/test/android/javatests/src"/>
<classpathentry kind="src" path="net/android/java/src"/>
<classpathentry kind="src" path="net/android/javatests/src"/>
<classpathentry kind="src" path="base/test/android/java/src"/>
<classpathentry kind="src" path="base/test/android/javatests/src"/>
<classpathentry kind="src" path="base/android/jni_generator/java/src"/>
<classpathentry kind="src" path="base/android/java/src"/>
<classpathentry kind="src" path="base/android/javatests/src"/>
<classpathentry kind="src" path="components/cronet/android/java/src"/>
<classpathentry kind="src" path="components/cronet/android/sample/src"/>
<classpathentry kind="src" path="components/cronet/android/sample/javatests/src"/>
<classpathentry kind="src" path="components/autofill/core/browser/android/java/src"/>
<classpathentry kind="src" path="components/embedder_support/android/java/src"/>
<classpathentry kind="src" path="components/dom_distiller/android/java/src"/>
<classpathentry kind="src" path="components/navigation_interception/android/java/src"/>
<classpathentry kind="src" path="ui/android/java/src"/>
<classpathentry kind="src" path="media/base/android/java/src"/>
<classpathentry kind="src" path="chrome/test/android/unit_tests_apk/src"/>
<classpathentry kind="src" path="chrome/test/android/javatests/src"/>
<classpathentry kind="src" path="chrome/test/chromedriver/test/webview_shell/java/src"/>
<classpathentry kind="src" path="chrome/common/extensions/docs/examples/extensions/irc/servlet/src"/>
<classpathentry kind="src" path="chrome/android/java/src"/>
<classpathentry kind="src" path="chrome/android/uiautomator_tests/src"/>
<classpathentry kind="src" path="chrome/android/javatests/src"/>
<classpathentry kind="src" path="sync/test/android/javatests/src"/>
<classpathentry kind="src" path="sync/android/java/src"/>
<classpathentry kind="src" path="sync/android/javatests/src"/>
<classpathentry kind="src" path="mojo/public/java/base/src"/>
<classpathentry kind="src" path="mojo/public/java/bindings/src"/>
<classpathentry kind="src" path="mojo/public/java/system/javatests/src"/>
<classpathentry kind="src" path="mojo/public/java/system/src"/>
<classpathentry kind="src" path="testing/android/java/src"/>
<classpathentry kind="src" path="printing/android/java/src"/>
<classpathentry kind="src" path="tools/binary_size/java/src"/>
<classpathentry kind="src" path="tools/android/memconsumer/java/src"/>
<classpathentry kind="src" path="remoting/android/java/src"/>
<classpathentry kind="src" path="remoting/android/apk/src"/>
<classpathentry kind="src" path="remoting/android/javatests/src"/>
<classpathentry kind="src" path="third_party/webrtc/voice_engine/test/android/android_test/src"/>
<classpathentry kind="src" path="third_party/webrtc/modules/video_capture/android/java/src"/>
<classpathentry kind="src" path="third_party/webrtc/modules/video_render/android/java/src"/>
<classpathentry kind="src" path="third_party/webrtc/modules/audio_device/test/android/audio_device_android_test/src"/>
<classpathentry kind="src" path="third_party/webrtc/modules/audio_device/android/java/src"/>
<classpathentry kind="src" path="third_party/webrtc/examples/android/media_demo/src"/>
<classpathentry kind="src" path="third_party/webrtc/examples/android/opensl_loopback/src"/>
<classpathentry kind="src" path="third_party/webrtc/video_engine/test/auto_test/android/src"/>
<classpathentry kind="src" path="third_party/libjingle/source/talk/app/webrtc/java/src"/>
<classpathentry kind="src" path="third_party/libjingle/source/talk/app/webrtc/javatests/src"/>
<classpathentry kind="src" path="third_party/libjingle/source/talk/examples/android/src"/>
<classpathentry kind="src" path="android_webview/java/src"/>
<classpathentry kind="src" path="android_webview/test/shell/src"/>
<classpathentry kind="src" path="android_webview/unittestjava/src"/>
<classpathentry kind="src" path="android_webview/javatests/src"/>
<classpathentry kind="src" path="chrome/test/android/browsertests_apk/src"/>
<classpathentry kind="src" path="components/test/android/browsertests_apk/src"/>
<classpathentry kind="src" path="content/public/test/android/javatests/src"/>
<classpathentry kind="src" path="content/public/android/java/src"/>
<classpathentry kind="src" path="content/public/android/javatests/src"/>
<classpathentry kind="src" path="content/shell/android/browsertests_apk/src"/>
<classpathentry kind="src" path="content/shell/android/java/src"/>
<classpathentry kind="src" path="content/shell/android/shell_apk/src"/>
<classpathentry kind="src" path="content/shell/android/javatests/src"/>
<classpathentry kind="src" path="content/shell/android/linker_test_apk/src"/>
<classpathentry kind="lib" path="third_party/android_sdk/public/platforms/android-27/data/layoutlib.jar"/>
<classpathentry kind="lib" path="third_party/android_sdk/public/platforms/android-27/android.jar"/>
<classpathentry kind="output" path="out/bin"/>
</classpath>
```

1.  Obtain the debug port
    1.  Go to Window > Open Perspective > DDMS
    1.  In order for the app org.chromium.chromoting to show up, you must build
        Debug instead of Retail
    1.  Note down the port number, should be 8600 or 8700
1.  Setup a debug configuration
    1.  Go to Window > Open Perspective > Debug
    1.  Run > Debug > Configurations
    1.  Select "Remote Java Application" and click "New"
    1.  Put Host: localhost and Port: <the port from DDMS>
    1.  Hit Debug
1.  Configure source path
    1.  Right click on the Chromoting [Application](Remoting.md) and select Edit
        source Lookup Path
    1.  Click "Add" and select File System Directory
    1.  Select the location of your chromium checkout,
        e.g. <project root>/src/remoting/android
1.  Debugging
    1.  To add a breakpoint, simply open the source file and hit Ctrl+Shift+B to
        toggle the breakpoint. Happy hacking.

### Command line debugger

With the Android Device Monitor open, look under `Devices`, expand the entry for
the device on which you want to debug, and select the entry for
`org.chromium.chromoting` (it must already be running). This forwards the JVM
debugging connection to your local port 8700.  In your shell, do `$ jdb -attach
localhost:8700`.

## Attaching GDB to debug native code

The Chromium build system provides a convenience wrapper script that can be used
to easily launch GDB. Run

```shell
$ build/android/adb_gdb --package-name=org.chromium.chromoting \
--activity=.Chromoting --start
```

Note that if you have multiple devices connected, you must export
`ANDROID_SERIAL` to select one; set it to the serial number of the desired
device as output by `$ adb devices`.
