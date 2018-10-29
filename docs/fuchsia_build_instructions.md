# Checking out and building on Fuchsia

***Note that the Fuchsia port is in the early stages, and things are likely to
frequently be broken. Try #cr-fuchsia on Freenode if something seems awry.***

There are instructions for other platforms linked from the
[get the code](get_the_code.md) page.

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
not an entry inside the `solutions` dict.

You will then need to run:

```shell
$ gclient runhooks
```

This makes sure the Fuchsia SDK is available in third\_party and keeps it up to
date.

The remaining instructions assume you have switched to the `src` directory:

```shell
$ cd src
```

### Update your checkout

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
a tool called [GN](https://gn.googlesource.com/gn/+/master/docs/quick_start.md)
to generate `.ninja` files. You can create any number of *build directories*
with different configurations. To create a build directory, run:

```shell
$ gn gen out/fuchsia --args="is_debug=false dcheck_always_on=true is_component_build=false target_os=\"fuchsia\""
```

You can also build for Debug, with `is_debug=true`, but since we don't currently
have any Debug build-bots, it may be more broken than Release.

`use_goma=true` is fine to use also if you're a Googler.

## Build

Currently, not all targets build on Fuchsia. You can build base\_unittests, for
example:

```shell
$ autoninja -C out/fuchsia base_unittests
```

(`autoninja` is a wrapper that automatically provides optimal values for the
arguments passed to `ninja`.)

## Run

Once it is built, you can run by:

```shell
$ out/fuchsia/bin/run_base_unittests
```

This packages the built binary and test data into a disk image, and runs a QEMU
instance from the Fuchsia SDK, outputting to the console.

Common gtest arguments such as `--gtest_filter=...` are supported by the run
script.

The run script also symbolizes backtraces.
