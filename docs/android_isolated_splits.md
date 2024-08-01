# Isolated Splits

This doc aims to explain the ins and outs of using Isolated Splits on Android.

For an overview of apk splits and how to use them in Chrome, see
[android_dynamic_feature_modules.md].

[TOC]

## About

### What are Isolated Splits?

Isolated Splits is an opt-in feature (via [android:isolatedSplits] manifest
entry) that cause all feature splits in an application to have separate
`Context` objects, rather than being merged together into a single Application
`Context`. The `Context` objects have distict `ClassLoader` and `Resources`
instances. They are loaded on-demand instead of eagerly on launch.

With Isolated Splits, each feature split is loaded in its own ClassLoader, with
the parent split set as the parent ClassLoader.

[android:isolatedSplits]: https://developer.android.com/reference/android/R.attr#isolatedSplits

### Why use Isolated Splits?

The more DEX that is loaded on start-up, the more RAM and time it takes for
application code to start running. Loading less code on start-up is particularly
helpful for Chrome, since Chrome tends to spawn a lot of processes, and because
renderer processes require almost no DEX.

### What Splits Exist in Chrome?

Chrome's splits look like:

```
base.apk <-- chrome.apk <-- image_editor.apk
                        <-- feedv2.apk
                        <-- ...
```

* The browser process loads the `chrome` split on start-up, and other splits are
  loaded on-demand.
* Renderer and GPU processes do not load any feature splits.
  * The `chrome` split exists to minimize the amount of DEX loaded by renderer
    processes. However, it also enables faster browser process start-up by
    allowing DEX to be loaded concurrently with other start-up tasks.

### How are Isolated Splits Loaded?

There are two ways:
1) They can be loaded by Android Framework when handling an intent.
   * E.g.: If a feature split defines an Activity in its manifest, Android
     will create the split's Context and associate the Activity with it.
2) They can be loaded explicitly via [BundleUtils.createIsolatedSplitContext()].
   * The most common way to load in this way is through declaring a
     `ModuleInterface`, as described in [android_dynamic_feature_modules.md].

[BundleUtils.createIsolatedSplitContext()]: https://source.chromium.org/search?q=func:createIsolatedSplitContext&ss=chromium
[android_dynamic_feature_modules.md]: android_dynamic_feature_modules.md

## OS Support for Isolated Splits

Initial support was added in Android O. On earlier Android versions, all
feature splits are loaded during process start-up and merged into the
Application Context.

## OS Bugs

### Base ClassLoader used for Services in Splits (Android Pre-S)

Service Contexts are created with the base split's ClassLoader rather than the
split's ClassLoader.

Fixed in Android S. Bug: [b/169196314] (Googler only).

**Work-around:**

We use [SplitCompatService] (and siblings) to put a minimal service class in the
base split. They forward all calls to an implementation class, which can live
in the `chrome` split (or other splits). We also have a [compile-time check] to
enforce that no Service subclasses exist outside of the base split.

[b/169196314]: https://issuetracker.google.com/169196314
[SplitCompatService]: https://source.chromium.org/search?q=symbol:SplitCompatService&ss=chromium
[compile-time check]: https://source.chromium.org/search?q=symbol:_MaybeCheckServicesAndProvidersPresentInBase&ss=chromium

### Corrupted .odex (Android O MR1)

Android O MR1 has a bug where `bg-dexopt-job` (runs during maintenance windows)
breaks optimized dex files for Isolated Splits. The corrupt `.odex` files cause
extremely slow startup times.

**Work-around:**

We [preemptively run] `dexopt` so that `bg-dexopt-job` decides there is no work
to do. We trigger this from [PackageReplacedBroadcastReceiver] so that it
happens whenever Chrome is updated rather than when the user launches Chrome.

[preemptively run]: https://source.chromium.org/search?q=symbol:DexFixer.needsDexCompile&ss=chromium
[PackageReplacedBroadcastReceiver]: https://source.chromium.org/search?q=symbol:PackageReplacedBroadcastReceiver&ss=chromium

### Conflicting ClassLoaders #1

Tracked by [b/172602571], sometimes a split's parent ClassLoader is different
from the Application's ClassLoader. This manifests as odd-looking
`ClassCastExceptions` where `"TypeA cannot be cast to TypeA"` (since the two
`TypeAs` are from different ClassLoaders).

Tracked by UMA `Android.IsolatedSplits.ClassLoaderReplaced`. Occurs < 0.05% of
the time.

**Work-around:**

On Android O, there is no work-around. We just [detect and crash early].

Android P added [AppComponentFactory], which offers a hook that we use to
[detect and fix] ClassLoader mixups. The ClassLoader mixup also needs to be
corrected for `ContextImpl` instances, which we do via
[ChromeBaseAppCompatActivity.attachBaseContext()].

[b/172602571]: https://issuetracker.google.com/172602571
[detect and crash early]: https://source.chromium.org/search?q=crbug.com%2F1146745&ss=chromium
[AppComponentFactory]: https://developer.android.com/reference/android/app/AppComponentFactory
[detect and fix]: https://source.chromium.org/search?q=f:splitcompatappcomponentfactory&ss=chromium
[ChromeBaseAppCompatActivity.attachBaseContext()]: https://source.chromium.org/search?q=BundleUtils\.checkContextClassLoader&ss=chromium

### Conflicting ClassLoaders #2

Tracked by [b/172602571], when a new split language split or feature split is
installed, the ClassLoaders for non-base splits are recreated. Any reference to
a class from the previous ClassLoader (e.g. due to native code holding
references to them) will result in `ClassCastExceptions` where
`"TypeA cannot be cast to TypeA"`.

**Work-around:**

There is no work-around. This is a source of crashes. We could potentially
mitigate by restarting chrome when a split is installed.

### System.loadLibrary() Broken for Libraries in Splits

Tracked by [b/171269960], Android is not adding the apk split to the associated
ClassLoader's `nativeSearchPath`.  This means that `libfoo.so` within an
isolated split is not found by a call to `System.loadLibrary("foo")`.

**Work-around:**

Load libraries via `System.load()` instead.

```java
System.load(BundleUtils.getNativeLibraryPath("foo", "mysplitsname"));
```

[b/171269960]: https://issuetracker.google.com/171269960

### System.loadLibrary() Unusable from Split if Library depends on Another Loaded by Base Split

Also tracked by [b/171269960], maybe related to linker namespaces. If a split
tries to load `libfeature.so`, and `libfeature.so` has a `DT_NEEDED` entry for
`libbase.so`, and `libbase.so` is loaded by the base split, then the load will
fail.

**Work-around:**

Have base split load libraries from within splits. Proxy all JNI calls through
a class that exists in the base split.

### System.loadLibrary() Broken for Libraries in Splits on System Image

Also tracked by [b/171269960], Android's linker config (`ld.config.txt`) sets
`permitted_paths="/data:/mnt/expand"`, and then adds the app's `.apk` to an
allowlist. This allowlist does not contain apk splits, so library loading is
blocked by `permitted_paths` when the splits live on the `/system` partition.

**Work-around:**

Use compressed system image stubs (`.apk.gz` and `-Stub.apk`) so that Chrome is
extracted to the `/data` partition upon boot.

### Too Many Splits Break App Zygote

Starting with Android Q / TriChrome, Chrome uses an [Application Zygote]. As
part of initialization, Chrome's `ApplicationInfo` object is serialized into a
fixed size buffer. Each installed split increases the size of the
`ApplicationInfo` object, and can push it over the buffer's limit.

**Work-around:**

Do not add too many splits, and monitor the size of our `ApplicationInfo` object
([crbug/1298496]).

[crbug/1298496]: https://bugs.chromium.org/p/chromium/issues/detail?id=1298496
[Application Zygote]: https://developer.android.com/reference/android/app/ZygotePreload

### AppComponentFactory does not Hook Split ClassLoaders

`AppComponentFactory#instantiateClassLoader()` is meant to allow apps to hook
`ClassLoader` creation. The hook is called for the base split, but not for other
isolated splits. Tracked by [b/265583114]. There is no work-around.

[b/265583114]: https://issuetracker.google.com/265583114

### Incorrect Handling of Shared Libraries

Tracked by [b/265589431]. If an APK split has `<uses-library>` in its manifest,
the classloader for the split is meant to have that library added to it by the
framework. However, Android does not add the library to the classpath when a
split is dynamically installed, but instead adds it to the classpath of the base
split's classloader upon subsequent app launches.

**Work-around:**

 * Always add `<uses-library>` to the base split.

[b/265589431]: https://issuetracker.google.com/265589431

## Other Quirks & Subtleties

### System Image APKs

When distributing Chrome on Android system images, we generate a single `.apk`
file that contains all splits merged together (or rather, all splits whose
`AndroidManifest.xml` contain `<dist:fusing dist:include="true" />`). We do this
for simplicity; Android supports apk splits on the system image.

You can build Chrome's system `.apk` via:
```sh
out/Release/bin/trichrome_chrome_bundle build-bundle-apks --output-apks SystemChrome.apks --build-mode system
unzip SystemChrome.apks system/system.apk
```

Shipping a single `.apk` file simplifies distribution, but eliminates all the
benefits of Isolated Splits.

### Chrome's Application ClassLoader

A lot of Chrome's code uses the `ContextUtils.getApplicationContext()` as a
Context object. Rather than auditing all usages and replacing applicable ones
with the `chrome` split's Context, we [use reflection] to change the
Application instance's ClassLoader to point to the `chrome` split's ClassLoader.

[use reflection]: https://source.chromium.org/search?q=f:SplitChromeApplication%20replaceClassLoader&ss=chromium

### ContentProviders

Unlike other application components, ContentProviders are created on start-up
even when they are not the reason the process is being created. If a
ContentProvider were to be declared in a split, its split's Context would need
to be loaded during process creation, eliminating any benefit.

**Work-around:**

We declare all ContentProviders in the base split's `AndroidManifest.xml` and
enforce this with a [compile-time check]. ContentProviders that would pull in
significant amounts of code use [SplitCompatContentProvider] to delegate to a
helper class living within a split.

[compile-time check]: https://source.chromium.org/search?q=symbol:_MaybeCheckServicesAndProvidersPresentInBase&ss=chromium
[SplitCompatContentProvider]: https://source.chromium.org/search?q=symbol:SplitCompatContentProvider&ss=chromium

### JNI and ClassLoaders

When you call from native-&gt;Java (via `@CalledByNative`), there are two APIs
that Chrome could use to resolve the target class:

1) JNI API: [JNIEnv::FindClass()]
2) Java Reflection API:`ClassLoader.loadClass())`

Chrome uses #2. For methods within feature splits, `generate_jni()` targets
use `split_name = "foo"` to make the generated JNI code use the split's
ClassLoader.

[JNIEnv::FindClass()]: https://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/functions.html#wp16027

### Accessing Android Resources

When resources live in a split, they must be accessed through a Context object
associated with that split. However:

* Bug: Chrome's build system [improperly handles ID conflicts] between splits.
* Bug: Splash screens [fail to load] for activities in Isolated Splits (unless
  associated resources are defined in the base split).
* Quirk: `RemoteViews`, notification icons, and other Android features that
  access resources by Package ID require resources to be in the base split when
  Isolated Splits are enabled.

**Work-around:**

Chrome [stores all Android resources in the base split]. There is [a crbug] to
track moving resources into splits, but it may prove too challenging.

[stores all Android resources in the base split]: https://source.chromium.org/search?q=recursive_resource_deps%5C%20%3D%5C%20true
[improperly handles ID conflicts]: https://crbug.com/1133898
[fail to load]: https://issuetracker.google.com/171743801
[a crbug]: https://crbug.com/1165782

### Inflating Layouts

Layouts should be inflated with an Activity Context so that
configuration-specific resources and themes are used. If layouts contain
references to View classes from different feature splits than the Activity's,
then the views' split ClassLoaders must be used.

**Work-around:**

Use the `ContextWrapper` created via: [BundleUtils.createContextForInflation()]

[BundleUtils.createContextForInflation()]: https://source.chromium.org/search?q=symbol:BundleUtils.createContextForInflation&ss=chromium

### onRestoreInstanceState with Classes From Splits

When Android kills an app, it normally calls `onSaveInstanceState()` to allow
the app to first save state. The saved state includes the class names of active
Fragments, RecyclerViews, and potentially other classes from splits. Upon
re-launch, these class names are used to reflectively instantiate instances.
`FragmentManager` uses the ClassLoader of the Activity to instantiate them,
and `RecyclerView` uses the ClassLoader associated with the `Bundle` object.
The reflection fails if the active Activity resides in a different spilt from
the reflectively instantiated classes.

**Work-around:**

Chrome stores the list of all splits that have been used for inflation during
[`onSaveInstanceState`] and then uses [a custom ClassLoader] to look within them
for classes that do not exist in the application's ClassLoader. The custom
ClassLoader is passed to `Bundle` instances in
`ChromeBaseAppCompatActivity.onRestoreInstanceState()`.

Having Android Framework call `Bundle.setClassLoader()` is tracked in
[b/260574161].

[`onSaveInstanceState`]: https://source.chromium.org/search?q=symbol:ChromeBaseAppCompatActivity.onSaveInstanceState&ss=chromium
[a custom ClassLoader]: https://source.chromium.org/search?q=symbol:ChromeBaseAppCompatActivity.getClassLoader&ss=chromium
[b/260574161]: https://issuetracker.google.com/260574161

### Calling Methods Across a Split Boundary

Due to having different ClassLoaders, package-private methods don't work across
the boundary, even though they will compile.

**Work around:**

Make any method public that you wish to call in another module, even if it's in
the same package.

### Proguarding Splits

"Proguarding" is the build step that performs whole-program optimization of Java
code, and "R8" is the program Chrome uses to do this. R8 currently supports
mapping input `.jar` files to output feature splits. If two feature splits share
a common GN `dep`, then its associated `.jar` will be promoted to the parent
split (or to the base split) by our [proguard.py] wrapper script.

This scheme means that if a single class from a large library is needed by, or
promoted to, the base split, then every class needed from that library by
feature splits will also remain in the base split. The feature request to have
R8 move code into deeper splits on a per-class basis is [b/225876019] (Googler
only).

[proguard.py]: https://source.chromium.org/search?q=symbol:_DeDupeInputJars%20f:proguard.py&ss=chromium
[b/225876019]: https://issuetracker.google.com/225876019

### Metadata in Splits

Metadata is queried on a per-app basis (not a per-split basis). E.g.:

```java
ApplicationInfo ai = context.getPackageManager().getApplicationInfo(context.getPackageName(), PackageManager.GET_META_DATA);
Bundle b = ai.metaData;
```

This bundle contains merged values from all fully-installed apk splits.

## Other Resources

 * [go/isolated-splits-dev-guide] (Googlers only).
 * [go/clank-isolated-splits-architecture] (Googlers only).

[go/isolated-splits-dev-guide]: http://go/isolated-splits-dev-guide
[go/clank-isolated-splits-architecture]: http://go/clank-isolated-splits-architecture
