# Checking out and building Chromium for Mac

There are instructions for other platforms linked from the
[get the code](get_the_code.md) page.

## Instructions for Google Employees

Are you a Google employee? See
[go/building-chrome](https://goto.google.com/building-chrome) instead.

[TOC]

## System requirements

*   A Mac, Intel or Arm.
    ([More details about Arm Macs](https://chromium.googlesource.com/chromium/src.git/+/main/docs/mac_arm64.md).)
*   [Xcode](https://developer.apple.com/xcode/). Xcode comes with...
*   The macOS SDK. Run

    ```shell
    $ ls `xcode-select -p`/Platforms/MacOSX.platform/Developer/SDKs
    ```

    to check whether you have it, and what version you have.
    `mac_sdk_official_version` in [mac_sdk.gni](../build/config/mac/mac_sdk.gni)
    is the SDK version used on all the bots and for
    [official builds](https://source.chromium.org/search?q=MAC_BINARIES_LABEL&ss=chromium),
    so that version is guaranteed to work. Building with a newer SDK usually
    works too (please fix or file a bug if it doesn't).

    Building with an older SDK might also work, but if it doesn't then we won't
    accept changes for making it work.

    The easiest way to get the newest SDK is to use the newest version of Xcode,
    which often requires using the newest version of macOS. We don't use Xcode
    itself much, so if you're know what you're doing, you can likely get the
    build working with an older version of macOS as long as you get a new
    version of the macOS SDK on it.
*   An APFS-formatted volume (this is the default format for macOS volumes).

## Install `depot_tools`

Clone the `depot_tools` repository:

```shell
$ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```

Add `depot_tools` to the end of your PATH (you will probably want to put this in
your `~/.bash_profile` or `~/.zshrc`). Assuming you cloned `depot_tools` to
`/path/to/depot_tools` (note: you **must** use the absolute path or Python will
not be able to find infra tools):

```shell
$ export PATH="$PATH:/path/to/depot_tools"
```

## Get the code

Create a `chromium` directory for the checkout and change to it (you can call
this whatever you like and put it wherever you like, as long as the full path
has no spaces):

```shell
$ mkdir chromium && cd chromium
```

Run the `fetch` tool from `depot_tools` to check out the code and its
dependencies.

```shell
$ caffeinate fetch chromium
```

Running the `fetch` with `caffeinate` is optional, but it will prevent the
system from sleeping for the duration of the `fetch` command, which may run for
a considerable amount of time.

If you don't need the full repo history, you can save time by using
`fetch --no-history chromium`. You can call `git fetch --unshallow` to retrieve
the full history later.

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

Chromium uses [Ninja](https://ninja-build.org) as its main build tool along with
a tool called [GN](https://gn.googlesource.com/gn/+/main/docs/quick_start.md)
to generate `.ninja` files. You can create any number of *build directories*
with different configurations. To create a build directory:

```shell
$ gn gen out/Default
```

* You only have to run this once for each new build directory, Ninja will
  update the build files as needed.
* You can replace `Default` with another name, but
  it should be a subdirectory of `out`.
* For other build arguments, including release settings, see [GN build
  configuration](https://www.chromium.org/developers/gn-build-configuration).
  The default will be a debug component build matching the current host
  operating system and CPU.
* For more info on GN, run `gn help` on the command line or read the
  [quick start guide](https://gn.googlesource.com/gn/+/main/docs/quick_start.md).
* Building Chromium for arm Macs requires [additional setup](mac_arm64.md).


### Faster builds

Full rebuilds are about the same speed in Debug and Release, but linking is a
lot faster in Release builds.

Put

```
is_debug = false
```

in your `args.gn` to do a release build.

Put

```
is_component_build = true
```

in your `args.gn` to build many small dylibs instead of a single large
executable. This makes incremental builds much faster, at the cost of producing
a binary that opens less quickly. Component builds work in both debug and
release.

Put

```
symbol_level = 0
```

in your args.gn to disable debug symbols altogether.  This makes both full
rebuilds and linking faster (at the cost of not getting symbolized backtraces
in gdb).

#### Use Reclient

In addition, Google employees should use Reclient, a distributed compilation system.
Detailed information is available internally but the relevant gn arg is:
* `use_remoteexec = true`

Google employees can visit
[go/building-chrome-mac#using-remote-execution](https://goto.google.com/building-chrome-mac#using-remote-execution)
for more information. For external contributors, Reclient does not support Mac
builds.

#### CCache

You might also want to [install ccache](ccache_mac.md) to speed up the build.

## Build Chromium

Build Chromium (the "chrome" target) with Ninja using the command:

```shell
$ autoninja -C out/Default chrome
```

(`autoninja` is a wrapper that automatically provides optimal values for the
arguments passed to `ninja`.)

You can get a list of all of the other build targets from GN by running `gn ls
out/Default` from the command line. To compile one, pass the GN label to Ninja
with no preceding "//" (so, for `//chrome/test:unit_tests` use `autoninja -C
out/Default chrome/test:unit_tests`).

## Run Chromium

Once it is built, you can simply run the browser:

```shell
$ out/Default/Chromium.app/Contents/MacOS/Chromium
```

## Avoiding system permissions dialogs after each build

Every time you start a new developer build, you may get two system dialogs:
`Chromium wants to use your confidential information stored in "Chromium Safe
Storage" in your keychain.`, and `Do you want the application "Chromium.app" to
accept incoming network connections?`.

To avoid them, you can run Chromium with these command-line flags (but of
course beware that they will change the behavior of certain subsystems):

```shell
--use-mock-keychain --disable-features=DialMediaRouteProvider
```

## Build and run test targets

Tests are split into multiple test targets based on their type and where they
exist in the directory structure. To see what target a given unit test or
browser test file corresponds to, the following command can be used:

```shell
$ gn refs out/Default --testonly=true --type=executable --all chrome/browser/ui/browser_list_unittest.cc
//chrome/test:unit_tests
```

In the example above, the target is unit_tests. The unit_tests binary can be
built by running the following command:

```shell
$ autoninja -C out/Default unit_tests
```

You can run the tests by running the unit_tests binary. You can also limit which
tests are run using the `--gtest_filter` arg, e.g.:

```shell
$ out/Default/unit_tests --gtest_filter="BrowserListUnitTest.*"
```

You can find out more about GoogleTest at its
[GitHub page](https://github.com/google/googletest).

## Debugging

Good debugging tips can be found [here](mac/debugging.md).

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

### Using Xcode-Ninja Hybrid

While using Xcode is unsupported, GN supports a hybrid approach of using Ninja
for building, but Xcode for editing and driving compilation.  Xcode is still
slow, but it runs fairly well even **with indexing enabled**.  Most people
build in the Terminal and write code with a text editor, though.

With hybrid builds, compilation is still handled by Ninja, and can be run from
the command line (e.g. `autoninja -C out/gn chrome`) or by choosing the `chrome`
target in the hybrid project and choosing Build.

To use Xcode-Ninja Hybrid pass `--ide=xcode` to `gn gen`:

```shell
$ gn gen out/gn --ide=xcode
```

Open it:

```shell
$ open out/gn/all.xcodeproj
```

You may run into a problem where http://YES is opened as a new tab every time
you launch Chrome. To fix this, open the scheme editor for the Run scheme,
choose the Options tab, and uncheck "Allow debugging when using document
Versions Browser". When this option is checked, Xcode adds
`--NSDocumentRevisionsDebugMode YES` to the launch arguments, and the `YES`
gets interpreted as a URL to open.

If you have problems building, join us in `#chromium` on `irc.freenode.net` and
ask there. Be sure that the
[waterfall](https://build.chromium.org/buildbot/waterfall/) is green and the
tree is open before checking out. This will increase your chances of success.

### Improving performance of git commands

#### Increase the vnode cache size

`git status` is used frequently to determine the status of your checkout.  Due
to the large number of files in Chromium's checkout, `git status` performance
can be quite variable.  Increasing the system's vnode cache appears to help. By
default, this command:

```shell
$ sysctl -a | egrep 'kern\..*vnodes'
```

Outputs `kern.maxvnodes: 263168` (263168 is 257 * 1024).  To increase this
setting:

```shell
$ sudo sysctl kern.maxvnodes=$((512*1024))
```

Higher values may be appropriate if you routinely move between different
Chromium checkouts.  This setting will reset on reboot.  To apply it at startup:

```shell
$ sudo tee /Library/LaunchDaemons/kern.maxvnodes.plist > /dev/null <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
  <dict>
    <key>Label</key>
      <string>kern.maxvnodes</string>
    <key>ProgramArguments</key>
      <array>
        <string>sysctl</string>
        <string>kern.maxvnodes=524288</string>
      </array>
    <key>RunAtLoad</key>
      <true/>
  </dict>
</plist>
EOF
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

### Exclude checkout from Spotlight indexing

Chromium's checkout contains a lot of files, and building generates many more.
Spotlight will try to index all of those files, and uses a lot of CPU time
doing so, especially during a build, which can slow things down.

To prevent the Chromium checkout from being indexed by Spotlight, open System
Preferences, go to "Spotlight" -> "Privacy" and add your Chromium checkout
directory to the list of excluded locations.
