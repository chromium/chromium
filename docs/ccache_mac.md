# Using CCache on Mac

[ccache](http://ccache.samba.org/) is a compiler cache. It speeds up
recompilation of C/C++ code by caching previous compilations and detecting when
the same compilation is being done again. This often results in a significant
speedup in common compilations, especially when switching between branches. This
page is about using ccache on Mac with clang and the NinjaBuild system.

[TOC]

## Installation

In order to use [ccache](http://ccache.samba.org) with
[clang](clang.md), you need to use the
current [git HEAD](http://ccache.samba.org/repo.html), since the most recent
version (3.1.9) doesn't contain the
[patch needed](https://github.com/jrosdahl/ccache/pull/4) for using
[the chromium style plugin](clang.md#Using_plugins).

To install ccache with [homebrew](http://mxcl.github.com/homebrew/), use the
following command:

```shell
brew install --HEAD ccache
```

You can also download and install yourself (with GNU automake, autoconf and
libtool installed):

```shell
git clone git://git.samba.org/ccache.git cd ccache
./autogen.sh
./configure && make && make install
```

Make sure ccache can be found in your `$PATH`.

You can also just use the current released version of ccache (3.1.8 or 3.1.9)
and disable the chromium style plugin with `clang_use_chrome_plugins = false`
in your args.gn.

## Use with GN

You just need to set the use\_ccache variable. Do so like the following:

```shell
gn gen out-gn --args='cc_wrapper="ccache"'
```

## Build

In the build phase, the following environment variables must be set (assuming
you are in `chromium/src`):

```shell
export CCACHE_CPP2=yes
export CCACHE_SLOPPINESS=time_macros
export PATH=`pwd`/third_party/llvm-build/Release+Asserts/bin:$PATH
```

Then you can just run ninja as normal:

```shell
ninja -C out/Release chrome
```

## Optional Steps

*   Configure ccache to use a different cache size with `ccache -M <max size>`.
    You can see a list of configuration options by calling ccache alone.  * The
    default ccache directory is `~/.ccache`. You might want to symlink it to
    another directory (for example, when using FileVault for your home
    directory).
