# Checking out and building Chromium for iOS

There are instructions for other platforms linked from the
[get the code](../get_the_code.md) page.

## Instructions for Google Employees

Are you a Google employee? See
[go/building-chrome](https://goto.google.com/building-chrome) instead.

[TOC]

## System requirements

* A 64-bit Mac capable of running the required version of Xcode.
* [Xcode](https://developer.apple.com/xcode) 16.0 or higher.

Note: after installing Xcode, you need to launch it and to let it install
the iOS simulator. This is required as part of the build, see [this discussion](
https://groups.google.com/a/chromium.org/g/chromium-dev/c/98d6MyLoYHM/m/A_HyOGxPAgAJ)
on chromium-dev.

## Install `depot_tools`

Clone the `depot_tools` repository:

```shell
$ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```

Add `depot_tools` to the end of your PATH (you will probably want to put this
in your `~/.bashrc` or `~/.zshrc`). Assuming you cloned `depot_tools` to
`/path/to/depot_tools`:

```shell
$ export PATH="$PATH:/path/to/depot_tools"
```

## Get the code

Create a `chromium` directory for the checkout and change to it (you can call
this whatever you like and put it wherever you like, as
long as the full path has no spaces):

```shell
$ mkdir chromium && cd chromium
```

Run the `fetch` tool from `depot_tools` to check out the code and its
dependencies.

```shell
$ fetch ios
```

If you don't want the full repo history, you can save a lot of time by
adding the `--no-history` flag to `fetch`.

Expect the command to take 30 minutes on even a fast connection, and many
hours on slower ones.

When `fetch` completes, it will have created a hidden `.gclient` file and a
directory called `src` in the working directory. The remaining instructions
assume you have switched to the `src` directory:

```shell
$ cd src
```

*Optional*: You can also [install API
keys](https://www.chromium.org/developers/how-tos/api-keys) if you want your
build to talk to some Google services, but this is not necessary for most
development and testing purposes.

## Setting up the build

Since the iOS build is a bit more complicated than a desktop build, we provide
`ios/build/tools/setup-gn.py`, which will create four appropriately configured
build directories under `out` for Release and Debug device and simulator
builds, and generates an appropriate Xcode project (`out/build/all.xcodeproj`)
as well.

More information about [developing with Xcode](xcode_tips.md). *Xcode project
is an artifact, any changes made in the project itself will be ignored.*

You can customize the build by editing the file `$HOME/.setup-gn` (create it if
it does not exist).  Look at `src/ios/build/tools/setup-gn.config` for
available configuration options.

From this point, you can either build from Xcode or from the command line using
`autoninja`. `setup-gn.py` creates sub-directories named
`out/${configuration}-${platform}`, so for a `Debug` build for simulator use:

```shell
$ autoninja -C out/Debug-iphonesimulator gn_all
```

(`autoninja` is a wrapper that automatically provides optimal values for the
arguments passed to `ninja`.)

Note: The `setup-gn.py` script needs to run every time one of the `BUILD.gn`
files is updated (either by you or after rebasing). If you forget to run it,
the list of targets and files in the Xcode solution may be stale. You can run
the script directly or use either `gclient sync` or `gclient runhooks` which
will run `setup-gn.py` for you as part of the update hooks.

You can add a custom hook to `.gclient` file to configure `setup-gn.py` to
be run as part of `gclient runhooks`. In that case, your `.gclient` file
would look like this:

```
solutions = [
  {
    "name"        : "src",
    "url"         : "https://chromium.googlesource.com/chromium/src.git",
    "deps_file"   : "DEPS",
    "managed"     : False,
    "custom_deps" : {},
    "custom_vars" : {},
    "custom_hooks": [{
      "name": "setup_gn",
      "pattern": ".",
      "action": [
        "python3",
        "src/ios/build/tools/setup-gn.py",
      ]
    }],
    "safesync_url": "",
  },
]
target_os = ["ios"]
target_os_only = True
```

You can also follow the manual instructions on the
[Mac page](../mac_build_instructions.md), but make sure you set the
GN arg `target_os="ios"`.

### Faster builds

This section contains some things you can change to speed up your builds,
sorted so that the things that make the biggest difference are first.

#### Use Reclient

Google employees should use Reclient, a distributed compilation system. Detailed
information is available internally but the relevant gn arg is:
* `use_remoteexec = true`

Google employees can visit
[go/building-chrome-mac#using-remote-execution](https://goto.google.com/building-chrome-mac#using-remote-execution)
for more information. For external contributors, Reclient does not support iOS
builds.

## Building for device

To be able to build and run Chromium and the tests for devices, you need to
have an Apple developer account (a free one will work) and the appropriate
provisioning profiles, then configure the build to use them.

### Code signing identity

Please refer to the Apple documentation on how to get a code signing identity
and certificates. You can check that you have a code signing identity correctly
installed by running the following command.

```shell
$ xcrun security find-identity -v -p codesigning
  1) 0123456789ABCDEF0123456789ABCDEF01234567 "iPhone Developer: someone@example.com (XXXXXXXXXX)"
     1 valid identities found
```

If the command output says you have zero valid identities, then you do not
have a code signing identity installed and need to get one from Apple. If
you have more than one identity, the build system may select the wrong one
automatically, and you can use the `ios_code_signing_identity` gn variable
to control which one to use by setting it to the identity hash, e.g. to
`"0123456789ABCDEF0123456789ABCDEF01234567"`.

### Mobile provisioning profiles

Once you have the code signing identity, you need to decide on a prefix
for the application bundle identifier. This is controlled by the gn variable
`ios_app_bundle_id_prefix` and usually corresponds to a reversed domain name
(the default value is `"org.chromium"`).

You then need to request provisioning profiles from Apple for your devices
for the following bundle identifiers to build and run Chromium with these
application extensions:

-   `${prefix}.chrome.ios.dev`
-   `${prefix}.chrome.ios.dev.ContentTodayExtension`
-   `${prefix}.chrome.ios.dev.CredentialProviderExtension`
-   `${prefix}.chrome.ios.dev.IntentsExtension`
-   `${prefix}.chrome.ios.dev.OpenExtension`
-   `${prefix}.chrome.ios.dev.ShareExtension`
-   `${prefix}.chrome.ios.dev.TodayExtension`
-   `${prefix}.chrome.ios.dev.WidgetKitExtension`

All these certificates need to have the "App Groups"
(`com.apple.security.application-groups`) capability enabled for
the following groups:

-   `group.${prefix}.chrome`
-   `group.${prefix}.common`

The `group.${prefix}.chrome` is only shared by Chromium and its extensions
to share files and configurations while the `group.${prefix}.common` is shared
with Chromium and other applications from the same organisation and can be used
to send commands to Chromium.

`${prefix}.chrome.ios.dev` and
`${prefix}.chrome.ios.dev.CredentialProviderExtension` need the AutoFill
Credential Provider Entitlement, which corresponds to the key
`com.apple.developer.authentication-services.autofill-credential-provider`.

`${prefix}.chrome.ios.dev` additionally needs the
`com.apple.developer.kernel.extended-virtual-addressing` entitlement when
running on a real device.

### Mobile provisioning profiles for tests

In addition to that, you need a different provisioning profile for each
test application. Those provisioning profile will have a bundle identifier
matching the following pattern `${prefix}.gtest.${test-suite-name}` where
`${test-suite-name}` is the name of the test suite with underscores changed
to dashes (e.g. `base_unittests` app will use `${prefix}.gtest.base-unittests`
as bundle identifier).

To be able to run the EarlGrey tests on a device, you'll need two provisioning
profiles for EarlGrey and OCHamcrest frameworks:

-   `${prefix}.test.OCHamcrest`
-   `${prefix}.test.EarlGrey`

In addition to that, then you'll need one additional provisioning profile for
the XCTest module too. It must match the pattern:
`${prefix}.gtest.${test-suite-name}-module`.

### Other applications

Other applications like `ios_web_shell` usually will require mobile provisioning
profiles with bundle identifiers that may usually match the following pattern
`${prefix}.${application-name}` and may require specific capabilities.

Generally, if the mobile provisioning profile is missing then the code signing
step will fail and will print the bundle identifier of the bundle that could not
be signed on the command line, e.g.:

```shell
$ autoninja -C out/Debug-iphoneos ios_web_shell
ninja: Entering directory `out/Debug-iphoneos'
FAILED: ios_web_shell.app/ios_web_shell ios_web_shell.app/_CodeSignature/CodeResources ios_web_shell.app/embedded.mobileprovision
python ../../build/config/apple/codesign.py code-sign-bundle -t=iphoneos -i=0123456789ABCDEF0123456789ABCDEF01234567 -e=../../build/config/ios/entitlements.plist -b=obj/ios/web/shell/ios_web_shell ios_web_shell.app
Error: no mobile provisioning profile found for "org.chromium.ios-web-shell".
ninja: build stopped: subcommand failed.
```

Here, the build is failing because there are no mobile provisioning profiles
installed that could sign the `ios_web_shell.app` bundle with the identity
`0123456789ABCDEF0123456789ABCDEF01234567`. To fix the build, you'll need to
request such a mobile provisioning profile from Apple.

You can inspect the file passed via the `-e` flag to the `codesign.py` script
to check which capabilities are required for the mobile provisioning profile
(e.g. `src/build/config/ios/entitlements.plist` for the above build error,
remember that the paths are relative to the build directory, not to the source
directory).

If the required capabilities are not enabled on the mobile provisioning profile,
then it will be impossible to install the application on a device (Xcode will
display an error stating that "The application was signed with invalid
entitlements").

## Building Blink for iOS

The iOS build supports compiling the blink web platform. To compile blink
set a gn arg in your `.setup-gn` file. Note the blink web platform is
experimental code and should only be used for analysis.

```
[gn_args]
use_blink = true
```
Note that only certain targets support blink. `content_shell` being the
most useful.

```shell
$ autoninja -C out/Debug-iphonesimulator content_shell
```

## Running apps from the command line

Any target that is built and runs on the bots (see [below](#Troubleshooting))
should run successfully in a local build. To run in the simulator from the
command line, you can use `iossim`. For example, to run a debug build of
`Chromium`:

```shell
$ out/Debug-iphonesimulator/iossim -i out/Debug-iphonesimulator/Chromium.app
```

From Xcode 9 on, `iossim` no longer automatically launches the Simulator. This must now
be done manually from within Xcode (`Xcode > Open Developer Tool > Simulator`), and
also must be done *after* running `iossim`.

### Passing arguments

Arguments needed to be passed to the test application through `iossim`, such as
`--gtest_filter=SomeTest.FooBar` should be passed through the `-c` flag:

```shell
$ out/Debug-iphonesimulator/iossim -i \
    -c "--gtest_filter=SomeTest.FooBar --gtest_repeat=3" \
    out/Debug-iphonesimulator/base_unittests.app
```

### Running EarlGrey tests

EarlGrey tests are run differently than other test targets, as there is an
XCTest bundle that is injected into the target application. Therefore you must
also pass in the test bundle:

```shell
$ out/Debug-iphonesimulator/iossim -i \
    out/Debug-iphonesimulator/ios_chrome_ui_egtests.app \
    out/Debug-iphonesimulator/ios_chrome_ui_egtests.app/PlugIns/ios_chrome_ui_egtests_module.xctest
```

### Running Web Tests on Blink for iOS

The current Blink for iOS only supports running Web Tests on the simulator
environment now. Before you run the web tests, you need to build the blink_tests
target to get content_shell and all of the other needed binaries for the
simulator test environment.

```shell
$ autoninja -C out/Debug-iphonesimulator blink_tests
```

When the blink_tests target is complete you can then run the test runner script
(third_party/blink/tools/run_web_tests.py) as below. See [Web Tests](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/web_tests.md) document
for more information.

```shell
$ third_party/blink/tools/run_web_tests.py -t Debug-iphonesimulator \
    --platform ios
```

### Running on specific simulator

By default, `iossim` will pick an arbitrary simulator to run the tests. If
you want to run them on a specific simulator, you can use `-d` to pick the
simulated device and `-s` to select the iOS version.

For example, to run the tests on a simulated iPhone 6s running iOS 10.0,
you would invoke `iossim` like this.

```shell
$ out/Debug-iphonesimulator/iossim -i -d 'iPhone 6s' -s '10.0' \
    out/Debug-iphonesimulator/base_unittests.app
```

Please note that by default only a subset of simulator devices are installed
with Xcode. You may have to install additional simulators in Xcode (or even
an older version of Xcode) to be able to run on a specific configuration.

Go to "Preferences > Components" tab in Xcode to install other simulator images
(this is the location the setting is in Xcode 9.2; it may be different in other
version of the tool).

### Remote debugging with DevTools (on Blink for iOS)

Developers are able to remotely use DevTools in a host machine (e.g. Mac) and
inspect `content_shell` for development.

On the simulator, one just needs to pass the `--remote-debugging-port=9222`
argument for `content_shell` and in the host machine access it via
`chrome://inspect`. It is possible to change the default port listening (9222)
and configure another one via the  "Configure…" button and then "Target
discovery settings" dialog.

To use DevTools in the remote device it is necessary to also pass the remote
debugging address argument to `content-shell` so any address could bind for
debugging: ` --remote-debugging-address=0.0.0.0 --remote-debugging-port=9222`.
Then in the host machine one needs to configure the IP address of the device in
the "Target discovery settings" dialog e.g. `192.168.0.102:9222`.

## Update your checkout

To update an existing checkout, you can run

```shell
$ git rebase-update
$ gclient sync
```

The first command updates the primary Chromium source repository and rebases
any of your local branches on top of tip-of-tree (aka the Git branch
`origin/main`). If you don't want to use this script, you can also just use
`git pull` or other common Git commands to update the repo.

The second command syncs dependencies to the appropriate versions and re-runs
hooks as needed.

## Tips, tricks, and troubleshooting

Remember that the XCode project you interact with while working on Chromium is a
build artifact, generated from the `BUILD.gn` files. Do not use it to add new
files; instead see the procedures for [working with
files](working_with_files.md).

If you have problems building, join us in `#chromium` on `irc.freenode.net` and
ask there. As mentioned above, be sure that the
[waterfall](https://build.chromium.org/buildbot/waterfall/) is green and the tree
is open before checking out. This will increase your chances of success.

### Debugging

To help with deterministic builds, and to work with reclient, the path to source
files in debugging symbols are relative to source directory. To allow Xcode
to find the source files, you need to ensure to have an `~/.lldbinit-Xcode`
file with the following lines into it (substitute {SRC} for your actual path
to the root of Chromium's sources):

```
script sys.path[:0] = ['{SRC}/tools/lldb']
script import lldbinit
```

This will also allow you to see the content of some of Chromium types in the
debugger like `std::u16string`, ... If you want to use `lldb` directly, name
the file `~/.lldbinit` instead of `~/.lldbinit-Xcode`.

Note: if you are using `ios/build/tools/setup-gn.py` to generate the Xcode
project, the script also generate an `.lldbinit` file next to the project and
configure Xcode to use that file instead of the global one.

### Changing the version of Xcode

To change the version of Xcode used to build Chromium on iOS, please follow
the steps below:

1.  Launch the new version of Xcode.app.

    This is required as Xcode may need to install some components into
    the system before the new version can be used from the command-line.

1.  Reboot your computer.

    This is required as some of Xcode components are daemons that are not
    automatically stopped when updating Xcode, and command-line tools will
    fail if the daemon version is incompatible (usually `actool` fails).

1.  Run `gn gen`.

    This is required as the `ninja` files generated by `gn` encodes some
    information about Xcode (notably the path to the SDK, ...) that will
    change with each version. It is not possible to have `ninja` re-run
    `gn gen` automatically when those changes unfortunately.

    If you have a downstream chekout, run `gclient runhooks` instead of
    `gn gen` as it will ensure that `gn gen` will be run automatically
    for all possible combination of target and configuration from within
    Xcode.

If you skip some of those steps, the build may occasionally succeed, but
it has been observed in the past that those steps are required in the
vast majority of the situation. Please save yourself some painful build
debugging and follow them.

If you use `xcode-select` to switch between multiple version of Xcode,
you will have to follow the same steps.

### Improving performance of git commands

#### Increase the vnode cache size

`git status` is used frequently to determine the status of your checkout.  Due
to the large number of files in Chromium's checkout, `git status` performance
can be quite variable.  Increasing the system's vnode cache appears to help.
By default, this command:

```shell
$ sysctl -a | egrep kern\..*vnodes
```

Outputs `kern.maxvnodes: 263168` (263168 is 257 * 1024).  To increase this
setting:

```shell
$ sudo sysctl kern.maxvnodes=$((512*1024))
```

Higher values may be appropriate if you routinely move between different
Chromium checkouts.  This setting will reset on reboot, the startup setting can
be set in `/etc/sysctl.conf`:

```shell
$ echo kern.maxvnodes=$((512*1024)) | sudo tee -a /etc/sysctl.conf
```

Or edit the file directly.

#### Configure git to use an untracked cache

Try running

```shell
$ git update-index --test-untracked-cache
```

If the output ends with `OK`, then the following may also improve performance of
`git status`:

```shell
$ git config core.untrackedCache true
```

#### Configure git to use fsmonitor

You can significantly speed up git by using [fsmonitor.](https://github.blog/2022-06-29-improve-git-monorepo-performance-with-a-file-system-monitor/)
You should enable fsmonitor in large repos, such as Chromium and v8. Enabling
it globally will launch many processes and probably isn't worthwhile. Be sure
you have at least version 2.43 (fsmonitor on the Mac is broken before then). The
command to enable fsmonitor in the current repo is:

```shell
$ git config core.fsmonitor true
```

### Xcode license agreement

If you're getting the error

> Agreeing to the Xcode/iOS license requires admin privileges, please re-run as
> root via sudo.

the Xcode license hasn't been accepted yet which (contrary to the message) any
user can do by running:

```shell
$ xcodebuild -license
```

Only accepting for all users of the machine requires root:

```shell
$ sudo xcodebuild -license
```
