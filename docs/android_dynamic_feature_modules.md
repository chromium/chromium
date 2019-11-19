# Dynamic Feature Modules (DFMs)

[Android App bundles and Dynamic Feature Modules (DFMs)](https://developer.android.com/guide/app-bundle)
is a Play Store feature that allows delivering pieces of an app when they are
needed rather than at install time. We use DFMs to modularize Chrome and make
Chrome's install size smaller.

[TOC]


## Limitations

DFMs have the following limitations:

* **WebView:** We don't support DFMs for WebView. If your feature is used by
  WebView you cannot put it into a DFM.
* **Android K:** DFMs are based on split APKs, a feature introduced in Android
  L. Therefore, we don't support DFMs on Android K. As a workaround
  you can add your feature to the Android K APK build. See below for details.

## Getting started

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
`//chrome/android/features/foo/internal/java/AndroidManifest.xml` and add:

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
`//chrome/android/features/foo/foo_module.gni` and add the following:

```gn
foo_module_desc = {
  name = "foo"
  android_manifest =
      "//chrome/android/features/foo/internal/java/AndroidManifest.xml"
}
```

Then, add the module descriptor to the appropriate descriptor list in
//chrome/android/modules/chrome_feature_modules.gni, e.g. the Chrome Modern
list:

```gn
import("//chrome/android/features/foo/foo_module.gni")
...
chrome_modern_module_descs += [ foo_module_desc ]
```

The next step is to add Foo to the list of feature modules for UMA recording.
For this, add `foo` to the `AndroidFeatureModuleName` in
`//tools/metrics/histograms/histograms.xml`:

```xml
<histogram_suffixes name="AndroidFeatureModuleName" ...>
  ...
  <suffix name="foo" label="Super Duper Foo Module" />
  ...
</histogram_suffixes>
```

<!--- TODO(tiborg): Add info about install UI. -->
Lastly, give your module a title that Chrome and Play can use for the install
UI. To do this, add a string to
`//chrome/android/java/strings/android_chrome_strings.grd`:

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

Congrats! You added the DFM Foo to Monochrome. That is a big step but not very
useful so far. In the next sections you'll learn how to add code and resources
to it.


### Building and installing modules

Before we are going to jump into adding content to Foo, let's take a look on how
to build and deploy the Monochrome bundle with the Foo DFM. The remainder of
this guide assumes the environment variable `OUTDIR` is set to a properly
configured GN build directory (e.g. `out/Debug`).

To build and install the Monochrome bundle to your connected device, run:

```shell
$ autoninja -C $OUTDIR monochrome_public_bundle
$ $OUTDIR/bin/monochrome_public_bundle install -m base -m foo
```

This will install Foo alongside the rest of Chrome. The rest of Chrome is called
_base_ module in the bundle world. The Base module will always be put on the
device when initially installing Chrome.

*** note
**Note:** You have to specify `-m base` here to make it explicit which modules
will be installed. If you only specify `-m foo` the command will fail. It is
also possible to specify no modules. In that case, the script will install the
set of modules that the Play Store would install when first installing Chrome.
That may be different than just specifying `-m base` if we have non-on-demand
modules.
***

You can then check that the install worked with:

```shell
$ adb shell dumpsys package org.chromium.chrome | grep splits
>   splits=[base, config.en, foo]
```

Then try installing the Monochrome bundle without your module and print the
installed modules:

```shell
$ $OUTDIR/bin/monochrome_public_bundle install -m base
$ adb shell dumpsys package org.chromium.chrome | grep splits
>   splits=[base, config.en]
```


### Adding java code

To make Foo useful, let's add some Java code to it. This section will walk you
through the required steps.

First, define a module interface for Foo. This is accomplished by adding the
`@ModuleInterface` annotation to the Foo interface. This annotation
automatically creates a `FooModule` class that can be used later to install and
access the module. To do this, add the following in the new file
`//chrome/android/features/foo/public/java/src/org/chromium/chrome/features/foo/Foo.java`:

```java
package org.chromium.chrome.features.foo;

import org.chromium.components.module_installer.builder.ModuleInterface;

/** Interface to call into Foo feature. */
@ModuleInterface(module = "foo", impl = "org.chromium.chrome.features.FooImpl")
public interface Foo {
    /** Magical function. */
    void bar();
}
```

*** note
**Note:** To reflect the separation from "Chrome browser" code, features should
be defined in their own package name, distinct from the chrome package - i.e.
`org.chromium.chrome.features.<feature-name>`.
***

Next, define an implementation that goes into the module in the new file
`//chrome/android/features/foo/internal/java/src/org/chromium/chrome/features/foo/FooImpl.java`:

```java
package org.chromium.chrome.features.foo;

import org.chromium.base.Log;
import org.chromium.base.annotations.UsedByReflection;

@UsedByReflection("FooModule")
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
Therefore, put those classes into the base module. For this create a list of
those Java files in
`//chrome/android/features/foo/public/foo_public_java_sources.gni`:

```gn
foo_public_java_sources = [
  "//chrome/android/features/foo/public/java/src/org/chromium/chrome/features/foo/Foo.java",
]
```

Then add this list to `chrome_java in //chrome/android/BUILD.gn`:

```gn
...
import("//chrome/android/features/foo/public/foo_public_java_sources.gni")
...
android_library("chrome_java") {
  ...
  java_files += foo_public_java_sources
}
...
```

The actual implementation, however, should go into the Foo DFM. For this
purpose, create a new file `//chrome/android/features/foo/internal/BUILD.gn` and
make a library with the module Java code in it:

```gn
import("//build/config/android/rules.gni")

android_library("java") {
  # Define like ordinary Java Android library.
  java_files = [
    "java/src/org/chromium/chrome/features/foo/FooImpl.java",
    # Add other Java classes that should go into the Foo DFM here.
  ]
  # Put other Chrome libs into the classpath so that you can call into the rest
  # of Chrome from the Foo DFM.
  deps = [
    "//base:base_java",
    "//chrome/android:chrome_java",
    # etc.
    # Also, you'll need to depend on any //third_party or //components code you
    # are using in the module code.
  ]
}
```

Then, add this new library as a dependency of the Foo module descriptor in
`//chrome/android/features/foo/foo_module.gni`:

```gn
foo_module_desc = {
  ...
  java_deps = [
    "//chrome/android/features/foo/internal:java",
  ]
}
```

Finally, tell Android that your module is now containing code. Do that by
removing the `android:hasCode="false"` attribute from the `<application>` tag in
`//chrome/android/features/foo/internal/java/AndroidManifest.xml`. You should be
left with an empty tag like so:

```xml
...
    <application />
...
```

Rebuild and install `monochrome_public_bundle`. Start Chrome and run through a
flow that tries to executes `bar()`. Depending on whether you installed your
module (`-m foo`) "`bar in module`" or "`module not installed`" is printed to
logcat. Yay!

### Adding third-party native code

You can add a third-party native library (or any standalone library that doesn't
depend on Chrome code) by adding it as a loadable module to the module descriptor in
`//chrome/android/features/foo/foo_module.gni`:

```gn
foo_module_desc = {
  ...
  loadable_modules_32_bit = [ "//path/to/32/bit/lib.so" ]
  loadable_modules_64_bit = [ "//path/to/64/bit/lib.so" ]
}
```

### Adding Chrome native code

Chrome native code may be placed in a DFM.

A linker-assisted partitioning system automates the placement of code into
either the main Chrome library or feature-specific .so libraries. Feature code
may continue to make use of core Chrome code (eg. base::) without modification,
but Chrome must call feature code through a virtual interface.

Partitioning is explained in [Android Native
Libraries](android_native_libraries.md#partitioned-libraries).

#### Creating an interface to feature code

One way of creating an interface to a feature library is through an interface
definition. Feature Foo could define the following in
`//chrome/android/features/foo/public/foo_interface.h`:

```c++
class FooInterface {
 public:
  virtual ~FooInterface() = default;

  virtual void ProcessInput(const std::string& input) = 0;
}
```

Alongside the interface definition, also in
`//chrome/android/features/foo/public/foo_interface.h`, it's helpful to define a
factory function type that can be used to create a Foo instance:

```c++
typedef FooInterface* CreateFooFunction(bool arg1, bool arg2);
```

<!--- TODO(cjgrant): Add a full, pastable Foo implementation. -->
The feature library implements class Foo, hiding its implementation within the
library. The library may then expose a single entrypoint, a Foo factory
function. Here, C naming is (optionally) used so that the entrypoint symbol
isn't mangled. In `//chrome/android/features/foo/internal/foo.cc`:

```c++
extern "C" {
// This symbol is retrieved from the Foo feature module library via dlsym(),
// where it's bare address is type-cast to its actual type and executed.
// The forward declaration here ensures that CreateFoo()'s signature is correct.
CreateFooFunction CreateFoo;

__attribute__((visibility("default"))) FooInterface* CreateFoo(
    bool arg1, bool arg2) {
  return new Foo(arg1, arg2);
}
}  // extern "C"
```

Ideally, the interface to the feature will avoid feature-specific types. If a
feature defines complex data types, and uses them in its own interface, then its
likely the main library will utilize the code backing these types. That code,
and anything it references, will in turn be pulled back into the main library.

Therefore, designing the feature inferface to use C types, C++ standard types,
or classes that aren't expected to move out of Chrome's main library is ideal.
If feature-specific classes are needed, they simply need to avoid referencing
feature library internals.

*** note
**Note:** To help enforce separation between the feature interface and
implementation, the interface class is best placed in its own GN target, on
which the feature and main library code both depend.
***

#### Marking feature entrypoints

Foo's feature module descriptor needs to pull in the appropriate native GN code
dependencies, and also indicate the name of the file that lists the entrypoint
symbols. In `//chrome/android/features/foo/foo_module.gni`:

```gn
foo_module_desc = {
  ...
  native_deps = [ "//chrome/android/features/foo/internal:foo" ]
  native_entrypoints = "//chrome/android/features/foo/internal/module_entrypoints.lst"
}
```

The module entrypoint file is a text file listing symbols. In this example,
`//chrome/android/features/foo/internal/module_entrypoints.lst` has only a
single factory function exposed:

```shell
# This file lists entrypoints exported from the Foo native feature library.

CreateFoo
```

These symbols will be pulled into a version script for the linker, indicating
that they should be exported in the dynamic symbol table of the feature library.

*** note
**Note:** If C++ symbol names are chosen as entrypoints, the full mangled names
must be listed.
***

Additionally, it's necessary to map entrypoints to a particular partition. To
follow compiler/linker convention, this is done at the compiler stage. A cflag
is applied to source file(s) that may supply entrypoints (it's okay to apply the
flag to all feature source - the attribute is utilized only on modules that
export symbols). In `//chrome/android/features/foo/internal/BUILD.gn`:

```gn
static_library("foo") {
  sources = [
    ...
  ]

  # Mark symbols in this target as belonging to the Foo library partition. Only
  # exported symbols (entrypoints) are affected, and only if this build supports
  # native modules.
  if (use_native_modules) {
    cflags = [ "-fsymbol-partition=libfoo.so" ]
  }
}
```

Feature code is free to use any existing Chrome code (eg. logging, base::,
skia::, cc::, etc), as well as other feature targets. From a GN build config
perspective, the dependencies are defined as they normally would. The
partitioning operation works independently of GN's dependency tree.

```gn
static_library("foo") {
  ...

  # It's fine to depend on base:: and other Chrome code.
  deps = [
    "//base",
    "//cc/animation",
    ...
  ]

  # Also fine to depend on other feature sub-targets.
  deps += [
    ":some_other_foo_target"
  ]

  # And fine to depend on the interface.
  deps += [
    ":foo_interface"
  ]
}
```

#### Opening the feature library

Now, code in the main library can open the feature library and create an
instance of feature Foo. Note that in this example, no care is taken to scope
the lifetime of the opened library. Depending on the feature, it may be
preferable to open and close the library as a feature is used.
`//chrome/android/features/foo/factory/foo_factory.cc` may contain this:

```c++
std::unique_ptr<FooInterface> FooFactory(bool arg1, bool arg2) {
  // Open the feature library, using the partition library helper to map it into
  // the correct memory location. Specifying partition name *foo* will open
  // libfoo.so.
  void* foo_library_handle =
        base::android::BundleUtils::DlOpenModuleLibraryPartition("foo");
  }
  DCHECK(foo_library_handle != nullptr) << "Could not open foo library:"
      << dlerror();

  // Pull the Foo factory function out of the library. The function name isn't
  // mangled because it was extern "C".
  CreateFooFunction* create_foo = reinterpret_cast<CreateFooFunction*>(
      dlsym(foo_library_handle, "CreateFoo"));
  DCHECK(create_foo != nullptr);

  // Make and return a Foo!
  return base::WrapUnique(create_foo(arg1, arg2));
}

```

*** note
**Note:** Component builds do not support partitioned libraries (code splitting
happens across component boundaries instead). As such, an alternate, simplified
feature factory implementation must be supplied (either by linking in a
different factory source file, or using #defines in the factory) that simply
instantiates a Foo object directly.
***

Finally, the main library is free to utilize Foo:

```c++
  auto foo = FooFactory::Create(arg1, arg2);
  foo->ProcessInput(const std::string& input);
```

#### JNI

Read the `jni_generator` [docs](../base/android/jni_generator/README.md) before
reading this section.

There are some subtleties to how JNI registration works with DFMs:

* Generated wrapper `ClassNameJni` classes are packaged into the DFM's dex file
* The class containing the actual native definitions, `GEN_JNI.java`, is always
  stored in the base module
* If the DFM is only included in bundles that use
  [implicit JNI registration](android_native_libraries.md#JNI-Native-Methods-Resolution)
  (i.e. Monochrome and newer), then no extra consideration is necessary
* Otherwise, the DFM will need to provide a `generate_jni_registration` target
  that will generate all of the native registration functions


### Adding Android resources

In this section we will add the required build targets to add Android resources
to the Foo DFM.

First, add a resources target to
`//chrome/android/features/foo/internal/BUILD.gn` and add it as a dependency on
Foo's `java` target in the same file:

```gn
...
android_resources("java_resources") {
  # Define like ordinary Android resources target.
  ...
  custom_package = "org.chromium.chrome.features.foo"
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
`//chrome/android/features/foo/internal/java/strings/android_foo_strings.grd` as
follows:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<grit
    current_release="1"
    latest_public_release="0"
    output_all_resource_defines="false">
  <outputs>
    <output
        filename="values-am/android_foo_strings.xml"
        lang="am"
        type="android" />
    <!-- List output file for all other supported languages. See
         //chrome/android/java/strings/android_chrome_strings.grd for the full
         list. -->
    ...
  </outputs>
  <translations>
    <file lang="am" path="vr_translations/android_foo_strings_am.xtb" />
    <!-- Here, too, list XTB files for all other supported languages. -->
    ...
  </translations>
  <release allow_pseudo="false" seq="1">
    <messages fallback_to_english="true">
      <message name="IDS_BAR_IMPL_TEXT" desc="Magical string.">
        impl
      </message>
    </messages>
  </release>
</grit>
```

Then, create a new GRD target and add it as a dependency on `java_resources` in
`//chrome/android/features/foo/internal/BUILD.gn`:

```gn
...
java_strings_grd("java_strings_grd") {
  defines = chrome_grit_defines
  grd_file = "java/strings/android_foo_strings.grd"
  outputs = [
    "values-am/android_foo_strings.xml",
    # Here, too, list output files for other supported languages.
    ...
  ]
}
...
android_resources("java_resources") {
  ...
  deps = [":java_strings_grd"]
  custom_package = "org.chromium.chrome.features.foo"
}
...
```

You can then access Foo's resources using the
`org.chromium.chrome.features.foo.R` class. To do this change
`//chrome/android/features/foo/internal/java/src/org/chromium/chrome/features/foo/FooImpl.java`
to:

```java
package org.chromium.chrome.features.foo;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.features.foo.R;

@UsedByReflection("FooModule")
public class FooImpl implements Foo {
    @Override
    public void bar() {
        Log.i("FOO", ContextUtils.getApplicationContext().getString(
                R.string.bar_impl_text));
    }
}
```

*** note
**Warning:** While your module is emulated (see [below](#on-demand-install))
your resources are only available through
`ContextUtils.getApplicationContext()`. Not through activities, etc. We
therefore recommend that you only access DFM resources this way. See
[crbug/949729](https://bugs.chromium.org/p/chromium/issues/detail?id=949729)
for progress on making this more robust.
***


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
the DFM is not installed as a true split. Instead it will be emulated by Chrome.
Fake-install and launch Chrome with the following command:

```shell
$ $OUTDIR/bin/monochrome_public_bundle install -m base -f foo
$ $OUTDIR/bin/monochrome_public_bundle launch --args="--fake-feature-module-install"
```

When running the install code, the Foo DFM module will be emulated.
This will be the case in production right after installing the module. Emulation
will last until Play Store has a chance to install your module as a true split.
This usually takes about a day.

*** note
**Warning:** There are subtle differences between emulating a module and
installing it as a true split. We therefore recommend that you always test both
install methods.
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
`//chrome/android/features/foo/internal/java/AndroidManifest.xml` should look
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


### Integration test APK and Android K support

On Android K we still ship an APK. To make the Foo feature available on Android
K add its code to the APK build. For this, add the `java` target to
the `chrome_public_common_apk_or_module_tmpl` in
`//chrome/android/chrome_public_apk_tmpl.gni` like so:

```gn
template("chrome_public_common_apk_or_module_tmpl") {
  ...
  target(_target_type, target_name) {
    ...
    if (_target_type != "android_app_bundle_module") {
      deps += [
        "//chrome/android/features/foo/internal:java",
      ]
    }
  }
}
```

This will also add Foo's Java to the integration test APK. You may also have to
add `java` as a dependency of `chrome_test_java` if you want to call into Foo
from test code.
