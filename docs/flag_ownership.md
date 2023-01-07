# Chromium Flag Ownership

ellyjones@ / avi@

This document introduces the concept of flag ownership in Chromium.

See also [Chromium Flag Expiry](flag_expiry.md).

[TOC]

## TL;DR / What Do I Need To Do?

Look through
[`//chrome/browser/flag-metadata.json`](../chrome/browser/flag-metadata.json)
for flags that your team added, maintains, or cares about. For each such flag
you find, either:

- **If it is still in use:** ensure that the entries in the owners list are
  correct (see the comment at the top of the file) and that an appropriate
  expiration milestone is set;

- **If it is not in use:** delete the entry from the [`//chrome/browser/flag-metadata.json`](../chrome/browser/flag-metadata.json)
  file, and delete it from `kFeatureEntries` in
  [`//chrome/browser/about_flags.cc`](../chrome/browser/about_flags.cc) or
  [`//ios/chrome/browser/flags/about_flags.mm`](../ios/chrome/browser/flags/about_flags.mm)
  for iOS. Remember to file a cleanup bug to remove code paths that become dead.

## Wait, What Are You Doing?

When the flag ownership project started, many of the hundreds of flags in
`chrome://flags` were obsolete and unused, but each of them represented
configuration surface that was exposed to users and to QA. Worse, obsolete flags
often prevented removal of legacy code paths that were not used in the field but
were still reachable via a flag setting.

The flag ownership project has dealt with that by moving Chromium towards a
model where `chrome://flags` entries are what they were originally intended to
be: temporary, experimental options. Each flag must have a set owner who can
keep track of whether or when that flag should expire and an express time by
which it will expire, either because the feature it controls will have become
default-enabled or because the feature it controls will have been cancelled.

Note that this change only affects `chrome://flags` entries, not features
controlled via [`FeatureList`](../base/feature_list.h) (commonly used to run
Finch trials) or command-line switches.

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

A good rule of thumb is that your flag should expire one milestone after you
expect your feature to have launched to stable. In other words, if your feature
will be 100% enabled on Stable in M74, your flag should be marked as expiring in
M75.

Please do not stress about the expiration date. The purpose of the expiration
milestone is to let us remove *abandoned* flags. Pick a reasonable milestone by
which you'll be done with the flag; you can always adjust it later if your
schedule changes.

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
