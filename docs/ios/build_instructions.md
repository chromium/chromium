# Checking out and building Chromium for iOS

There are instructions for other platforms linked from the
[get the code](../get_the_code.md) page.

## Instructions for Google Employees

Are you a Google employee? See
[go/building-chrome](https://goto.google.com/building-chrome) instead.

[TOC]

## System requirements

* A 64-bit Mac running 10.12.6 or later.
* [Xcode](https://developer.apple.com/xcode) 10.0+.
* The current version of the JDK (required for the Closure compiler).

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
builds, and generates an appropriate Xcode workspace
(`out/build/all.xcworkspace`) as well.

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

Note: you need to run `setup-gn.py` script every time one of the `BUILD.gn`
file is updated (either by you or after rebasing). If you forget to run it,
the list of targets and files in the Xcode solution may be stale.

You can also follow the manual instructions on the
[Mac page](../mac_build_instructions.md), but make sure you set the
GN arg `target_os="ios"`.

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

-   `${prefix}.chrome.ios.herebedragons`
-   `${prefix}.chrome.ios.herebedragons.ShareExtension`
-   `${prefix}.chrome.ios.herebedragons.TodayExtension`
-   `${prefix}.chrome.ios.herebedragons.SearchTodayExtension`

All these certificates need to have the "App Groups"
(`com.apple.security.application-groups`) capability enabled for
the following groups:

-   `group.${prefix}.chrome`
-   `group.${prefix}.common`

The `group.${prefix}.chrome` is only shared by Chromium and its extensions
to share files and configurations while the `group.${prefix}.common` is shared
with Chromium and other applications from the same organisation and can be used
to send commands to Chromium.

### Mobile provisioning profiles for tests

In addition to that, you need a different provisioning profile for each
test application. Those provisioning profile will have a bundle identifier
matching the following pattern `${prefix}.gtest.${test-suite-name}` where
`${test-suite-name}` is the name of the test suite with underscores changed
to dashes (e.g. `base_unittests` app will use `${prefix}.gest.base-unittests`
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
python ../../build/config/ios/codesign.py code-sign-bundle -t=iphoneos -i=0123456789ABCDEF0123456789ABCDEF01234567 -e=../../build/config/ios/entitlements.plist -b=obj/ios/web/shell/ios_web_shell ios_web_shell.app
Error: no mobile provisioning profile found for "org.chromium.ios-web-shell".
ninja: build stopped: subcommand failed.
```

Here, the build is failing because there are no mobile provisioning profiles
installed that could sign the `ios_web_shell.app` bundle with the identity
`0123456789ABCDEF0123456789ABCDEF01234567`. To fix the build, you'll need to
request such a mobile provisioning profile from Apple.

You can inspect the file passed via the `-e` flag to the `codesign.py` script
to check which capabilites are required for the mobile provisioning profile
(e.g. `src/build/config/ios/entitlements.plist` for the above build error,
remember that the paths are relative to the build directory, not to the source
directory).

If the required capabilities are not enabled on the mobile provisioning profile,
then it will be impossible to install the application on a device (Xcode will
display an error stating that "The application was signed with invalid
entitlements").

## Running apps from the commandline

Any target that is built and runs on the bots (see [below](#Troubleshooting))
should run successfully in a local build. To run in the simulator from the
command line, you can use `iossim`. For example, to run a debug build of
`Chromium`:

```shell
$ out/Debug-iphonesimulator/iossim out/Debug-iphonesimulator/Chromium.app
```

With Xcode 9, `iossim` no longer automatically launches the Simulator. This must now
be done manually from within Xcode (`Xcode > Open Developer Tool > Simulator`), and
also must be done *after* running `iossim`.

### Passing arguments

Arguments needed to be passed to the test application through `iossim`, such as
`--gtest_filter=SomeTest.FooBar` should be passed through the `-c` flag:

```shell
$ out/Debug-iphonesimulator/iossim \
    -c "--gtest_filter=SomeTest.FooBar --gtest_repeat=3" \
    out/Debug-iphonesimulator/base_unittests.app
```

### Running EarlGrey tests

EarlGrey tests are run differently than other test targets, as there is an
XCTest bundle that is injected into the target application. Therefore you must
also pass in the test bundle:

```shell
$ out/Debug-iphonesimulator/iossim \
    out/Debug-iphonesimulator/ios_chrome_ui_egtests.app \
    out/Debug-iphonesimulator/ios_chrome_ui_egtests.app/PlugIns/ios_chrome_ui_egtests_module.xctest
```

### Running on specific simulator

By default, `iossim` will pick an arbitrary simulator to run the tests. If
you want to run them on a specific simulator, you can use `-d` to pick the
simulated device and `-s` to select the iOS version.

For example, to run the tests on a simulated iPhone 6s running iOS 10.0,
you would invoke `iossim` like this.

```shell
$ out/Debug-iphonesimulator/iossim -d 'iPhone 6s' -s '10.0' \
    out/Debug-iphonesimulator/base_unittests.app
```

Please note that by default only a subset of simulator devices are installed
with Xcode. You may have to install additional simulators in Xcode (or even
an older version of Xcode) to be able to run on a specific configuration.

Go to "Preferences > Components" tab in Xcode to install other simulator images
(this is the location the setting is in Xcode 9.2; it may be different in other
version of the tool).

## Update your checkout

To update an existing checkout, you can run

```shell
$ git rebase-update
$ gclient sync
```

The first command updates the primary Chromium source repository and rebases
any of your local branches on top of tip-of-tree (aka the Git branch
`origin/master`). If you don't want to use this script, you can also just use
`git pull` or other common Git commands to update the repo.

The second command syncs dependencies to the appropriate versions and re-runs
hooks as needed.

## Tips, tricks, and troubleshooting

If you have problems building, join us in `#chromium` on `irc.freenode.net` and
ask there. As mentioned above, be sure that the
[waterfall](https://build.chromium.org/buildbot/waterfall/) is green and the tree
is open before checking out. This will increase your chances of success.

### Improving performance of `git status`

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

If `git --version` reports 2.8 or higher, try running

```shell
$ git update-index --test-untracked-cache
```

If the output ends with `OK`, then the following may also improve performance of
`git status`:

```shell
$ git config core.untrackedCache true
```

If `git --version` reports 2.6 or higher, but below 2.8, you can instead run

```shell
$ git update-index --untracked-cache
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
