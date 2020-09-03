# Flag Expiry

This document outlines the process by which flags in chromium expire and are
removed from the codebase, and describes which flags are about to expire. This
is the authoritative list of flags that are expiring and being removed. This
document only describes entries in chrome://flags, *not* command-line switches
(commonly also called command-line flags). This process does not cover
command-line switches and there continue to be no guarantees about those.

[TOC]

## Do Not Depend On Flags

If you are a user or administrator of Chrome and are using (or think you need to
use) a flag to configure Chromium for your use case, please [file a bug] or
email flags-dev@, because that flag will likely be removed at some point. If you
are a chromium developer, please carry on using flags as normal :)

Flags have never been a supported configuration surface in Chromium, and we have
never guaranteed that any specific flag will behave consistently or even
continue to exist. This document describes a process for removing flags that
have been around for long enough that users *might* have come to rely on their
ongoing existence in a way that hopefully minimizes pain, but Chromium
developers are free to change the behavior of or remove flags at any time. In
particular, just because a flag will expire through this process does not mean a
developer will not remove it earlier than this process specifies.

## The Process

The logic in //tools/flags/generate_unexpire_flags.py implements most of this.
At any given time, if the current value of MAJOR in //chrome/version is $MSTONE,
the two previous milestones ($MSTONE-1 and $MSTONE-2) are considered recent.

Then:
1) Flags whose expiration is $MSTONE or higher are not expired
2) Flags whose expiration is $MSTONE-3 or lower are unconditionally expired
3) Flags whose expiration is $MSTONE-1 or $MSTONE-2 are expired by default, but
   can be temporarily unexpired via flags named
   "temporary-unexpire-flags-M$MSTONE".

There are other elements of this process not described here, such as emails to
flags-dev@ tracking the status of the process.

Google employees: See more at
[go/flags-expiry-process](http://goto.google.com/flags-expiry-process) and
[go/chrome-flags:expiry-process](http://goto.google.com/chrome-flags:expiry-process).

## Removing A Flag
If a flag is no longer used (for instance, it was used to control a feature
that has since launched), the flag should be removed. Delete the entry in
[about\_flags.cc](/chrome/browser/about_flags.cc) (and any corresponding entries
for the flag description), and remove any references in
[flag-metadata.json](/chrome/browser/flag-metadata.json).

## The Set

In M78, the following flags are being hidden as the second step of this process.
If you are using one of these flags for some reason, please get in touch with
the flags team (via flags-dev@) and/or the listed owner(s) of that flag. This
list will be updated at each milestone as we expire more flags. This is the
authoritative source of the expiry set for a given milestone.

TODO(https://crbug.com/953690): Fill in this list :)

## See Also

* [//chrome/browser/flag-metadata.json](../chrome/browser/flag-metadata.json)
* [//chrome/browser/expired_flags_list.h](../chrome/browser/expired_flags_list.h)
* [//tools/flags/generate_expired_list.py](../tools/flags/generate_expired_list.py)

[file a bug]: https://new.crbug.com
