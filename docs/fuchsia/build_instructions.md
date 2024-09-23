# Checking out and building on Fuchsia

***If you have followed the instructions below and are still having trouble,
see [Contact information](README.md#contact-information).***

There are instructions for other platforms linked from the
[get the code](../get_the_code.md) page.

[TOC]

## System requirements

*   An x86-64 machine with at least 8GB of RAM. More than 16GB is highly
    recommended.
*   At least 100GB of free disk space.
*   You must have Git and Python installed already.

Most development is done on Ubuntu. Mac build is not supported.
If you already have a Chromium checkout, continue to the
[next section](#instructions-for-current-chromium-developers). Otherwise, skip
to the [following section](#instructions-for-new-chromium-developers). If you
are a Fuchsia developer, see also
[Working with the Fuchsia tree](#working-with-the-fuchsia-tree).

## Instructions for current Chromium developers

This section applies to you if you already have a Chromium checkout. You will
need to update it to install Fuchsia-specific dependencies.

1. Edit your `.gclient` to add `fuchsia` to the `target_os` list. The file
   should look similar to this:

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

2. Run `gclient sync`
3. Create a build directory:

   ```shell
   $ gn gen out/fuchsia --args="is_debug=false dcheck_always_on=true is_component_build=false target_os=\"fuchsia\""
   ```

   You can add many of the usual GN arguments like `use_remoteexec = true`. In
   particular, when working with devices, consider using `is_debug = false` and
   `is_component_build = false` since debug and component builds can drastically
   increase run time and used space.

Build the target as you would for any other platform:
```shell
$ autoninja out/fuchsia <target_name>
```

To run the tests in an emulator, see the [Run](#run) section.

## Instructions for new Chromium developers

### Install `depot_tools`

Clone the `depot_tools` repository:

```shell
$ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```

Add `depot_tools` to the end of your PATH (you will probably want to put this in
your `~/.bashrc` or `~/.zshrc`). Assuming you cloned `depot_tools` to
`/path/to/depot_tools`:

```shell
$ export PATH="$PATH:/path/to/depot_tools"
```

### Get the code

Create a `chromium` directory for the checkout and change to it (you can call
this whatever you like and put it wherever you like, as long as the full path
has no spaces):

```shell
$ mkdir ~/chromium && cd ~/chromium
```

Run the `fetch` tool from depot_tools to check out the code and its
dependencies. Depending on your needs, you can check out Chromium along with
all of its previous revisions, or you can just check out the latest trunk.
Omitting the history is much faster to download and requires much less disk
space. If you are checking out on a slow or metered Internet connection,
you should consider omitting history.

* **No Git version history - faster**
```shell
  $ fetch --nohooks --no-history chromium
  ```

*  **With Git version history - slower (up to 30m on fast connection)**
  ```shell
  $ fetch --nohooks chromium
  ```

If you've already installed the build dependencies on the machine (from another
checkout, for example), you can omit the `--nohooks` flag and `fetch` will
automatically execute `gclient runhooks` at the end.

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

The Fuchsia boot images (also called "SDK companion images") to check out are
specified by the `checkout_fuchsia_boot_images` variable. For instance, adding
`"checkout_fuchsia_boot_images": "qemu.x64,workstation.qemu-x64-release",` to
the `custom_vars` section of your `.gclient` file would allow you to check out
both images. The set of available images is listed in the
[DEPS file](https://source.chromium.org/chromium/chromium/src/+/main:DEPS).

Note: fxbug.dev/85552 tracks migration away from the legacy image names, like
`qemu.x64`, which is mapped to `terminal.x64-release` by the
[`update_images.py`](https://source.chromium.org/chromium/chromium/src/+/main:build/fuchsia/update_images.py)
helper script.

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

Chromium relies on some platform packages to be present in order to build. You
can install the current set of required packages with:

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

The first command updates the primary Chromium source repository and rebases any
of your local branches on top of tip-of-tree (aka the Git branch `origin/main`).
If you don't want to use this script, you can also just use `git pull` or other
common Git commands to update the repo.

The second command syncs dependencies to the appropriate versions and re-runs
hooks as needed. `gclient sync` updates dependencies to the versions specified
in `DEPS`, so any time that file is modified (pulling, changing branches, etc.)
`gclient sync` should be run.

### (Mac-only) Download additional required Clang binaries

Go to
[this page](https://chrome-infra-packages.appspot.com/p/fuchsia/clang/mac-amd64/+/)
and download the most recent build. Extract `bin/llvm-ar` to the clang folder in
Chromium:

```shell
$ unzip /path/to/clang.zip bin/llvm-ar -d ${CHROMIUM_SRC}/third_party/llvm-build/Release+Asserts
```

### Setting up the build

Chromium uses [Ninja](https://ninja-build.org) as its main build tool along with
a tool called [GN](https://gn.googlesource.com/gn/+/main/docs/quick_start.md) to
generate `.ninja` files. You can create any number of *build directories* with
different configurations. To create a build directory, run:

```shell
$ gn gen out/fuchsia --args="is_debug=false dcheck_always_on=true is_component_build=false target_os=\"fuchsia\""
```

You can also build for Debug, with `is_debug=true`, but since we don't currently
have any Debug build-bots, it may be more broken than Release.

`use_remoteexec=true` is fine to use also if you're a Googler.

Architecture options are x64 (default) and arm64. This can be set with
`target_cpu=\"arm64\"`.

### Build

All targets included in the GN build should build successfully. You can also
build a specific binary, for example, base\_unittests:

```shell
$ autoninja -C out/fuchsia base_unittests
```

(`autoninja` is a wrapper that automatically provides optimal values for the
arguments passed to `ninja`.)

## Run

Once you've built a package, you'll want to run it!

### (Linux-only) Enable KVM acceleration (strongly recommended)

Under Linux, if your host and target CPU architectures are the same (e.g. you're
building for Fuchsia/x64 on a Linux/x64 host) then you can benefit from QEMU's
support for the KVM hypervisor:

1.  Install the KVM module for your kernel, to get a /dev/kvm device.
2.  Ensure that your system has a "kvm" group, and it owns /dev/kvm. You can do
    that by installing the QEMU system common package: `shell $ sudo apt-get
    install qemu-system-common`
3.  Add users to the "kvm" group, and have them login again, to pick-up the new
    group. `shell $ sudo adduser <user> kvm $ exit [log in again]`

### Running test suites

There are four types of tests available to run on Fuchsia:

1.  [Gtests](gtests.md)
2.  [GPU integration tests](gpu_testing.md)
3.  [Blink tests](web_tests.md)
4.  [Webpage tests](webpage_tests.md)

Check the documentations to learn more about how to run these tests.

Documentation for the underlying testing scripts work can be found
[here](test_scripts.md).

### Working with the Fuchsia tree

If you have a Fuchsia checkout and build, there are GN arguments in Chromium
that make working with both Fuchsia and Chromium checkouts easier.

* `default_fuchsia_out_dir`. Point this to an output
  directory in Fuchsia. For instance. `/path/to/src/fuchsia/out/qemu-x64`. This
  will automatically add the `--fuchsia-out-dir` flag to wrapper scripts.
* `default_fuchsia_device_node_name`. Set this to a Fuchsia device node name.
  This will automatically add the `--target-id` flag to most wrapper scripts.
* Finally, use the `-d` flag when running the <test_target_name> wrappers to
  execute them on an already running device or emulator, rather than starting an
  ephemeral emulator instance. This speeds up subsequent runs since the runner
  script does not need to wait for the emulator instance to boot and only
  differential changes are pushed to the device.
