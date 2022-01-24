# WebGL Bug Triage Rotation

The WebGL team receives many bug reports from users of Chrome, and web
developers in particular. In order to maintain a high quality product, and
enable customers to deploy successful 3D web applications, it's important that
these reports be evaluated in a timely manner.

In order to better scale the team's efforts, a bug triage rotation has been
introduced. The specifics of the rotation follow:

* Each rotation is one week long.

* The current triager is responsible for:

  * Monitoring the incoming new bugs to the Blink>WebGL component, per the
    following query:

    * [Open bugs not needing feedback, and in the Unconfirmed or Unassigned
      state](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=is%3Aopen+component%3ABlink%3EWebGL+status%3AUnconfirmed%2CUntriaged+-label%3ANeeds-Feedback%2CNeeds-Bisect%2CNeeds-TestConfirmation&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids)

  * If the query above doesn't turn up anything, it's possible that the bug was
    already moved into a different state by the GPU triage rotation. In this
    case please look into the following queries. Please note that these will
    turn up much larger lists, so only focus on the new bugs.

    * [Same query as above, but including Available and Assigned bugs](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=is%3Aopen+component%3ABlink%3EWebGL+status%3AUnconfirmed%2CUntriaged%2CAvailable%2CAssigned+-label%3ANeeds-Feedback%2CNeeds-Bisect%2CNeeds-TestConfirmation&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids)

    * [Open bugs which haven't been
      started](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=is%3Aopen+component%3ABlink%3EWebGL+-status%3AStarted&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids)

    * [All open
      bugs](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=is%3Aopen+component%3ABlink%3EWebGL&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids)

  * Please also monitor these candidates for closing as WontFix:

    * [Untriaged bugs labeled Hotlist-Recharge-Cold](https://bugs.chromium.org/p/chromium/issues/list?colspec=ID%20Pri%20M%20Stars%20ReleaseBlock%20Component%20Status%20Owner%20Summary%20OS%20Modified&x=m&y=releaseblock&cells=ids&q=is%3Aopen%20component%3ABlink%3EWebGL%20status%3AUntriaged%20label%3Ahotlist-recharge-cold&can=2)

    * [Open bugs needing feedback of some sort, not updated in the last 30
      days](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=is%3Aopen+component%3ABlink%3EWebGL+label%3ANeeds+modified%3Ctoday-30&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids)

  * If an issue interacts with other components, add those components. (e.g. V8
    via Blink>JavaScript or sub-components, media usually via
    Internals>GPU>Video or Internals>Media>Video)

  * Determining on what graphics hardware the bug occurs, and assigning GPU- and
    OS- labels.

  * Reproducing these bugs, as best as possible, given the graphics hardware at
    the team's disposal.

  * Figuring out whether it's a bug in the application, and, if so, closing the
    bug with an explanation.

  * Figuring out whether the bug is a regression. If it's appropriate to ask the
    submitter to [run a
    bisect](https://www.chromium.org/developers/bisect-builds-py), please do so;
    if it would be easy for Chrome's Test Engineering team to run it, add the
    Needs-Bisect label; or run a [per-revision
    bisect](https://sites.google.com/a/google.com/chrome-te/home/tools/bisect_builds?pli=1)
    (Google internal, sorry) manually.

  * If the Summary is undescriptive or imprecise, rewriting it.

  * Determining a Type and Priority for the bug, and assigning
    ReleaseBlock-(Stable/Beta/Dev) if appropriate.

  * Working with the submitter to produce a reduced test case if at all
    possible, and, if so, integrating that test into the WebGL conformance
    suite. Add the Needs-Feedback label as appropriate to remove the bug from
    the queries above.

  * Marking the bug as duplicate if necessary.

  * Finding potentially-related bugs and recent changes, adding
    Blocking/Blocked-on links.

  * Assigning to the owner of a likely-related bug or recent change.

Prefer to use the "Available" state to indicate that a bug's been triaged,
rather than assigning bugs to yourself, to avoid having a big bug backlog. For
any new bug that's not related to a recent or existing issue, there should be no
owner.

It's the triager's responsibility to do the above steps for all of the incoming
bugs during that shift. Bugs that aren't handled during a given shift stay with
the triager; they don't spill over to the next shift, unless there's agreement
with the person next on the triage rotation.

Please create a saved query in Monorail for `component:Blink>WebGL` and select
"Notify Immediately" to get emails for every change to a WebGL bug.

This is intended to be a lightweight rotation that shouldn't take too much of
the triager's time. For this reason it's scheduled independently of other shifts
like [pixel wrangling], and may overlap. If any conflicts do
arise, please reach out or swap shifts.

[pixel wrangling]: http://go/gpu-pixel-wrangler

The rotation is [managed
here](https://rotations.corp.google.com/rotation/6257611988008960) via the
Google-internal rotations tool. Shift swaps are managed via the tool. The
calendar which can be subscribed to in order to see the oncall person is linked
from the "Google Calendar Integration" heading under "Settings".
