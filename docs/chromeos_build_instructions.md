# Chrome OS Build Instructions

Chrome for Chromium OS can be built in a couple different ways. After following
the [initial setup](#common-setup), you'll need to choose one of the following
build configurations:

- If you're interested in testing Chrome OS code in Chrome, but not interactions
  with Chrome OS services, you can build for
  [linux-chromeos](#Chromium-OS-on-Linux-linux_chromeos) using just a Linux
  workstation.
- Otherwise, Chrome's full integration can be covered by building for a real
  Chrome OS device or VM using [Simple Chrome](#Chromium-OS-Device-Simple-Chrome).
- Use `is_chromeos_device` in GN and `BUILDFLAG(IS_CHROMEOS_DEVICE)` in C++ code
  to differentiate between these two modes.

[TOC]

## Common setup

First, follow the [normal Linux build
instructions](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md)
as usual to get a Chromium checkout.

You'll also need to add `'chromeos'` to the `target_os` list in your `.gclient`
configuration, which will fetch the additional build dependencies required for
CrOS. This file is located one level up from your Chromium checkout's `src`.

If you don't already have a `target_os` line present, simply add this to the
end of the `.gclient` file:

    target_os = ['chromeos']

If you already have a `target_os` line present in your `.gclient file`, you can
simply append `'chromeos'` to the existing list there. For example:

    target_os = ['android', 'chromeos']

Once your `.gclient` file is updated, you will need to run `gclient sync` once
before proceeding with the rest of these instructions.

## Chromium OS on Linux (linux-chromeos)

Chromium on Chromium OS uses Linux Chromium as a base, but adds a large number
of Chrome OS-specific features to the code. For example, the login UI, window
manager and system UI are part of the Chromium code base and built into the
chrome binary.

Fortunately, most Chromium changes that affect Chromium OS can be built and
tested on a Linux workstation. This build is called "linux-chromeos". In this
configuration most system services (like the power manager, bluetooth daemon,
etc.) are stubbed out. The entire system UI runs in a single X11 window on your
desktop.

You can test sign-in/sync in this mode by adding the --login-manager flag, see
the [Login notes](#Login-notes) section.

### Building and running Chromium with Chromium OS UI on your local machine

Run the following in your chromium checkout:

    $ gn gen out/Default --args='target_os="chromeos"'
    $ autoninja -C out/Default chrome
    $ out/Default/chrome --use-system-clipboard

(`autoninja` is a wrapper that automatically provides optimal values for the
arguments passed to `ninja`).

Some additional options you may wish to set by passing in `--args` to `gn gen`
or running `gn args out/Default`:

    # Googlers: Reclient is a distributed compiler service.
    use_remoteexec = true

    is_component_build = true  # Links faster.
    is_debug = false           # Release build, runs faster.
    dcheck_always_on = true    # Enables DCHECK despite release build.
    enable_nacl = false        # Skips native client build, compiles faster.

    # Builds Chrome instead of Chromium. This requires a src-internal
    # checkout. Adds internal features and branded art assets.
    is_chrome_branded = true

    # Enables many optimizations, leading to much slower compiles, links,
    # and no runtime stack traces.
    #
    # Note: not compatible with `is_component_build = true`.
    is_official_build = true

NOTE: You may wish to replace 'Default' with something like 'Cros' if
you switch back and forth between Linux and Chromium OS builds, or 'Debug'
if you want to differentiate between Debug and Release builds (see below).

See [GN Build Configuration](https://www.chromium.org/developers/gn-build-configuration)
for more information about configuring your build.

You can also build and run test targets like `unit_tests`, `browser_tests`, etc.

### Flags

Some useful flags:

*    `--ash-debug-shortcuts`: Enable shortcuts such as Ctl+Alt+Shift+T to toggle
     tablet mode.
*    `--ash-host-window-bounds="0+0-800x600,800+0-800x600"`: Specify one or more
     virtual screens, by display position and size.
*    `--enable-features=Feature1,OtherFeature2`: Enable specified features.
     Features are often listed in chrome://flags, or in source files such as
     [chrome_features.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/common/chrome_features.cc)
     or [ash_features.cc](https://source.chromium.org/chromium/chromium/src/+/main:ash/constants/ash_features.cc).
     Note that changing values in chrome://flags does not work for
     linux-chromeos, and this flag must be used.
*    `--enable-ui-devtools[=9223]`: Allow debugging of the system UI through
     devtools either within linux-chromeos at chrome://inspect, or from a remote
     browser at
     devtools://devtools/bundled/devtools_app.html?uiDevTools=true&ws=127.0.0.1:9223/0
*    `--remote-debugging-port=9222`: Allow debugging through devtools at
     http://localhost:9222
*    `--use-system-clipboard`: Integrate clipboard with the host X11 system.

### Login notes

By default this build signs in with a stub user. To specify a real user:

*   For first run, add the following options to chrome's command line:
    `--user-data-dir=/tmp/chrome --login-manager`
*   Go through the out-of-the-box UX and sign in with a real Gmail account.
*   For subsequent runs, if you want to skip the login manager page, add:
    `--user-data-dir=/tmp/chrome --login-user=username@gmail.com
    --login-profile=username@gmail.com-hash`. It's also fine to just keep
    --login-manager instead.
*   To run in guest mode instantly, add:
    `--user-data-dir=/tmp/chrome --bwsi --incognito --login-user='$guest'
    --login-profile=user`

Signing in as a specific user is useful for debugging features like sync
that require a logged in user.

### Graphics notes

The Chromium OS build requires a functioning GL so if you plan on
testing it through Chromium Remote Desktop you might face drawing
problems (e.g. Aura window not painting anything). Possible remedies:

*   `--ui-enable-software-compositing --ui-disable-threaded-compositing`
*   `--use-gl=angle --use-angle=swiftshader`, but it's slow.

To more closely match the UI used on devices, you can install fonts used
by Chrome OS, such as Roboto, on your Linux distro.

## Chromium OS Device (Simple Chrome)

This configuration allows you to build a fully functional Chrome for a real
Chrome OS device or VM. Since Chrome OS uses a different toolchain for each
device model, you'll first need to know the name of the model (or "board") you
want to build for. For most boards, `amd64-generic` and `arm-generic` will
produce a functional binary, though it won't be optimized and may be missing
functionality.

### Additional gclient setup

Each board has its own toolchain and misc. build dependencies. To fetch these,
list the board under the `"cros_boards"` gclient custom var. If you were using
the `amd64-generic` board, your `.gclient` file would look like:
```
solutions = [
  {
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "name": "src",
    "custom_deps": {},
    "custom_vars" : {
        "cros_boards": "amd64-generic",
    },
  },
]
target_os = ["chromeos"]
```
Once your .gclient file is updated, you will need to run `gclient sync` again
to fetch the toolchain.

NOTE:
 - If you'd like a VM image additionally downloaded for the board, add it to the
   `"cros_boards_with_qemu_images"` gclient custom var. That var downloads the
   SDK along with a VM image. `cros_boards` downloads only the SDK.
 - If you'd like to fetch multiple boards, add a `:` between each board in the
   gclient var. For example: `"cros_boards": "amd64-generic:arm-generic"`.

### Building for the board

After the needed toolchain has been downloaded for your ${BOARD}, a build dir
will have been conveniently created for you at `out_$BOARD/Release`, which can
then be used to build Chrome. For the `amd64-generic` board, this would
look like:

    $ gn gen out_amd64-generic/Release
    $ autoninja -C out_$BOARD/Release chrome

Or if you prefer to use your own build dir, simply add the following line to the
top of your GN args: `import("//build/args/chromeos/amd64-generic.gni")`. eg:

    $ gn gen out/Default --args='import("//build/args/chromeos/amd64-generic.gni")'
    $ autoninja -C out/Default chrome

That will produce a Chrome OS build of Chrome very similar to what is shipped
for that device. You can also supply additional args or even overwrite ones
supplied in the imported .gni file after the `import()` line.

### Additional notes

For more information (like copying the locally-built Chrome to a device, or
running Tast tests), consult Simple Chrome's
[full documentation](https://chromium.googlesource.com/chromiumos/docs/+/main/simple_chrome_workflow.md).
