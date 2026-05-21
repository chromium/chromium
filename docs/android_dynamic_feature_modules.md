# App Bundles and Dynamic Feature Modules (DFMs)

[TOC]

## About Bundles
[Android App bundles] is a Play Store feature that allows packaging an app as
multiple `.apk` files, known as "splits". Bundles are zip files with an `.aab`
extension. See [android_build_instructions.md#multiple-chrome-targets] for a
list of buildable bundle targets.

Bundles provide three main advantages over monolithic `.apk` files:
1. Language resources are split into language-specific `.apk` files, known as
   "resource splits". Delivering only the active languages reduces the overhead
   of UI strings.
   * Resource splits can also be made on a per-screen-density basis (for drawables),
     but Chrome has not taken advantage of this (yet).
2. Features can be packaged into lazily loaded `.apk` files, known as
   "feature splits". Chrome enables [isolated splits], which means feature
   splits have no performance overhead until used (on Android O+ at least).
3. Feature splits can be downloaded on-demand, saving disk space for users that
   do not need the functionality they provide. These are known as
   "Dynamic feature modules", or "DFMs".
   * **The install experience for DFMs is quite poor (5-30 seconds install times,
     sometimes fails, sometimes [triggers a crash]).**

You can inspect which `.apk` files are produced by a bundle target via:
```
out/Default/bin/${target_name} build-bundle-apks --output-apks foo.apks
unzip -l foo.apks
```

*** note
Adding new features via feature splits is highly encouraged when it makes sense
to do so:
 * Has a non-trivial amount of Java code (after optimization). E.g. >150kb
 * Not needed on startup
 * Has a small integration surface (calls into it must be done with reflection)
 * Not used by WebView
***

[android_build_instructions.md#multiple-chrome-targets]: android_build_instructions.md#multiple-chrome-targets
[Android App Bundles]: https://developer.android.com/guide/app-bundle
[isolated splits]: android_isolated_splits.md
[triggers a crash]: https://chromium.googlesource.com/chromium/src/+/main/docs/android_isolated_splits.md#Conflicting-ClassLoaders-2

### Declaring App Bundles with GN Templates

Here's an example that shows how to declare a simple bundle that contains a
single base module, which enables language-based splits:

```gn
  android_app_bundle_module("foo_base_module") {
    # Declaration are similar to android_apk here.
    ...
  }

  android_app_bundle("foo_bundle") {
    base_module_target = ":foo_base_module"

    # The name of our bundle file (without any suffix).
    bundle_name = "FooBundle"

    # Enable language-based splits for this bundle. Which means that
    # resources and assets specific to a given language will be placed
    # into their own split APK in the final .apks archive.
    enable_language_splits = true

    # Proguard settings must be passed at the bundle, not module, target.
    proguard_enabled = !is_java_debug
  }
```

When generating the `foo_bundle` target with Ninja, you will end up with
the following:

  * The bundle file under `out/Release/apks/FooBundle.aab`

  * A helper script called `out/Release/bin/foo_bundle`, which can be used
    to install / launch / uninstall the bundle on local devices.

    This works like an APK wrapper script (e.g. `foo_apk`). Use `--help`
    to see all possible commands supported by the script.


The remainder of this doc focuses on DFMs.

## Declaring Dynamic Feature Modules (DFMs)

This guide walks you through the steps to create a DFM called _Foo_ and add it
to the Chrome bundles.

*** note
**Note:** To make your own module you'll essentially have to replace every
instance of `foo`/`Foo`/`FOO` with `your_feature_name`/`YourFeatureName`/
`YOUR_FEATURE_NAME`.
***

### Create DFM target

DFMs are APKs. They have a manifest and can contain Java and native code as well
as resources. This section walks you through creating the module target in our
build system.

First, create the file
`//chrome/android/modules/foo/internal/java/AndroidManifest.xml` and add:

```xml
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:dist="http://schemas.android.com/apk/distribution"
    featureSplit="foo">

    <!-- dist:onDemand="true" makes this a separately installed module.
         dist:onDemand="false" would always install the module alongside the
         rest of Chrome. -->
    <dist:module
        dist:onDemand="true"
        dist:title="@string/foo_module_title">
        <!-- This will fuse the module into the base APK if a system image
             APK is built from this bundle. -->
        <dist:fusing dist:include="true" />
    </dist:module>

    <!-- Remove android:hasCode="false" when adding Java code. -->
    <application android:hasCode="false" />
</manifest>
```

Next, create a descriptor configuring the Foo module. To do this, create
`//chrome/android/modules/foo/foo_module.gni` and add the following:

```gn
foo_module_desc = {
  name = "foo"
  android_manifest =
      "//chrome/android/modules/foo/internal/java/AndroidManifest.xml"
}
```

Then, add the module descriptor to the appropriate descriptor list in
//chrome/android/modules/chrome_feature_modules.gni, e.g. the Chrome list:

```gn
import("//chrome/android/modules/foo/foo_module.gni")
...
chrome_module_descs += [ foo_module_desc ]
```

The next step is to add Foo to the list of feature modules for UMA recording.
For this, add `foo` to the `AndroidFeatureModuleName` in
`//tools/metrics/histograms/metadata/histogram_suffixes_list.xml`:

```xml
<histogram_suffixes name="AndroidFeatureModuleName" ...>
  ...
  <suffix name="foo" label="Super Duper Foo Module" />
  ...
</histogram_suffixes>
```

Lastly, give your module a title that Chrome and Play can use for the install
UI. To do this, add a string to
`//chrome/browser/ui/android/strings/android_chrome_strings.grd`:

```xml
...
<message name="IDS_FOO_MODULE_TITLE"
  desc="Text shown when the Foo module is referenced in install start, success,
        failure UI (e.g. in IDS_MODULE_INSTALL_START_TEXT, which will expand to
        'Installing Foo for Chrome…').">
  Foo
</message>
...
```

*** note
**Note:** This is for module title only. Other strings specific to the module
should go in the module, not here (in the base module).
***

Congrats! You added the DFM Foo to Chrome. That is a big step but not very
useful so far. In the next sections you'll learn how to add code and resources
to it.


### Building and installing modules

Before we are going to jump into adding content to Foo, let's take a look on how
to build and deploy the Chrome bundle with the Foo DFM. The remainder of
this guide assumes the environment variable `OUTDIR` is set to a properly
configured GN build directory (e.g. `out/Debug`).

To build and install the Chrome bundle to your connected device, run:

```shell
$ autoninja -C $OUTDIR chrome_public_bundle
$ $OUTDIR/bin/chrome_public_bundle install -m foo
```

This will install the `Foo` module, the `base` module, and all modules with an
`AndroidManifest.xml` that:
 * Sets `<module dist:onDemand="false">`, or
 * Has `<dist:delivery>` conditions that are satisfied by the device being
   installed to.

*** note
**Note:** The install script may install more modules than you specify, e.g.
when there are default or conditionally installed modules (see
[below](#conditional-install) for details).
***

You can then check that the install worked with:

```shell
$ adb shell dumpsys package org.chromium.chrome | grep splits
>   splits=[base, config.en, foo]
```

Then try installing the Chrome bundle without your module and print the
installed modules:

```shell
$ $OUTDIR/bin/chrome_public_bundle install
$ adb shell dumpsys package org.chromium.chrome | grep splits
>   splits=[base, config.en]
```

*** note
The wrapper script's `install` command does approximately:
```sh
java -jar third_party/android_build_tools/bundletool/cipd/bundletool.jar build-apks --output tmp.apks ...
java -jar third_party/android_build_tools/bundletool/cipd/bundletool.jar install-apks --apks tmp.apks
```

The `install-apks` command uses `adb install-multiple` under-the-hood.
***

### Adding Java code

To make Foo useful, let's add some Java code to it. This section will walk you
through the required steps.

First, define a module interface for Foo. This is accomplished by adding the
`@ModuleInterface` annotation to the Foo interface. This annotation
automatically creates a `FooModule` class that can be used later to install and
access the module. To do this, add the following in the new file
`//chrome/browser/foo/android/java/src/org/chromium/chrome/browser/foo/Foo.java`:

```java
package org.chromium.chrome.browser.foo;

import org.chromium.components.module_installer.builder.ModuleInterface;

/** Interface to call into Foo feature. */
@ModuleInterface(module = "foo", impl = "org.chromium.chrome.browser.FooImpl")
public interface Foo {
    /** Magical function. */
    void bar();
}
```

Next, define an implementation that goes into the module in the new file
`//chrome/browser/foo/internal/android/java/src/org/chromium/chrome/browser/foo/FooImpl.java`:

```java
package org.chromium.chrome.browser.foo;

import org.chromium.base.Log;

public class FooImpl implements Foo {
    @Override
    public void bar() {
        Log.i("FOO", "bar in module");
    }
}
```

You can then use this provider to access the module if it is installed. To test
that, instantiate Foo and call `bar()` somewhere in Chrome:

```java
if (FooModule.isInstalled()) {
    FooModule.getImpl().bar();
} else {
    Log.i("FOO", "module not installed");
}
```

The interface has to be available regardless of whether the Foo DFM is present.
Therefore, put those classes into the base module, creating a new public
build target in: `//chrome/browser/foo/BUILD.gn`:

```gn
import("//build/config/android/rules.gni")

android_library("java") {
  sources = [
    "android/java/src/org/chromium/chrome/browser/foo/Foo.java",
  ]
  deps = [
    "//components/module_installer/android:module_installer_java",
    "//components/module_installer/android:module_interface_java",
  ]
  annotation_processor_deps =
    [ "//components/module_installer/android:module_interface_processor" ]
}
```

Then, depend on this target from where it is used as usual. For example, if the
caller is in `chrome_java in //chrome/android/BUILD.gn`:

```gn
...
android_library("chrome_java") {
  deps =[
    ...
    "//chrome/browser/foo:java",
    ...
  ]
}
...
```

The actual implementation, however, should go into the Foo DFM. For this
purpose, create a new file `//chrome/browser/foo/internal/BUILD.gn` and
make a library with the module Java code in it:

```gn
import("//build/config/android/rules.gni")

android_library("java") {
  # Define like ordinary Java Android library.
  sources = [
    "android/java/src/org/chromium/chrome/browser/foo/FooImpl.java",
    # Add other Java classes that should go into the Foo DFM here.
  ]
  deps = [
    "//base:base_java",
    # Put other Chrome libs into the classpath so that you can call into them
    # from the Foo DFM.
    "//chrome/browser/bar:java",
    # The module can depend even on `chrome_java` due to factory magic, but this
    # is discouraged. Consider passing a delegate interface in instead.
    "//chrome/android:chrome_java",
    # Also, you'll need to depend on any //third_party or //components code you
    # are using in the module code.
  ]
}
```

Then, add this new library as a dependency of the Foo module descriptor in
`//chrome/android/modules/foo/foo_module.gni`:

```gn
foo_module_desc = {
  ...
  java_deps = [
    "//chrome/browser/foo/internal:java",
  ]
}
```

Finally, tell Android that your module is now containing code. Do that by
removing the `android:hasCode="false"` attribute from the `<application>` tag in
`//chrome/android/modules/foo/internal/java/AndroidManifest.xml`. You should be
left with an empty tag like so:

```xml
...
    <application />
...
```

Rebuild and install `chrome_public_bundle`. Start Chrome and run through a
flow that tries to executes `bar()`. Depending on whether you installed your
module (`-m foo`) "`bar in module`" or "`module not installed`" is printed to
logcat. Yay!

### Adding pre-built native libraries

You can add a third-party native library (or any standalone library that doesn't
depend on Chrome code) by adding it as a loadable module to the module descriptor in
`//chrome/android/modules/foo/foo_module.gni`:

```gn
foo_module_desc = {
  ...
  loadable_modules_32_bit = [ "//path/to/32/bit/lib.so" ]
  loadable_modules_64_bit = [ "//path/to/64/bit/lib.so" ]
}
```

### Adding Android resources

In this section we will add the required build targets to add Android resources
to the Foo DFM.

First, add a resources target to
`//chrome/browser/foo/internal/BUILD.gn` and add it as a dependency on
Foo's `java` target in the same file:

```gn
...
android_resources("java_resources") {
  # Define like ordinary Android resources target.
  ...
  custom_package = "org.chromium.chrome.browser.foo"
}
...
android_library("java") {
  ...
  deps = [
    ":java_resources",
  ]
}
```

To add strings follow steps
[here](http://dev.chromium.org/developers/design-documents/ui-localization) to
add new Java GRD file. Then create
`//chrome/browser/foo/internal/android/resources/strings/android_foo_strings.grd` as
follows:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<grit current_release="1" latest_public_release="0">
  <outputs>
    <output
        filename="values-am/android_foo_strings.xml"
        lang="am"
        type="android" />
    <!-- List output file for all other supported languages. See
         //chrome/browser/ui/android/strings/android_chrome_strings.grd for the
         full list. -->
    ...
  </outputs>
  <translations>
    <file lang="am" path="vr_translations/android_foo_strings_am.xtb" />
    <!-- Here, too, list XTB files for all other supported languages. -->
    ...
  </translations>
  <release seq="1">
    <messages fallback_to_english="true">
      <message name="IDS_BAR_IMPL_TEXT" desc="Magical string.">
        impl
      </message>
    </messages>
  </release>
</grit>
```

Then, create a new GRD target and add it as a dependency on `java_resources` in
`//chrome/browser/foo/internal/BUILD.gn`:

```gn
...
java_strings_grd("java_strings_grd") {
  defines = chrome_grit_defines
  grd_file = "android/resources/strings/android_foo_strings.grd"
}
...
android_resources("java_resources") {
  ...
  deps = [":java_strings_grd"]
  custom_package = "org.chromium.chrome.browser.foo"
}
...
```

You can then access Foo's resources using the
`org.chromium.chrome.browser.foo.R` class. To do this change
`//chrome/browser/foo/internal/android/java/src/org/chromium/chrome/browser/foo/FooImpl.java`
to:

```java
package org.chromium.chrome.browser.foo;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.foo.R;

public class FooImpl implements Foo {
    @Override
    public void bar() {
        Log.i("FOO", ContextUtils.getApplicationContext().getString(
                R.string.bar_impl_text));
    }
}
```

### Adding non-string native resources

This section describes how to add non-string native resources to Foo DFM.
Key ideas:

* The compiled resource file shipped with the DFM is `foo_resourcess.pak`.
* At run time, native resources need to be loaded before use. Also, DFM native
  resources can only be used from the Browser process.

#### Creating PAK file

Two ways to create `foo_resourcess.pak` (using GRIT) are:

1. (Preferred) Use `foo_resourcess.grd` to refer to individual files (e.g.,
  images, HTML, JS, or CSS) and assigns resource text IDs. `foo_resourcess.pak`
  must have an entry in `/tools/gritsettings/resource_ids.spec`.
1. Combine existing .pak files via `repack` rules in GN build files. This is
  done by the DevUI DFM, which aggregates resources from many DevUI pages.

#### Loading PAK file

At runtime, `foo_resources.pak` needs to be loaded (memory-mapped) before any of
its resource gets used. Alternatives to do this are:

1. (Simplest) Specify native resources (with native libraries if any exist) to
  be automatically loaded on first call to `FooModule.getImpl()`. This behavior
  is specified via `load_native_on_get_impl = true` in `foo_module_desc`.
1. In Java code, call `FooModule.ensureNativeLoaded()`.
1. In C++ code, use JNI to call `FooModule.ensureNativeLoaded()`. The code to do
  this can be placed in a helper class, which can also have JNI calls to
  `FooModule.isInstalled()` and `FooModule.installModule()`.

#### Cautionary notes

Compiling `foo_resources.pak` auto-generates `foo_resources.h`, which defines
textual resource IDs, e.g., `IDR_FOO_HTML`. C++ code then uses these IDs to get
resource bytes. Unfortunately, this behavior is fragile: If `IDR_FOO_HTML` is
accessed before the Foo DFM is (a) installed, or (b) loaded, then runtime error
ensues! Some mitigation strategies are as follows:

* (Ideal) Access Foo DFM's native resources only from code in Foo DFM's native
  libraries. So by the time that `IDR_FOO_HTML` is accessed, everything is
  already in place! This isn't always possible; henceforth we assume that
  `IDR_FOO_HTML` is accessed by code in the base DFM.
* Before accessing IDR_FOO_HTML, ensure Foo DFM is installed and loaded. The
  latter can use `FooModule.ensureNativeLoaded()` (needs to be called from
  Browser thread).
* Use inclusion of `foo_resources.h` to restrict availability of `IDR_FOO_HTML`.
  Only C++ files dedicated to "DFM-gated code" (code that runs only when its DFM
  is installed and loaded) should include `foo_resources.h`.

#### Associating native resources with DFM

Here are the main GN changes to specify PAK files and default loading behavior
for a DFM's native resources:

```gn
foo_module_desc = {
  ...
  paks = [ "$root_gen_dir/chrome/browser/foo/internal/foo_resourcess.pak" ]
  pak_deps = [ "//chrome/browser/foo/internal:foo_paks" ]
  load_native_on_get_impl = true
}
```


### Module install

So far, we have installed the Foo DFM as a true split (`-m foo` option on the
install script). In production, however, we have to explicitly install the Foo
DFM for users to get it. There are three install options: _on-demand_,
_deferred_ and _conditional_.

#### On-demand install

On-demand requesting a module will try to download and install the
module as soon as possible regardless of whether the user is on a metered
connection or whether they have turned updates off in the Play Store app.

You can use the autogenerated module class to on-demand install the module like
so:

```java
FooModule.install((success) -> {
    if (success) {
        FooModule.getImpl().bar();
    }
});
```

**Optionally**, you can show UI telling the user about the install flow. For
this, add a function like the one below. Note, it is possible
to only show either one of the  install, failure and success UI or any
combination of the three.

```java
public static void installModuleWithUi(
        Tab tab, OnModuleInstallFinishedListener onFinishedListener) {
    ModuleInstallUi ui =
            new ModuleInstallUi(
                    tab,
                    R.string.foo_module_title,
                    new ModuleInstallUi.FailureUiListener() {
                        @Override
                        public void onFailureUiResponse(retry) {
                            if (retry) {
                                installModuleWithUi(tab, onFinishedListener);
                            } else {
                                onFinishedListener.onFinished(false);
                            }
                        }
                    });
    // At the time of writing, shows toast informing user about install start.
    ui.showInstallStartUi();
    FooModule.install(
            (success) -> {
                if (!success) {
                    // At the time of writing, shows infobar allowing user
                    // to retry install.
                    ui.showInstallFailureUi();
                    return;
                }
                // At the time of writing, shows toast informing user about
                // install success.
                ui.showInstallSuccessUi();
                onFinishedListener.onFinished(true);
            });
}
```

To test on-demand install, "fake-install" the DFM. It's fake because
the DFM is not installed as a true split. Instead it will be emulated by play
core's `--local-testing` [mode][play-core-local-testing].
Fake-install and launch Chrome with the following command:

```shell
$ $OUTDIR/bin/chrome_public_bundle install -f foo
$ $OUTDIR/bin/chrome_public_bundle launch
```

When running the install code, the Foo DFM module will be emulated.
This will be the case in production right after installing the module. Emulation
will last until Play Store has a chance to install your module as a true split.
This usually takes about a day. After it has been installed, it will be updated
atomically alongside Chrome. Always check that it is installed and available
before invoking code within the DFM.

*** note
**Warning:** There are subtle differences between emulating a module and
installing it as a true split. We therefore recommend that you always test both
install methods.
***

*** note
To simplify development, the DevUI DFM (dev_ui) is installed by default, i.e.,
`-m dev_ui` is implied by default. This is overridden by:
* `--no-module dev_ui`, to test error from missing DevUI,
* `-f dev_ui`, for fake module install.
***

#### Deferred install

Deferred install means that the DFM is installed in the background when the
device is on an unmetered connection and charging. The DFM will only be
available after Chrome restarts. When deferred installing a module it will
not be faked installed.

To defer install Foo do the following:

```java
FooModule.installDeferred();
```

#### Conditional install

Conditional install means the DFM will be installed automatically upon first
installing or updating Chrome if the device supports a particular feature.
Conditional install is configured in the module's manifest. To install your
module on all Daydream-ready devices for instance, your
`//chrome/android/modules/foo/internal/java/AndroidManifest.xml` should look
like this:

```xml
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:dist="http://schemas.android.com/apk/distribution"
    featureSplit="foo">

    <dist:module
      dist:instant="false"
      dist:title="@string/foo_module_title">
      <dist:fusing dist:include="true" />
      <dist:delivery>
        <dist:install-time>
          <dist:conditions>
            <dist:device-feature
              dist:name="android.hardware.vr.high_performance" />
          </dist:conditions>
        </dist:install-time>
        <!-- Allows on-demand or deferred install on non-Daydream-ready
             devices. -->
        <dist:on-demand />
      </dist:delivery>
    </dist:module>

    <application />
</manifest>
```

You can also specify no conditions to have your module always installed.
You might want to do this in order to delay the performance implications
of loading your module until its first use (true only on Android O+ where
[android:isolatedSplits](https://developer.android.com/reference/android/R.attr#isolatedSplits)
is supported. See [go/isolated-splits-dev-guide](http://go/isolated-splits-dev-guide)
(googlers only).

### chrome_public_apk and Integration Tests

To make the Foo feature available in the non-bundle `chrome_public_apk`
target, add the `java` target to the template in
`//chrome/android/chrome_public_apk_tmpl.gni` like so:

```gn
  # Add to where "chrome_all_java" is added:
  if (!_is_bundle) {
    deps += [ "//chrome/browser/foo/internal:java" ]
  }
}
```

You may also have to add `java` as a dependency of
`//chrome/android/javatests/chrome_test_java_org.chromium.chrome.browser.foo`
if you want to call into Foo from test code.

[play-core-local-testing]: https://developer.android.com/guide/playcore/feature-delivery/on-demand#local-testing
