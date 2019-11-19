# Chrome Flag Ownership

ellyjones@ / avi@

[TOC]

## TL;DR / What Do I Need To Do?

Look through
[`chrome/browser/flag-metadata.json`](https://cs.chromium.org/chromium/src/chrome/browser/flag-metadata.json?sq=package:chromium&q=flag-metadata.json&g=0&l=1)
for flags that your team added, maintains, or cares about. For each such flag
you find, either:

- **If it is still in use:** add entries to the owners list (see the comment at
  the top of the file) and set an appropriate expiration milestone;

- **If it is not in use:** delete it from `kFeatureEntries` in
  [`chrome/browser/about_flags.cc`](https://cs.chromium.org/chromium/src/chrome/browser/about_flags.cc?sq=package:chromium&g=0&l=1319).
  Remember to file a cleanup bug to remove code paths that become dead. It is
  not necessary to delete the corresponding entry in `flag-metadata.json` as it
  will be cleaned up for you in the future.

All existing flags have been set to expire in M76 (~6 months from now). At that
time, every flag with no owners will be hardcoded to behave as though it was
always its default value and will be removed from the `chrome://flags` UI.

## Wait, What Are You Doing?

Presently, Chrome has approximately 600 entries in `chrome://flags`, many of
which are obsolete and unused, but each of which represents configuration
surface that is exposed to users and to QA. Worse, obsolete flags often prevent
removal of legacy code paths that are not used in the field but are still
reachable via a flag setting.

To deal with that, we are moving Chrome towards a model where `chrome://flags`
entries are what they were originally intended to be: temporary, experimental
options. Each flag must have a set owner who can keep track of whether or when
that flag should expire and an express time by which it will expire, either
because the feature it controls will have become default-enabled or because the
feature it controls will have been cancelled.

Note that this change only affects `chrome://flags` entries, not features
controlled via
[`FeatureList`](https://cs.chromium.org/chromium/src/base/feature_list.h?q=FeatureList&sq=package:chromium&g=0&l=92)
(commonly used to run Finch trials) or command-line switches.

## I Don't Want My Flag To Expire!

Some flags do not control UI features, but rather are commonly-used debugging
controls, such as `ignore-gpu-blacklist`. For these flags, see the instructions
at the head of `flag-metadata.json`. Please be very judicious about
never-expiring flags, since they represent ongoing engineering, test and support
burden. The flags team will probably only approve your non-expiring flag if:

- You regularly ask users or developers to change its value for
  debugging/support purposes
- You have been doing so for at least six months
- You don't plan to stop doing so any time soon

If you have a non-expiring flag, the flags team requires a comment in the json
file as to the rationale that it be non-expiring. A quick sentence or two will
be fine. Yes, we are aware that, technically, JSON files can't have comments.
Don't worry about it. You'll also need to add your flag to the permitted list in
[`chrome/browser/flag-never-expire-list.json`](https://cs.chromium.org/chromium/src/chrome/browser/flag-never-expire-list.json?sq=package:chromium&q=flag-never-expire-list.json&g=0&l=1)
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
