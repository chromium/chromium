## Description

The src folder contains a partial copy of the source repository since we only
need the sources for standard library components that have a dependency on
libc++.

## Purpose

The iOS build may use the swift compiler's C++ interop feature. Some swift
standard library modules that provide support for C++ interop have dependencies
on libc++. The module binaries that are packaged with stable versions of the
swift compiler were built using a revision of libc++ that is different from the
revision in the Chromium toolchain and are therefore incompatible.

The solution is to use custom builds of these modules that were built using
Chromium's revion of libc++.

## Project Status

The use of swift/C++ interop is experimental at this time. It is not used in
official builds or on CQ bots.

At this interim stage, we do not yet have a proper mirror set up on
chromium.googlesource.com. We're just using a manual copy of files from the
upstream repo.
