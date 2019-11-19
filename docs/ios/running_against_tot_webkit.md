# Running Against ToT WebKit in the Simulator

This is a simplified workflow intended to make it as easy as possible to compile
and run against ToT WebKit locally. If you intend to do daily development in a
WebKit tree, you may want to consider an alternate workflow as described
[here](https://docs.google.com/document/d/1l74JNYr9dniUnT5aKUS1_EswlDmuQghlEfyVCiPh9a4).

A design doc explaining the details behind these steps can be found
[here](https://docs.google.com/document/d/15HCmvC_yKNpmcrgHaGUA5LWWwROCmOz7qyYnMmhQkBQ)

Note: this only works on Simulator.  At present, we do not have the ability to
compile WebKit for devices.

## Warning: do this in a new checkout

The changes below will substantially increase compile times, for both clean and
incremental builds.  No-op builds will jump from ~0s to ~60s, which will
likely be an unacceptable regression in a daily development checkout.

By pulling a separate, new checkout for WebKit, you sidestep these issues at the
expense of greater disk usage.

## Updating .gclient to pull WebKit

To start, edit your `.gclient` file to checkout WebKit @HEAD. Both of the custom
variables are necessary, as by default webkit.git is pinned to a revision in the
distant past.

```
solutions = [
  {
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "name": "src",

    "custom_vars": {
      "checkout_ios_webkit": True,
      "ios_webkit_revision": "refs/heads/master",
    },

  },
]
target_os = ["ios"]
target_os_only = "True"
````

Add the `custom_vars` section as above and re-run `gclient sync`.  After it
completes, you should have a WebKit checkout in `ios/third_party/webkit/src`.

## Building

WebKit-enabled checkouts expose a `webkit` target via GN/ninja. The WebKit
libraries will automatically be built as part of the default set of targets, or
you can build it individually.

```
# Builds all targets, including webkit.
ninja -C out/Debug-iphonesimulator

# Builds just the webkit target.
ninja -C out/Debug-iphonesimulator webkit
```

The WebKit build output can be found at
`out/Debug-iphonesimulator/obj/ios/third_party/webkit/`.

### Speeding up clean builds by building WebKit first

We build using a set of scripts provided by WebKit; they set up an environment
and then invoke xcodebuild directly.  To prevent xcodebuild from competing with
ninja for CPU/RAM, we limit the WebKit build to four parallel jobs. This is
generally sufficient for incremental builds, but for clean builds (or after a
sync) it may be faster to build WebKit without this limitation.

```
# After a sync, build WebKit first.
ios/third_party/webkit/build-webkit.py

# Once WebKit is built, invoke ninja as usual to build Chromium.
ninja -C out/Debug-iphonesimulator
```

### One-time setup (per machine, per version of Xcode)

If you move to a new machine or install a new version of Xcode, you'll need to
run a setup script in order to copy some headers from the macOS SDK into the iOS
SDK.

```
sudo ios/third_party/webkit/src/Tools/Scripts/configure-xcode-for-ios-development
```


## Running against locally-built libraries

To run against the libraries you've just built, set the `DYLD_FRAMEWORK_PATH`
environment variable to the directory containing the WebKit build output.  This
is usually easiest to do in the Xcode UI.

```
DYLD_FRAMEWORK_PATH = /path/to/out/Debug-iphonesimulator/obj/ios/third_party/webkit/Debug-iphonesimulator/
```

