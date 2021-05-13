# Chromium Flag Expiry

ellyjones@ / avi@

This document outlines the process by which flags in Chromium expire and are
removed from the codebase. This document only describes entries in
`chrome://flags`, *not* command-line switches (commonly also called command-line
flags). This process does not cover command-line switches and there continue to
be no guarantees about those.

See also [Chromium Flag Ownership](flag_ownership.md).

[TOC]

## Do Not Depend On Flags

If you are a user or administrator of Chrome and are using (or think you need to
use) a flag to configure Chromium for your use case, please [file a bug] or
email [flags-dev@], because that flag will likely be removed at some point. If
you are a Chromium developer, please carry on using flags as normal :)

Flags have never been a supported configuration surface in Chromium, and we have
never guaranteed that any specific flag will behave consistently or even
continue to exist. This document describes a process for removing flags that
have been around for long enough that users *might* have come to rely on their
ongoing existence in a way that hopefully minimizes pain, but Chromium
developers are free to change the behavior of or remove flags at any time. In
particular, just because a flag will expire through this process does not mean a
developer will not remove it earlier than this process specifies.

## The Process

The logic in
[`//tools/flags/generate_unexpire_flags.py`](../tools/flags/generate_unexpire_flags.py)
implements most of this. At any given time, if the current value of `MAJOR` in
[`//chrome/VERSION`](../chrome/VERSION) is *`$MSTONE`*, the two previous
milestones (*`$MSTONE-1`* and *`$MSTONE-2`*) are considered recent.

Then:
1) Flags whose expiration is *`$MSTONE`* or higher are not expired
2) Flags whose expiration is *`$MSTONE-3`* or lower are unconditionally expired
3) Flags whose expiration is *`$MSTONE-1`* or *`$MSTONE-2`* are expired by
   default, but can be temporarily unexpired via flags named
   "`temporary-unexpire-flags-M`*`$MSTONE`*".

There are other elements of this process not described here, such as emails to
[flags-dev@] tracking the status of the process.

Google employees: See more at
[go/flags-expiry-process](http://goto.google.com/flags-expiry-process) and
[go/chrome-flags:expiry-process](http://goto.google.com/chrome-flags:expiry-process).

## Removing A Flag
If a flag is no longer used (for instance, it was used to control a feature
that has since launched), the flag should be removed. Delete the entry in
[`//chrome/browser/about_flags.cc`](../chrome/browser/about_flags.cc) or
[`//ios/chrome/browser/flags/about_flags.mm`](../ios/chrome/browser/flags/about_flags.mm)
for iOS (and any corresponding entries for the flag description), and remove any
references in
[`//chrome/browser/flag-metadata.json`](../chrome/browser/flag-metadata.json).

## Removed Flags

[https://crbug.com/953690](https://crbug.com/953690) is the never-to-be-closed
bug to track flags that are removed.

## I Have Questions

Please get in touch with
[`flags-dev@chromium.org`](https://groups.google.com/a/chromium.org/forum/#!forum/flags-dev).
If you feel like you need to have a Google-internal discussion for some reason,
there's also
[`chrome-flags@`](https://groups.google.com/a/google.com/forum/#!forum/chrome-flags).

## Relevant Source Files

* [`//chrome/browser/about_flags.cc`](../chrome/browser/about_flags.cc)
* [`//chrome/browser/flag-metadata.json`](../chrome/browser/flag-metadata.json)
* [`//chrome/browser/flag-never-expire-list.json`](../chrome/browser/flag-never-expire-list.json)
* [`//chrome/browser/expired_flags_list.h`](../chrome/browser/expired_flags_list.h)
* [`//ios/chrome/browser/flags/about_flags.mm`](../ios/chrome/browser/flags/about_flags.mm)
* [`//tools/flags/generate_expired_list.py`](../tools/flags/generate_expired_list.py)
* [`//tools/flags/generate_unexpire_flags.py`](../tools/flags/generate_unexpire_flags.py)

[file a bug]: https://new.crbug.com
[flags-dev@]: https://groups.google.com/a/chromium.org/forum/#!forum/flags-dev
