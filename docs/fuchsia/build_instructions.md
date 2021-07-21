# Checking out and building on Fuchsia

***Note that the Fuchsia port is in the early stages, and things are likely to
frequently be broken. Try #cr-fuchsia on Freenode or Slack if something seems
awry.***

There are instructions for other platforms linked from the
[get the code](get_the_code.md) page.

[TOC]

## System requirements

*   A 64-bit Intel machine with at least 8GB of RAM. More than 16GB is highly
    recommended.
*   At least 100GB of free disk space.
*   You must have Git and Python installed already.

Most development is done on Ubuntu. Mac build is supported on a best-effort
basis.

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
this whatever you like and put it wherever you like, as long as the full path
has no spaces):

```shell
$ mkdir ~/chromium && cd ~/chromium
```

Run the `fetch` tool from depot_tools to check out the code and its
dependencies.

```shell
$ fetch --nohooks chromium
```

Expect the command to take 30 minutes on even a fast connection, and many
hours on slower ones.

If you've already installed the build dependencies on the machine (from another
checkout, for example), you can omit the `--nohooks` flag and `fetch`
will automatically execute `gclient runhooks` at the end.

When `fetch` completes, it will have created a hidden `.gclient` file and a
directory called `src` in the working directory.

### Configure for building on Fuchsia

Edit `.gclient` to include (this is a list, so it could also include `android`,
etc. if necessary.)

```
target_os = ['fuchsia']
```

Note that this should be added as a top-level statement in the `.gclient` file,
not an entry inside the `solutions` dict. An example `.gclient` file would look
as follows:

```
solutions = [
  {
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "managed": False,
    "name": "src",
    "custom_deps": {},
    "custom_vars": {}
  }
]
target_os = ['fuchsia']
```

You will then need to run:

```shell
$ gclient sync
```

This makes sure the Fuchsia SDK is available in third\_party and keeps it up to
date.

The remaining instructions assume you have switched to the `src` directory:

```shell
$ cd src
```

### (Linux-only) Install any required host packages

Chromium relies on some platform packages to be present in order to build.
You can install the current set of required packages with:

```shell
$ build/install-build-deps.sh
```

Note that you need to do this only once, and thereafter only if new dependencies
are added - these will be announced to the chromium-dev@ group.

### Update your checkout

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
hooks as needed. `gclient sync` updates dependencies to the versions specified
in `DEPS`, so any time that file is modified (pulling, changing branches, etc.)
`gclient sync` should be run.

## (Mac-only) Download additional required Clang binaries

Go to [this page](https://chrome-infra-packages.appspot.com/p/fuchsia/clang/mac-amd64/+/)
and download the most recent build. Extract `bin/llvm-ar` to the clang folder
in Chromium:

```shell
$ unzip /path/to/clang.zip bin/llvm-ar -d ${CHROMIUM_SRC}/third_party/llvm-build/Release+Asserts
```

## Setting up the build

Chromium uses [Ninja](https://ninja-build.org) as its main build tool along with
a tool called [GN](https://gn.googlesource.com/gn/+/main/docs/quick_start.md)
to generate `.ninja` files. You can create any number of *build directories*
with different configurations. To create a build directory, run:

```shell
$ gn gen out/fuchsia --args="is_debug=false dcheck_always_on=true is_component_build=false target_os=\"fuchsia\""
```

You can also build for Debug, with `is_debug=true`, but since we don't currently
have any Debug build-bots, it may be more broken than Release.

`use_goma=true` is fine to use also if you're a Googler.

Architecture options are x64 (default) and arm64. This can be set with
`target_cpu=\"arm64\"`.

## Build

All targets included in the GN build should build successfully. You can also
build a specific binary, for example, base\_unittests:

```shell
$ autoninja -C out/fuchsia base_unittests
```

(`autoninja` is a wrapper that automatically provides optimal values for the
arguments passed to `ninja`.)

## Run

Once you've built a package, you'll want to run it!

### (Recommended)(Linux-only) Enable KVM acceleration

Under Linux, if your host and target CPU architectures are the same (e.g. you're
building for Fuchsia/x64 on a Linux/x64 host) then you can benefit from QEMU's
support for the KVM hypervisor:

1. Install the KVM module for your kernel, to get a /dev/kvm device.
2. Ensure that your system has a "kvm" group, and it owns /dev/kvm.
You can do that by installing the QEMU system common package:
```shell
$ sudo apt-get install qemu-system-common
```
3. Add users to the "kvm" group, and have them login again, to pick-up the new
group.
```shell
$ sudo adduser <user> kvm
$ exit
[log in again]
```

### Running test suites

There are three types of tests available to run on Fuchsia:

1. [Gtests](gtests.md)
2. [GPU integration tests](gpu_testing.md)
3. [Blink tests](web_tests.md)

Check the documentations to learn more about how to run these tests.
