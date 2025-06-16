# //crypto README

This directory contains implementations of crypto primitives for use in
Chromium. Most of these are either:

* Wrappers around platform-specific APIs (DPAPI, libsecret, etc), so that code
  elsewhere in Chromium can use cross-platform abstractions, or
* Wrappers around BoringSSL APIs that use Chromium-native types like base::span
  and similar

There is very little actual cryptographic code in //crypto - it is mostly
wrappers.

This directory is actively being refactored as of 2025-06. See
[PLAN.md](PLAN.md).
