# Cr-Fuchsia Gardening

## Gardener Responsibilities

In priority order, the responsibilities of the Fuchsia Gardener are as follows (though see notes below!):

1.  **Chromium waterfall:** Keep [the Fuchsia bots on the Chromium waterfall](https://ci.chromium.org/p/chromium/g/chromium.linux/console) green.
    1.  Join the #chromium IRC channel on Freenode.
    1.  Not all waterfall bots have a corresponding try-bot configuration.
    1.  E.g. the Cast Fuchsia bots are not run on the CQ by default.
    1.  E.g. the CQ builds have DCHECKs enabled, whereas our Cast waterfall bots don't.
1.  **Chromium try-bots:** Ensure that Fuchsia bots are not causing CQ flake.
    1.  Join the #chromium IRC channel on Freenode.
    1.  Watch for try-flakes emails & investigate any tests/suites reported flakey.
    1.  Watch for IRC mentions of flakiness on the Fuchsia bots.
1.  **Fuchsia SDK rolls:** Keep [the Fuchsia SDK auto-roller](https://autoroll.skia.org/r/fuchsia-sdk-chromium-autoroll) working.
    1.  Watch for emailed status updates from auto-roller CLs.
    1.  If a roll CL fails, check the failed bot to confirm SDK-related fail vs other flake.
    1.  If it was an actual SDK-related failure, note the latest auto-roller patch-Id, and stop the auto-roller.
    1.  Create a local branch e.g. with "`git checkout -b sdkRoll origin`".
    1.  Pull-down the auto-roll CL with "`git cl patch <patch-Id> && gclient sync`".
    1.  Clear the CL metadata with "`git cl issue 0`".
    1.  Make any necessary modifications for compatibility with the new SDK.
    1.  Commit the changes and run "`git cl upload`" to upload a new CL.
    1.  Edit the CL description, which will include the git commit description from the auto-roller CL, making it easy to provide a consistent description.
    1.  Note that if the auto-roller is blocked for a long time then it may be easier to fix the most-recent failed roll, and roll from there, to avoid having to deal with several different causes of breakage in a single roll!
1.  **FYI waterfall:** Keep our bots on the [FYI waterfall](https://ci.chromium.org/p/chromium/g/chromium.fyi/console) green.
    1.  FYI bots don't block the CQ, but still provide early-warning of regressions.
    1.  They're also our staging-ground for bringing complex test suites to the CQ/waterfall.

While the Gardener takes primary responsibility for each of these areas during their rotation, that does not mean that they must do all the work - if another teammate happens to have started fixing the SDK roll, un-breaking the bots, or sending you CLs to fix our Debug builder (look, ma! No try-bot!), then lucky you, your Gardening job is done. :)

## Optional Gardener Tasks

The waterfall is green, the try-bots reliable, SDK is rolling and pigs soar majestically in the sky. Fear not, gentle Gardener, you still have a valuable role to play!

*   Look for tests that have been filtered under Fuchsia, and diagnose them.
    *   File a new bug, or upate the filter to refer to existing bugs, as appropriate.
