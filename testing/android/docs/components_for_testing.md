[< Instrumentation](/testing/android/docs/instrumentation.md)

# Creating a component for testing

Creating a component, e.g. a [ContentProvider][1] or [Service][2], for use
in testing can be tricky. This is particularly true when:
 - app code and test code are in separate APKs
 - the test APK instruments the app APK
 - the test Service depends on app code.

This doc explains the pitfalls in creating a test component and how to
avoid them.

[TOC]

## What can go wrong with a test component

> **tl;dr:** test components may not be able to access app code when defined in
> the test APK.

Chromium's instrumentation test suites are all currently set up in the manner
described above: app code is in one APK (the _APK under test_), test code is
in another APK (the _test APK_), and auxiliary code, when necessary, is in
one or more other APKs (_support APKs_). Test APKs build against app code
but do not retain it in their .dex files. This reduces the size of test APKs
and avoids potentially conflicting definitions. At instrumentation runtime,
the test code is loaded into the app package's process, and it's consequently
able to access code defined in the app APK's .dex file(s). Test components,
however, run in the test package's process and only have access to code in the
test APK. While test components referencing app code will build without issue,
they will fail to link at runtime and will consequently not be able to be
instantiated.

For example, here's the logcat from an attempt to use a test Service,
`TestPostMessageService`, that extended an app Service, `PostMessageService`.
Note that the runtime link failed because the superclass couldn't be resolved,
and the test Service could not be instantiated as a result.

``` text
Unable to resolve superclass of Lorg/chromium/chrome/browser/customtabs/TestPostMessageService; (184)
Link of class 'Lorg/chromium/chrome/browser/customtabs/TestPostMessageService;' failed
...
FATAL EXCEPTION: main
Process: org.chromium.chrome.tests, PID: 30023
java.lang.RuntimeException:
    Unable to instantiate service org.chromium.chrome.browser.customtabs.TestPostMessageService:
        java.lang.ClassNotFoundException:
        Didn't find class "org.chromium.chrome.browser.customtabs.TestPostMessageService" on path:
        DexPathList[
            [zip file "/system/framework/android.test.runner.jar",
             zip file "/data/app/org.chromium.chrome.tests-1.apk"],
            nativeLibraryDirectories=[
                /data/app-lib/org.chromium.chrome.tests-1,
                /vendor/lib,
                /system/lib]]
  at android.app.ActivityThread.handleCreateService(ActivityThread.java:2543)
  at android.app.ActivityThread.access$1800(ActivityThread.java:135)
  at android.app.ActivityThread$H.handleMessage(ActivityThread.java:1278)
  at android.os.Handler.dispatchMessage(Handler.java:102)
  at android.os.Looper.loop(Looper.java:136)
  at android.app.ActivityThread.main(ActivityThread.java:5001)
  at java.lang.reflect.Method.invokeNative(Native Method)
  at java.lang.reflect.Method.invoke(Method.java:515)
  at com.android.internal.os.ZygoteInit$MethodAndArgsCaller.run(ZygoteInit.java:785)
  at com.android.internal.os.ZygoteInit.main(ZygoteInit.java:601)
  at dalvik.system.NativeStart.main(Native Method)
Caused by: java.lang.ClassNotFoundException:
    Didn't find class "org.chromium.chrome.browser.customtabs.TestPostMessageService" on path:
    DexPathList[
        [zip file "/system/framework/android.test.runner.jar",
         zip file "/data/app/org.chromium.chrome.tests-1.apk"],
        nativeLibraryDirectories=[
            /data/app-lib/org.chromium.chrome.tests-1,
            /vendor/lib,
            /system/lib]]
  at dalvik.system.BaseDexClassLoader.findClass(BaseDexClassLoader.java:56)
  at java.lang.ClassLoader.loadClass(ClassLoader.java:497)
  at java.lang.ClassLoader.loadClass(ClassLoader.java:457)
  at android.app.ActivityThread.handleCreateService(ActivityThread.java:2540)
  ... 10 more
```

## How to implement a test component

There are (at least) two mechanisms for avoiding the failures described above:
using a support APK or using sharedUserIds.

### Use a support APK

Putting the Service in a support APK lets the build system include all necessary
code in the .dex without fear of conflicting definitions, as nothing in the
support APK runs in the same package or process as the test or app code.

To do this:

**Create the component.**

It should either be in an existing directory used by code in the appropriate
support APK or a new directory for such purpose. In particular, it should be
in neither a directory containing app code nor a directory containing test
code.

``` java
package org.chromium.chrome.test;

import org.chromium.chrome.MyAppService;

public class MyTestService extends MyAppService {
    ...
}
```

**Put the component in a separate gn target.**

This can either be a target upon which the support APK already depends or a new
target.

``` python
android_library("my_test_service") {
  sources = [ "src/org/chromium/chrome/test/MyTestService.java" ]
  deps = [ ... ]
}
```

The support APK must depend on this target. The target containing your test
code can also depend on this target if you want to refer to the component
class directly, e.g., when binding a Service.

> **NOTE:** Even if your test code directly depends on the service target,
> you won't be able to directly reference the test component instance from
> test code. Checking a test component's internal state will require adding
> APIs specifically for that purpose.

**Define the component in the support APK manifest.**

``` xml
<manifest ...
    package="org.chromium.chrome.tests.support" >

  <application>
    <service android:name="org.chromium.chrome.test.MyTestService"
        android:exported="true"
        tools:ignore="ExportedService">
      ...
    </service>

    ...
  </application>
</manifest>
```

### Use a sharedUserId

Using a [sharedUserId][3] will allow your component to run in your app's
process.

> Because this requires modifying the app manifest, it is not recommended at
> this time and is intentionally not further documented here.

[1]: https://developer.android.com/reference/android/content/ContentProvider.html
[2]: https://developer.android.com/reference/android/app/Service.html
[3]: https://developer.android.com/guide/topics/manifest/manifest-element.html#uid
