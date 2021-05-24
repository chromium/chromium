# Bytecode Rewriting

## TL;DR

We modify the return type of AndroidX's `Fragment.getActivity()` method from `FragmentActivity`
to `Activity` to more easily add Fragments from multiple ClassLoaders into the same Fragment tree.

## Why?

In Java, two instances of the same class loaded from two different ClassLoaders aren't compatible
with each other at runtime. Because AndroidX libraries are bundled with each APK that use them, and
different APKs are loaded with different ClassLoaders, AndroidX classes from one APK cannot be used
with the same class from another APK. This causes problems for Fragment-based UIs in WebLayer, where
the implementation is in a different ClassLoader than the embedding app, so its Fragments cannot be
added to the embedding app's Fragment tree.

Note that this issue doesn't apply to Framework or standard library classes. Java ClassLoaders form
a tree, and if a ClassLoader can't find a particular class, it delegates to its parent.
The leaf ClassLoader used to load an app is responsible for loading the app's class files, while one
of its parents will load system-level classes. Because AndroidX classes get loaded by the
app-specific ClassLoader, different apps will load mutually incompatible versions, but a class like
Activity, which gets loaded from a parent ClassLoader, *will* be compatible between APKs at runtime,
because it ends up gets loaded from a common ClassLoader.

To get around this incompatibility, we can create a RemoteFragment that lives in the embedding app,
and a RemoteFragmentImpl that lives in another APK. The RemoteFragment can be added to the original
Fragment tree, and will forward all Fragment lifecycle events over an AIDL interface to
RemoteFragmentImpl. The fake Fragment in the secondary APK (RemoteFragmentImpl) can create a
FragmentController, which allows it to become the host of its own Fragment tree, and any UIs from
the secondary ClassLoaded can be added to this new Fragment tree that's been essentially grafted
onto the original.

This mostly works, but runs into issues when Fragments call `Fragment.getActivity()`, which they do
a lot. The getActivity implementation takes the Activity given to the FragmentController constructor
(via FragmentHostCallback), and casts it to a FragmentActivity before returning it. The original
Activity will typically be a FragmentActivity from the embedding app's ClassLoader, which means that
due to the aforementioned issues, this cast will fail when run in the secondary ClassLoader's
Fragment class because even though the Activity is a FragmentActivity, it's from the wrong
ClassLoader.

To fix this second issue, we modify the bytecode of `Fragment.getActivity()` in the AndroidX
prebuilt .aar files to return a plain Activity instead of a FragmentActivity. This allows us to
continue calling getActivity() as normal. Note that this does mean FragmentActivity-specific methods
can no longer be used in Fragments, but there were no uses of them in Chromium that couldn't be
trivially removed as of late 2020.

## How does it work?

The bytecode rewriting happens at build time by
[FragmentActivityReplacer](https://source.chromium.org/chromium/chromium/src/+/main:build/android/bytecode/java/org/chromium/bytecode/FragmentActivityReplacer.java),
which is specified as a bytecode rewriter via the `bytecode_rewriter_target` rule.  Compilation errors
related to this should get detected by
[compile_java.py](https://source.chromium.org/chromium/chromium/src/+/main:build/android/gyp/compile_java.py),
and print a message pointing users here, which is likely why you're reading this :)

If you need to apply FragmentActivityReplacer to a given target then add …

```
bytecode_rewriter_target = "//build/android/bytecode:fragment_activity_replacer"
```

… to the build configuration for that target.

If you still get a build or runtime error related to a FragmentActivity after adding in the
replacer, then the library may actually rely on the Activity being a FragmentActivity. If so, it
likely won't work with WebLayer as-is. If you know there are no plans to use the library in
WebLayer, you can try adding this instead:

```
bytecode_rewriter_target = "//build/android/bytecode:fragment_activity_replacer_single_androidx"
```

## How does this affect my code?

The goal is for these changes to be as transparent as possible; most code shouldn't run into issues.
However, if there's no way around calling a FragmentActivity method in your code, **and your
Fragment is in //chrome**, you could cast the Activity to a FragmentActivity as AndroidX used to do.
If your Fragment is in //components, FragmentActivity methods will likely not work directly, and may
need to be forwarded to an implementation in the original ClassLoader somehow.

The more important thing to note is that in a multi-ClassLoader world, `getActivity()` and
`getContext()` will typically return two different objects, so we need to be more careful about which
method we call, particularly for code in //components. `getActivity()` will return the Activity from
the original ClassLoader, and should be used to for calls like `.finish*()`, `.setTitle(), and
`.startActivity()` (which live in Activity anyway). When loading resources, you should default to
calling `getContext()`, as resources usually come from the same ClassLoader as the Fragment, and the
Context should be configured to load them correctly.

As a rule of thumb, prefer `getContext()` to `getActivity()`, unless you need to operate on the
Activity itself, or you know the resource or setting you need belongs to the original Activity.
