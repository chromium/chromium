# Checking out and building Chromium on Linux

There are instructions for other platforms linked from the
[get the code](../get_the_code.md) page.

## Instructions for Google Employees

Are you a Google employee? See
[go/building-chrome](https://goto.google.com/building-chrome) instead.

[TOC]

## System requirements

* An x86-64 machine with at least 8GB of RAM. More than 16GB is highly
    recommended. If your machine has an SSD, it is recommended to have
    \>=32GB/>=16GB of swap for machines with 8GB/16GB of RAM respectively.
* At least 100GB of free disk space. It does not have to be on the same drive;
 Allocate ~50-80GB on HDD for build.
* You must have Git and Python v3.8+ installed already (and `python3` must point
    to a Python v3.8+ binary). Depot_tools bundles an appropriate version
    of Python in `$depot_tools/python-bin`, if you don't have an appropriate
    version already on your system.

Most development is done on Ubuntu (Chromium's build infrastructure currently
runs 22.04, Jammy Jellyfish). There are some instructions for other distros
below, but they are mostly unsupported, but installation instructions can be found in [Docker](#docker).

## Install `depot_tools`

Clone the `depot_tools` repository:

```shell
$ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```

Add `depot_tools` to the beginning of your `PATH` (you will probably want to put
this in your `~/.bashrc` or `~/.zshrc`). Assuming you cloned `depot_tools` to
`/path/to/depot_tools`:

```shell
$ export PATH="/path/to/depot_tools:$PATH"
```

When cloning `depot_tools` to your home directory **do not** use `~` on PATH,
otherwise `gclient runhooks` will fail to run. Rather, you should use either
`$HOME` or the absolute path:

```shell
$ export PATH="${HOME}/depot_tools:$PATH"
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

*** note
**NixOS users:** tools like `fetch` won’t work without a Nix shell. Clone [the
tools repo](https://chromium.googlesource.com/chromium/src/tools) with `git`,
then run `nix-shell tools/nix/shell.nix`.
***

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

```shell
$ ./build/install-build-deps.sh
```

You may need to adjust the build dependencies for other distros. There are
some [notes](#notes-for-other-distros) at the end of this document, but we make no guarantees
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
a tool called [GN](https://gn.googlesource.com/gn/+/main/docs/quick_start.md)
to generate `.ninja` files. You can create any number of *build directories*
with different configurations. To create a build directory, run:

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

### Faster builds

This section contains some things you can change to speed up your builds,
sorted so that the things that make the biggest difference are first.

#### Use Reclient

*** note
**Warning:** If you are a Google employee, do not follow the instructions below.
See
[go/chrome-linux-build#setup-remote-execution](https://goto.google.com/chrome-linux-build#setup-remote-execution)
instead.
***

Chromium's build can be sped up significantly by using a remote execution system
compatible with [REAPI](https://github.com/bazelbuild/remote-apis). This allows
you to benefit from remote caching and executing many build actions in parallel
on a shared cluster of workers.

For contributors who have
[tryjob access](https://www.chromium.org/getting-involved/become-a-committer/#try-job-access)
, please ask a Googler to email accounts@chromium.org on your behalf to access
RBE backend paid by Google. Note that reclient for external contributors is a
best-effort process. We do not guarantee when you will be invited. Reach out to
[reclient-users@chromium.org](https://groups.google.com/a/chromium.org/g/reclient-users)
if you have any questions about reclient usage.

To get started, you need access to an REAPI-compatible backend. The following
instructions assume that you received an invitation from Google to use
Chromium's RBE service and were granted access to it. However, you are welcome
to use any of the
[other compatible backends](https://github.com/bazelbuild/remote-apis#servers),
in which case you will have to adapt the following instructions regarding the
authentication method, instance name, etc. to work with your backend.

Chromium's build uses a client developed by Google called
[reclient](https://github.com/bazelbuild/reclient) to remotely execute build
actions. If you would like to use `reclient` with RBE, you'll first need to:

1. [Install the gcloud CLI](https://cloud.google.com/sdk/docs/install). You can
   pick any installation method from that page that works best for you.
2. Run `gcloud auth login --update-adc` and login with your authorized
   account. Ignore the message about the `--update-adc` flag being deprecated.

Next, you'll have to specify your `rbe_instance` in your `.gclient`
configuration to use the correct one for Chromium contributors:

*** note
**Warning:** If you are a Google employee, do not follow the instructions below.
See
[go/chrome-linux-build#setup-remote-execution](https://goto.google.com/chrome-linux-build#setup-remote-execution)
instead.
***

```
solutions = [
  {
    ...,
    "custom_vars": {
      # This is the correct instance name for using Chromium's RBE service.
      # You can only use it if you were granted access to it. If you use your
      # own REAPI-compatible backend, you will need to change this accordingly
      # to its requirements.
      "rbe_instance": "projects/rbe-chromium-untrusted/instances/default_instance",
    },
  },
]
```

And run `gclient sync`. This will regenerate the config files in
`buildtools/reclient_cfgs` to use the `rbe_instance` that you just added to your
`.gclient` file.

Then, add the following GN args to your `args.gn`:

```
use_remoteexec = true
reclient_cfg_dir = "../../buildtools/reclient_cfgs/linux"
```

*** note
If you are building an older version of Chrome with reclient you will need to
use `rbe_cfg_dir = "../../buildtools/reclient_cfgs_linux"`
***

That's it. Remember to always use `autoninja` for building Chromium as described
below, which handles the startup and shutdown of the reproxy daemon process
that's required during the build, instead of directly invoking `ninja`.

#### Disable NaCl

By default, the build includes support for
[Native Client (NaCl)](https://developer.chrome.com/native-client), but
most of the time you won't need it. You can set the GN argument
`enable_nacl=false` and it won't be built.

#### Include fewer debug symbols

By default GN produces a build with all of the debug assertions enabled
(`is_debug=true`) and including full debug info (`symbol_level=2`). Setting
`symbol_level=1` will produce enough information for stack traces, but not
line-by-line debugging. Setting `symbol_level=0` will include no debug
symbols at all. Either will speed up the build compared to full symbols.

#### Disable debug symbols for Blink and v8

Due to its extensive use of templates, the Blink code produces about half
of our debug symbols. If you don't ever need to debug Blink, you can set
the GN arg `blink_symbol_level=0`. Similarly, if you don't need to debug v8 you
can improve build speeds by setting the GN arg `v8_symbol_level=0`.

#### Use Icecc

[Icecc](https://github.com/icecc/icecream) is the distributed compiler with a
central scheduler to share build load. Currently, many external contributors use
it. e.g. Intel, Opera, Samsung (this is not useful if you're using Reclient).

In order to use `icecc`, set the following GN args:

```
use_debug_fission=false
is_clang=false
```

See these links for more on the
[bundled_binutils limitation](https://github.com/icecc/icecream/commit/b2ce5b9cc4bd1900f55c3684214e409fa81e7a92),
the [debug fission limitation](http://gcc.gnu.org/wiki/DebugFission).

Using the system linker may also be necessary when using glibc 2.21 or newer.
See [related bug](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=808181).

#### ccache

You can use [ccache](https://ccache.dev) to speed up local builds (again,
this is not useful if you're using Reclient).

Increase your ccache hit rate by setting `CCACHE_BASEDIR` to a parent directory
that the working directories all have in common (e.g.,
`/home/yourusername/development`). Consider using
`CCACHE_SLOPPINESS=include_file_mtime` (since if you are using multiple working
directories, header times in svn sync'ed portions of your trees will be
different - see
[the ccache troubleshooting section](https://ccache.dev/manual/latest.html#_troubleshooting)
for additional information). If you use symbolic links from your home directory
to get to the local physical disk directory where you keep those working
development directories, consider putting

```
alias cd="cd -P"
```

in your `.bashrc` so that `$PWD` or `cwd` always refers to a physical, not
logical directory (and make sure `CCACHE_BASEDIR` also refers to a physical
parent).

If you tune ccache correctly, a second working directory that uses a branch
tracking trunk and is up to date with trunk and was gclient sync'ed at about the
same time should build chrome in about 1/3 the time, and the cache misses as
reported by `ccache -s` should barely increase.

This is especially useful if you use
[git-worktree](http://git-scm.com/docs/git-worktree) and keep multiple local
working directories going at once.

#### Using tmpfs

You can use tmpfs for the build output to reduce the amount of disk writes
required. I.e. mount tmpfs to the output directory where the build output goes:

As root:
```
mount -t tmpfs -o size=20G,nr_inodes=40k,mode=1777 tmpfs /path/to/out
```

*** note
**Caveat:** You need to have enough RAM + swap to back the tmpfs. For a full
debug build, you will need about 20 GB. Less for just building the chrome target
or for a release build.
***

Quick and dirty benchmark numbers on a HP Z600 (Intel core i7, 16 cores
hyperthreaded, 12 GB RAM)

* With tmpfs:
  * 12m:20s
* Without tmpfs
  * 15m:40s

### Smaller builds

The Chrome binary contains embedded symbols by default. You can reduce its size
by using the Linux `strip` command to remove this debug information. You can
also reduce binary size and turn on all optimizations by enabling official build
mode, with the GN arg `is_official_build = true`.

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

## Compile a single file

Ninja supports a special [syntax `^`][ninja hat syntax] to compile a single object file specyfing
the source file. For example, `autoninja -C out/Default ../../base/logging.cc^`
compiles `obj/base/base/logging.o`.

[ninja hat syntax]: https://ninja-build.org/manual.html#:~:text=There%20is%20also%20a%20special%20syntax%20target%5E%20for%20specifying%20a%20target%20as%20the%20first%20output%20of%20some%20rule%20containing%20the%20source%20you%20put%20in%20the%20command%20line%2C%20if%20one%20exists.%20For%20example%2C%20if%20you%20specify%20target%20as%20foo.c%5E%20then%20foo.o%20will%20get%20built%20(assuming%20you%20have%20those%20targets%20in%20your%20build%20files)

In addition to `foo.cc^`, Siso also supports `foo.h^` syntax to compile
the corresponding `foo.o` if it exists.

## Run Chromium

Once it is built, you can simply run the browser:

```shell
$ out/Default/chrome
```

If you're using a remote machine that supports Chrome Remote Desktop, you can
add this to your .bashrc / .bash_profile.

```shell
if [[ -z "${DISPLAY}" ]]; then
  # In reality, Chrome Remote Desktop starts with 20 and increases until it
  # finds an available ID [1]. So this isn't guaranteed to always work, but
  # should work on the vast majoriy of cases.
  #
  # [1] https://source.chromium.org/chromium/chromium/src/+/main:remoting/host/linux/linux_me2me_host.py;l=112;drc=464a632e21bcec76c743930d4db8556613e21fd8
  export DISPLAY=:20
fi
```

This means if you launch Chrome from an SSH session, the UI output will be
available in Chrome Remote Desktop.

## Running test targets

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

### Linker Crashes

If, during the final link stage:

```
LINK out/Debug/chrome
```

You get an error like:

```
collect2: ld terminated with signal 6 Aborted terminate called after throwing an instance of 'std::bad_alloc'
collect2: ld terminated with signal 11 [Segmentation fault], core dumped
```

or:

```
LLVM ERROR: out of memory
```

you are probably running out of memory when linking. You *must* use a 64-bit
system to build. Try the following build settings (see [GN build
configuration](https://www.chromium.org/developers/gn-build-configuration) for
other settings):

* Build in release mode (debugging symbols require more memory):
    `is_debug = false`
* Turn off symbols: `symbol_level = 0`
* Build in component mode (this is for development only, it will be slower and
    may have broken functionality): `is_component_build = true`
* For official (ThinLTO) builds on Linux, increase the vm.max_map_count kernel
    parameter: increase the `vm.max_map_count` value from default (like 65530)
    to for example 262144. You can run the `sudo sysctl -w vm.max_map_count=262144`
    command to set it in the current session from the shell, or add the
    `vm.max_map_count=262144` to /etc/sysctl.conf to save it permanently.

### More links

* Information about [building with Clang](../clang.md).
* You may want to [use a chroot](using_a_chroot.md) to
    isolate yourself from versioning or packaging conflicts.
* Cross-compiling for ARM? See [LinuxChromiumArm](chromium_arm.md).
* Want to use Eclipse as your IDE? See
    [LinuxEclipseDev](eclipse_dev.md).
* Want to use your built version as your default browser? See
    [LinuxDevBuildAsDefaultBrowser](dev_build_as_default_browser.md).

## Next Steps

If you want to contribute to the effort toward a Chromium-based browser for
Linux, please check out the [Linux Development page](development.md) for
more information.

## Notes for other distros

### Arch Linux

Instead of running `install-build-deps.sh` to install build dependencies, run:

```shell
$ sudo pacman -S --needed python perl gcc gcc-libs bison flex gperf pkgconfig \
nss alsa-lib glib2 gtk3 nspr freetype2 cairo dbus xorg-server-xvfb \
xorg-xdpyinfo
```

For the optional packages on Arch Linux:

* `php-cgi` is provided with `pacman`
* `wdiff` is not in the main repository but `dwdiff` is. You can get `wdiff`
    in AUR/`yaourt`

### Crostini (Debian based)

First install the `file` and `lsb-release` commands for the script to run properly:

```shell
$ sudo apt-get install file lsb-release
```

Then invoke install-build-deps.sh with the `--no-arm` argument,
because the ARM toolchain doesn't exist for this configuration:

```shell
$ sudo install-build-deps.sh --no-arm
```

### Fedora

Instead of running `build/install-build-deps.sh`, run:

```shell
su -c 'yum install git python bzip2 tar pkgconfig atk-devel alsa-lib-devel \
bison binutils brlapi-devel bluez-libs-devel bzip2-devel cairo-devel \
cups-devel dbus-devel dbus-glib-devel expat-devel fontconfig-devel \
freetype-devel gcc-c++ glib2-devel glibc.i686 gperf glib2-devel \
gtk3-devel java-1.*.0-openjdk-devel libatomic libcap-devel libffi-devel \
libgcc.i686 libjpeg-devel libstdc++.i686 libX11-devel libXScrnSaver-devel \
libXtst-devel libxkbcommon-x11-devel ncurses-compat-libs nspr-devel nss-devel \
pam-devel pango-devel pciutils-devel pulseaudio-libs-devel zlib.i686 httpd \
mod_ssl php php-cli python-psutil wdiff xorg-x11-server-Xvfb'
```

The fonts needed by Blink's web tests can be obtained by following [these
instructions](https://gist.github.com/pwnall/32a3b11c2b10f6ae5c6a6de66c1e12ae).
For the optional packages:

* `php-cgi` is provided by the `php-cli` package.
* `sun-java6-fonts` is covered by the instructions linked above.

### Gentoo

You can just run `emerge www-client/chromium`.

### NixOS

To get a shell with the dev environment:

```sh
$ nix-shell tools/nix/shell.nix
```

To run a command in the dev environment:

```sh
$ NIX_SHELL_RUN='autoninja -C out/Default chrome' nix-shell tools/nix/shell.nix
```

To set up clangd with remote indexing support, run the command below, then copy
the path into your editor config:

```sh
$ NIX_SHELL_RUN='readlink /usr/bin/clangd' nix-shell tools/nix/shell.nix
```

### OpenSUSE

Use `zypper` command to install dependencies:

(openSUSE 11.1 and higher)

```shell
sudo zypper in subversion pkg-config python perl bison flex gperf \
     mozilla-nss-devel glib2-devel gtk-devel wdiff lighttpd gcc gcc-c++ \
     mozilla-nspr mozilla-nspr-devel php5-fastcgi alsa-devel libexpat-devel \
     libjpeg-devel libbz2-devel
```

For 11.0, use `libnspr4-0d` and `libnspr4-dev` instead of `mozilla-nspr` and
`mozilla-nspr-devel`, and use `php5-cgi` instead of `php5-fastcgi`.

(openSUSE 11.0)

```shell
sudo zypper in subversion pkg-config python perl \
     bison flex gperf mozilla-nss-devel glib2-devel gtk-devel \
     libnspr4-0d libnspr4-dev wdiff lighttpd gcc gcc-c++ libexpat-devel \
     php5-cgi alsa-devel gtk3-devel jpeg-devel
```

The Ubuntu package `sun-java6-fonts` contains a subset of Java of the fonts used.
Since this package requires Java as a prerequisite anyway, we can do the same
thing by just installing the equivalent openSUSE Sun Java package:

```shell
sudo zypper in java-1_6_0-sun
```

WebKit is currently hard-linked to the Microsoft fonts. To install these using `zypper`

```shell
sudo zypper in fetchmsttfonts pullin-msttf-fonts
```

To make the fonts installed above work, as the paths are hardcoded for Ubuntu,
create symlinks to the appropriate locations:

```shell
sudo mkdir -p /usr/share/fonts/truetype/msttcorefonts
sudo ln -s /usr/share/fonts/truetype/arial.ttf /usr/share/fonts/truetype/msttcorefonts/Arial.ttf
sudo ln -s /usr/share/fonts/truetype/arialbd.ttf /usr/share/fonts/truetype/msttcorefonts/Arial_Bold.ttf
sudo ln -s /usr/share/fonts/truetype/arialbi.ttf /usr/share/fonts/truetype/msttcorefonts/Arial_Bold_Italic.ttf
sudo ln -s /usr/share/fonts/truetype/ariali.ttf /usr/share/fonts/truetype/msttcorefonts/Arial_Italic.ttf
sudo ln -s /usr/share/fonts/truetype/comic.ttf /usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS.ttf
sudo ln -s /usr/share/fonts/truetype/comicbd.ttf /usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS_Bold.ttf
sudo ln -s /usr/share/fonts/truetype/cour.ttf /usr/share/fonts/truetype/msttcorefonts/Courier_New.ttf
sudo ln -s /usr/share/fonts/truetype/courbd.ttf /usr/share/fonts/truetype/msttcorefonts/Courier_New_Bold.ttf
sudo ln -s /usr/share/fonts/truetype/courbi.ttf /usr/share/fonts/truetype/msttcorefonts/Courier_New_Bold_Italic.ttf
sudo ln -s /usr/share/fonts/truetype/couri.ttf /usr/share/fonts/truetype/msttcorefonts/Courier_New_Italic.ttf
sudo ln -s /usr/share/fonts/truetype/impact.ttf /usr/share/fonts/truetype/msttcorefonts/Impact.ttf
sudo ln -s /usr/share/fonts/truetype/times.ttf /usr/share/fonts/truetype/msttcorefonts/Times_New_Roman.ttf
sudo ln -s /usr/share/fonts/truetype/timesbd.ttf /usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold.ttf
sudo ln -s /usr/share/fonts/truetype/timesbi.ttf /usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold_Italic.ttf
sudo ln -s /usr/share/fonts/truetype/timesi.ttf /usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Italic.ttf
sudo ln -s /usr/share/fonts/truetype/verdana.ttf /usr/share/fonts/truetype/msttcorefonts/Verdana.ttf
sudo ln -s /usr/share/fonts/truetype/verdanab.ttf /usr/share/fonts/truetype/msttcorefonts/Verdana_Bold.ttf
sudo ln -s /usr/share/fonts/truetype/verdanai.ttf /usr/share/fonts/truetype/msttcorefonts/Verdana_Italic.ttf
sudo ln -s /usr/share/fonts/truetype/verdanaz.ttf /usr/share/fonts/truetype/msttcorefonts/Verdana_Bold_Italic.ttf
```

The Ubuntu package `sun-java6-fonts` contains a subset of Java of the fonts used.
Since this package requires Java as a prerequisite anyway, we can do the same
thing by just installing the equivalent openSUSE Sun Java package:

```shell
sudo zypper in java-1_6_0-sun
```

WebKit is currently hard-linked to the Microsoft fonts. To install these using `zypper`

```shell
sudo zypper in fetchmsttfonts pullin-msttf-fonts
```

To make the fonts installed above work, as the paths are hardcoded for Ubuntu,
create symlinks to the appropriate locations:

```shell
sudo mkdir -p /usr/share/fonts/truetype/msttcorefonts
sudo ln -s /usr/share/fonts/truetype/arial.ttf /usr/share/fonts/truetype/msttcorefonts/Arial.ttf
sudo ln -s /usr/share/fonts/truetype/arialbd.ttf /usr/share/fonts/truetype/msttcorefonts/Arial_Bold.ttf
sudo ln -s /usr/share/fonts/truetype/arialbi.ttf /usr/share/fonts/truetype/msttcorefonts/Arial_Bold_Italic.ttf
sudo ln -s /usr/share/fonts/truetype/ariali.ttf /usr/share/fonts/truetype/msttcorefonts/Arial_Italic.ttf
sudo ln -s /usr/share/fonts/truetype/comic.ttf /usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS.ttf
sudo ln -s /usr/share/fonts/truetype/comicbd.ttf /usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS_Bold.ttf
sudo ln -s /usr/share/fonts/truetype/cour.ttf /usr/share/fonts/truetype/msttcorefonts/Courier_New.ttf
sudo ln -s /usr/share/fonts/truetype/courbd.ttf /usr/share/fonts/truetype/msttcorefonts/Courier_New_Bold.ttf
sudo ln -s /usr/share/fonts/truetype/courbi.ttf /usr/share/fonts/truetype/msttcorefonts/Courier_New_Bold_Italic.ttf
sudo ln -s /usr/share/fonts/truetype/couri.ttf /usr/share/fonts/truetype/msttcorefonts/Courier_New_Italic.ttf
sudo ln -s /usr/share/fonts/truetype/impact.ttf /usr/share/fonts/truetype/msttcorefonts/Impact.ttf
sudo ln -s /usr/share/fonts/truetype/times.ttf /usr/share/fonts/truetype/msttcorefonts/Times_New_Roman.ttf
sudo ln -s /usr/share/fonts/truetype/timesbd.ttf /usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold.ttf
sudo ln -s /usr/share/fonts/truetype/timesbi.ttf /usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold_Italic.ttf
sudo ln -s /usr/share/fonts/truetype/timesi.ttf /usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Italic.ttf
sudo ln -s /usr/share/fonts/truetype/verdana.ttf /usr/share/fonts/truetype/msttcorefonts/Verdana.ttf
sudo ln -s /usr/share/fonts/truetype/verdanab.ttf /usr/share/fonts/truetype/msttcorefonts/Verdana_Bold.ttf
sudo ln -s /usr/share/fonts/truetype/verdanai.ttf /usr/share/fonts/truetype/msttcorefonts/Verdana_Italic.ttf
sudo ln -s /usr/share/fonts/truetype/verdanaz.ttf /usr/share/fonts/truetype/msttcorefonts/Verdana_Bold_Italic.ttf
```

And then for the Java fonts:

```shell
sudo mkdir -p /usr/share/fonts/truetype/ttf-lucida
sudo find /usr/lib*/jvm/java-1.6.*-sun-*/jre/lib -iname '*.ttf' -print \
     -exec ln -s {} /usr/share/fonts/truetype/ttf-lucida \;
```

### Docker

#### Prerequisites

While it is not a common setup, Chromium compilation should work from within a
Docker container. If you choose to compile from within a container for whatever
reason, you will need to make sure that the following tools are available:

* `curl`
* `git`
* `lsb_release`
* `python3`
* `sudo`
* `file`

There may be additional Docker-specific issues during compilation. See
[this bug](https://crbug.com/1377520) for additional details on this.

Note: [Clone depot_tools](#install-depot_tools) first.

#### Build Steps

1. Put the following Dockerfile in `/path/to/chromium/`.

```docker
# Use an official Ubuntu base image with Docker already installed
FROM ubuntu:22.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive

# Install Mantatory tools (curl git python3) and optional tools (vim sudo)
RUN apt-get update && \
    apt-get install -y curl git lsb-release python3 git file vim sudo && \
    rm -rf /var/lib/apt/lists/*

# Export depot_tools path
ENV PATH="/depot_tools:${PATH}"

# Configure git for safe.directory
RUN git config --global --add safe.directory /depot_tools && \
    git config --global --add safe.directory /chromium/src

# Set the working directory to the existing Chromium source directory.
# This can be either "/chromium/src" or "/chromium".
WORKDIR /chromium/src

# Expose any necessary ports (if needed)
# EXPOSE 8080
RUN useradd -u 1000 chrom-d

# Create normal user with name "chrom-d". Optional and you can use root but
# not advised.
USER chrom-d

# Start Chromium Builder "chrom-d" (modify this command as needed)
# CMD ["autoninja -C out/Default chrome"]
CMD ["bash"]
```

2. Build Container

```shell
# chrom-b is just a name; You can change it but you must reflect the renaming
# in all commands below
$ docker build -t chrom-b .
```

3. Run container as root to install dependencies

```shell
$ docker run --rm \ # close instance upon exit
  -it \ # Run docker interactively
  --name chrom-b \ # with name "chrom-b"
  -u root \ # with user root
  -v /path/on/machine/to/chromium:/chromium \ # With chromium folder mounted
  -v /path/on/machine/to/depot_tools:/depot_tools \ # With depot_tools mounted
  chrom-b # Run container with image name "chrom-b"
```

4. Install dependencies:

```shell
./build/install-build-deps.sh
```

5. [Run hooks](#run-the-hooks) (On docker or machine if you installed depot_tools on machine)

6. Exit container

7. Save container image with tag-id name `dpv1.0`. Run this on the machine, not in container

```shell
# Get docker running instances, copy the id you get
$ docker ps
# Save/tag running docker container with name "chrom-b" with "dpv1.0"
# You can choose any tag name you want but propagate name accordingly
# You will need to create new tags when working on different parts of
# chromium which requires installing additional dependencies
$ docker commit <ID from above step> chrom-b:dpv1.0
# Optional, just saves space by deleting unnecessary images
$ docker image rmi chrom-b:latest && docker image prune \
  && docker container prune && docker builder prune
```

#### Run container

```shell
$ docker run --rm \ # close instance upon exit
  -it \ # Run docker interactively
  --name chrom-b \ # with name "chrom-b"
  -u $(id -u):$(id -g) \ # Run container as a non-root user with same UID & GID
  -v /path/on/machine/to/chromium:/chromium \ # With chromium folder mounted
  -v /path/on/machine/to/depot_tools:/depot_tools \ # With depot_tools mounted
  chrom-b:dpv1.0 # Run container with image name "chrom-b" and tag dpv1.0
```
