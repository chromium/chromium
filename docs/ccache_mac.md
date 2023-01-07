# Using CCache on Mac

[ccache](https://ccache.dev/) is a compiler cache. It speeds up
recompilation of C/C++ code by caching previous compilations and detecting when
the same compilation is being done again. This often results in a significant
speedup in common compilations, especially when switching between branches. This
page is about using ccache on Mac with clang and the Ninja build system.

[TOC]

## Installation

To install ccache with [Homebrew](https://brew.sh/), run `brew install ccache`.
With [MacPorts](https://macports.org/), run `port install ccache`. You can also
download and install yourself, using the
[instructions in
the repository](https://github.com/ccache/ccache/blob/master/doc/INSTALL.md).

Make sure ccache can be found in your `$PATH`.

## Use with GN

You just need to set the `cc_wrapper` GN variable. You can do so by running
`gn args out/Default` and adding
`cc_wrapper="env CCACHE_SLOPPINESS=time_macros ccache"` to the build arguments.

## Build

After setting the `cc_wrapper` GN variable you can just run ninja as normal:

```shell
ninja -C out/Default chrome
```

## Optional Steps

*   Configure ccache to use a different cache size with `ccache -M <max size>`.
    You can see a list of configuration options by calling `ccache` alone.
