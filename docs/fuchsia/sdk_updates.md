# Manually updating Chromium to a new Fuchsia SDK

Normally the Fuchsia SDK dependency is automatically updated to a recent build
on a regular basis, by the [Fuchsia SDK AutoRoll Bot](https://fuchsia-sdk-chromium-roll.skia.org).

Should you need to manually update the SDK dependency for some reason, then:

1. Check the [Fuchsia-side
   job](https://luci-scheduler.appspot.com/jobs/fuchsia/sdk-topaz-x64-linux)
   for a recent green archive. On the "SUCCEEDED" link, copy the SHA-1 from the
   `gsutil.upload` link of the `upload fuchsia-sdk` step.
0. Put that into Chromium's src.git `build/fuchsia/linux.sdk.sha1`.
0. `gclient sync && ninja ...` and make sure things go OK locally.
0. Upload the roll CL, making sure to include the `fuchsia` trybot. Tag the roll
   with `Bug: 707030`.

If you would like to build an SDK locally, `tools/fuchsia/local-sdk.py` tries to
do this (so you can iterate on ToT Fuchsia against your Chromium build), however
it's simply a copy of the steps run on the bot above, and so may be out of date.

In order to sync a Fuchsia tree to the state matching an SDK hash, you can use:

`jiri update https://storage.googleapis.com/fuchsia-snapshots/<SDK_HASH_HERE>`

If you are waiting for a Zircon CL to roll into the SDK, you can check the
status of the [Zircon
roller](https://luci-scheduler.appspot.com/jobs/fuchsia/zircon-roller).
Checking the bot's [list of
CLs](https://fuchsia-review.googlesource.com/q/owner:zircon-roller%40fuchsia-infra.iam.gserviceaccount.com)
might be useful too.

Another useful command, if the SDK was pulled by `cipd` (which it is in
Chromium-related projects like Crashpad, instead of directly pulling the
.tar.gz), is:

`cipd describe fuchsia/sdk/linux-amd64 -version <CIPD_HASH_HERE>`

This description will show the `jiri_snapshot` "tag" for the CIPD package which
corresponds to the SDK revision that's specified in `linux.sdk.sha1` here.
