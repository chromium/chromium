# Chromium Flag Ownership

ellyjones@ / avi@

This document explains flag ownership in Chromium, and gives some principles and
best practices.

See also [Chromium Flag Expiry](flag_expiry.md).

[TOC]

## What Is Flag Ownership?

Every entry in chrome://flags is required to have at least one listed owner,
which can be:

* A single person
* A mailing list
* A reference to an OWNERS file

The owners of a flag serve as points of contact for it, and are notified when
it expires.

## Who Should Be An Owner?

In general, it's a good idea to include:

* The one or two SWEs who know the most about the flag and why it exists
* An appropriate mailing list for the team that owns it, so that expirations are
  not missed, or
* An owners file that contains all the members of the owning team, again so that
  expirations are not missed.

Three important notes about listing owners:

* Any entry that is just a bare word (like "username") is treated as being
  @chromium.org; by far the most common mistake is to write "username" when
  "username@google.com" is meant. This is not a problem if you have both
  chromium.org and google.com addresses.
* The flag ownership database is *public*, so if your team's name is supposed to
  be secret, don't list it; in that situation, it's better to create a new list
  with a less revealing name and have it forward to your team's list.
* Every listed owner must be able to receive email from an *unprivileged*
  google.com account, so please don't list your team's private list that
  requires joining to post or similar - your flag expiration email will bounce.
  If your team's list needs to remain closed to posting, you should instead make
  a separate list that allows open posting, or list an OWNERS file full of
  individuals.

## I Don't Want My Flag To Expire!

Some flags do not control UI features, but rather are commonly-used debugging
controls, such as `ignore-gpu-blocklist`. For these flags, see the instructions
at the head of `flag-metadata.json`. Please be very judicious about
never-expiring flags, since they represent ongoing engineering, test and support
burden. The flags team will probably only approve your non-expiring flag if:

- You regularly ask users or developers to change its value for
  debugging/support purposes
- You have been doing so for at least six months
- You don't plan to stop doing so any time soon

If you have a non-expiring flag, the flags team requires a comment in the json
file as to the rationale that it be non-expiring. A quick sentence or two will
be fine. (Yes, we are aware that, technically, JSON files can't have comments.
Don't worry about it.) You'll also need to add your flag to the permitted list
in
[`//chrome/browser/flag-never-expire-list.json`](../chrome/browser/flag-never-expire-list.json)
which will require approval from the flags team.

## What Should My Expiry Be?

A good rule of thumb is that your flag should expire at least one milestone
after you expect your feature to have launched to stable. In other words, if
your feature will be 100% enabled on Stable in M74, your flag should be marked
as expiring in M75 or later. However, you can freely leave yourself lots of
slack - the purpose of expiration is to ensure that obsolete flags eventually
get cleaned up, but whether that takes one milestone or five for any given flag
is not a big deal. It is also very easy to adjust the expiration milestone later
if you need to.

One practice is to always set your expirations to the next "round" milestone
after you expect to launch, so that your team can batch flag cleanup work - for
example, if your feature is planned to go out in M101, you might set the
expiration to M105, and then your team might schedule a flag cleanup for the 105
branch time.

## I Have Other Questions

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
