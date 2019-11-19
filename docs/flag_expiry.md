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

After each milestone's branch point:

1. The flags team chooses a set of flags to begin expiring, from the list
   produced by `tools/flags/list_flags.py --expired-by $MSTONE`. In the steady
   state, when there is not a big backlog of flags to remove, this set will be
   the entire list of flags that are `expired-by $MSTONE`.
2. The flags team hides the flags in this set by default from `chrome://flags`,
   and adds a flag `temporary-unexpire-flags-m$MSTONE` and a base::Feature
   `TemporaryUnexpireFlagsM$MSTONE` which unhide these flags. When hidden from
   `chrome://flags`, all the expired flags will behave as if unset, so users
   cannot be stuck with a non-default setting of a hidden flag.
3. After two further milestones have passed (i.e. at $MSTONE+2 branch), the
   temporary unhide flag & feature will be removed (meaning the flags are now
   permanently invisible), and TPMs will file bugs against the listed owners to
   remove the flags and clean up the backing code.

There are other elements of this process not described here, such as emails to
flags-dev@ tracking the status of the process.

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
