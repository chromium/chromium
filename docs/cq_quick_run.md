# CQ Quick Run

CQ Quick Run (QR) is a new CQ mode with the goal of delivering results faster.
QR saves roughly 50% CPU time in exchange for at most a 5% chance of false
negative.

QR uses a novel regression test selection
[technique](./testing/regression-test-selection.md) that is more granular than
the conventional build dependency graph technique (see link for more info).

QR may be the home for other aggressive CQ speed improvements in the future.

## Usage

Ping guterman@google.com if you would like to be added to the pilot/beta. Then
one can trigger a quick run by running either `git cl try -q` or
`git cl upload -q`. This sets both the Quick-Run and Commit-Queue labels to 1,
which starts a Quick Run. Setting the two labels at the same time in the Gerrit
UI also starts a Quick Run

Unlike Dry Runs, Quick Runs can't be reused for CQ+2.

Quick Run starts all the builders a Dry Run would. Not all builders have the
new test selection algorithm enabled yet, so these builders will run at their
regular speed. Look for "use_rts: true" in the build to see which builders are
speed optimized.


## Known Issues

Please report any Quick Run issues [here](https://bugs.chromium.org/p/chromium/issues/entry?template=Quick%20Run%20Issue)

* The Gerrit integration isn't fully fleshed out yet so it's hard to tell what's
a Quick Run vs a normal run. Also, in the future there will be a dedicated
button in Gerrit for Quick Run, just like Dry Run and Submit.
