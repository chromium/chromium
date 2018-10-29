# Checking out and building Cast on Linux

**Note**: it is **not possible** to build a binary functionally
equivalent to a Chromecast. This is to build a single-page content
embedder with similar functionality to Cast products.

## Instructions for Google Employees

Are you a Google employee? See
[go/building-linux-cast](https://goto.google.com/building-linux-cast) instead.

[TOC]

## System requirements

*   A 64-bit Intel machine with at least 8GB of RAM. More than 16GB is highly
    recommended.
*   At least 100GB of free disk space.
*   You must have Git and Python installed already.

Most development is done on Ubuntu (currently 14.04, Trusty Tahr). There are
some instructions for other distros below, but they are mostly unsupported.

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

If you don't want the full repo history, you can save a lot of time by
adding the `--no-history` flag to `fetch`.

Expect the command to take 30 minutes on even a fast connection, and many
hours on slower ones.

If you've already installed the build dependencies on the machine (from another
checkout, for example), you can omit the `--nohooks` flag and `fetch`
will automatically execute `gclient runhooks` at the end.

When `fetch` completes, it will have created a hidden `.gclient` file and a
directory called `src` in the working directory. The remaining instructions
assume you have switched to the `src` directory:

```shell
$ cd src
```

### Install additional build dependencies

Once you have checked out the code, and assuming you're using Ubuntu, run
[build/install-build-deps.sh](/build/install-build-deps.sh)

You may need to adjust the build dependencies for other distros. There are
some [notes](#notes) at the end of this document, but we make no guarantees
for their accuracy.

### Run the hooks

Once you've run `install-build-deps` at least once, you can now run the
Chromium-specific hooks, which will download additional binaries and other
things you might need:

```shell
$ gclient runhooks
```

*Optional*: You can also [install API
keys](https://www.chromium.org/developers/how-tos/api-keys) if you want your
build to talk to some Google services, but this is not necessary for most
development and testing purposes.

## Setting up the build

Chromium uses [Ninja](https://ninja-build.org) as its main build tool along with
a tool called [GN](https://gn.googlesource.com/gn/+/master/docs/quick_start.md)
to generate `.ninja` files. You can create any number of *build directories*
with different configurations. To create a build directory, run:

```shell
$ gn gen out/Default --args='is_chromecast=true'
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
  [quick start guide](../tools/gn/docs/quick_start.md).

### <a name="faster-builds"></a>Faster builds

You might try some of the suggestions on the
[Linux build setup](linux_build_instructions.md#faster-builds).

## Build cast\_shell

Build cast\_shell with Ninja using the command:

```shell
$ autoninja -C out/Default cast_shell
```

(`autoninja` is a wrapper that automatically provides optimal values for the
arguments passed to `ninja`.)

## Run cast\_shell

Once it is built, you can simply run it:

```shell
$ out/Default/cast_shell --ozone-platform=x11 http://google.com
```

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

### More links

*   Want to use Eclipse as your IDE? See
    [LinuxEclipseDev](linux_eclipse_dev.md).
