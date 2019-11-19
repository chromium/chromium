# Chrome OS Build Instructions (Chromium OS on Linux)

Chromium on Chromium OS uses Linux Chromium as a base, but adds a large number
of features to the code. For example, the login UI, window manager and system UI
are part of the Chromium code base and built into the chrome binary.

Fortunately, most Chromium changes that affect Chromium OS can be built and
tested on a Linux workstation. This build is called "linux-chromeos". In this
configuration most system services (like the power manager, bluetooth daemon,
etc.) are stubbed out. The entire system UI runs in a single X11 window on your
desktop.

First, follow the [normal Linux build
instructions](https://chromium.googlesource.com/chromium/src/+/master/docs/linux_build_instructions.md)
as usual to get a Chromium checkout.

## Updating your gclient config

Chromium OS builds of Chromium require some additional build dependencies which
can be synced by adding `'chromeos'` to the `target_os` list in your `.gclient`
configuration. This file is located one level up from your Chromium checkout's
`src`.

If you don't already have a `target_os` line present, simply add this to the
end of the `.gclient` file:

    target_os = ['chromeos']

If you already have a `target_os` line present in your `.gclient file`, you can
simply append `'chromeos'` to the existing list there. For example:

    target_os = ['android', 'chromeos']

Once your `.gclient` file is updated, you will need to run `gclient sync` once
before proceeding with the rest of these instructions.

## Building and running Chromium with Chromium OS UI on your local machine

Run the following in your chromium checkout:

    $ gn gen out/Default --args='target_os="chromeos"'
    $ autoninja -C out/Default chrome
    $ out/Default/chrome

(`autoninja` is a wrapper that automatically provides optimal values for the
arguments passed to `ninja`).

Some additional options you may wish to set by passing in `--args` to `gn gen`
or running `gn args out/Default`:

    use_goma = true            # Googlers: Use build farm, compiles faster.
    is_component_build = true  # Links faster.
    is_debug = false           # Release build, runs faster.
    dcheck_always_on = true    # Enables DCHECK despite release build.
    enable_nacl = false        # Skips native client build, compiles faster.

    # Set the following true to create a Chrome (instead of Chromium) build.
    # This requires a src-internal checkout.
    is_chrome_branded = false  # Adds internal features and branded art assets.
    is_official_build = false  # Turns on many optimizations, slower build.

NOTE: You may wish to replace 'Default' with something like 'Cros' if
you switch back and forth between Linux and Chromium OS builds, or 'Debug'
if you want to differentiate between Debug and Release builds (see below).

See [GN Build Configuration](https://www.chromium.org/developers/gn-build-configuration)
for more information about configuring your build.

You can also build and run test targets like `unit_tests`, `browser_tests`, etc.

## Login notes

By default this build signs in with a stub user. To specify a real user:

*   For first run, add the following options to chrome's command line:
    `--user-data-dir=/tmp/chrome --login-manager`
*   Go through the out-of-the-box UX and sign in with a real Gmail account.
*   For subsequent runs, add:
    `--user-data-dir=/tmp/chrome --login-user=username@gmail.com
    --login-profile=username@gmail.com-hash`
*   To run in guest mode instantly, add:
    `--user-data-dir=/tmp/chrome --bwsi --incognito --login-user='$guest'
    --login-profile=user`

Signing in as a specific user is useful for debugging features like sync
that require a logged in user.

## Graphics notes

The Chromium OS build requires a functioning GL so if you plan on
testing it through Chromium Remote Desktop you might face drawing
problems (e.g. Aura window not painting anything). Possible remedies:

*   `--ui-enable-software-compositing --ui-disable-threaded-compositing`
*   `--use-gl=swiftshader`, but it's slow.

To more closely match the UI used on devices, you can install fonts used
by Chrome OS, such as Roboto, on your Linux distro.

## Compile Chromium for a Chromium OS device

See [Building Chromium for a Chromium OS device](https://chromium.googlesource.com/chromiumos/docs/+/master/simple_chrome_workflow.md)
for information about building and testing Chromium for Chromium OS.
