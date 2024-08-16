# Release Blockers

[TOC]

## tl;dr

*   Only mark bugs as blockers if the product **must not** be shipped with the
    bug present.
*   **Everyone** on the team can add or remove blocking labels.
*   Evaluate bugs as potential blockers based on their **severity** and
    **prevalence**.
*   Provide **detailed rationale** whenever adding or removing a blocking label.
*   Ensure all blockers have an **OS and milestone** tagged.
*   Release owners have final say on blocking status; contact them with any
    questions.

## Context

The Chromium project utilizes release block labels to help define each
milestone's critical path. Release block labels are tracked via an issue's
ReleaseBlock field, and the following values are available:

*   **ReleaseBlock:Dev**, which blocks shipping to the dev, beta, and stable
    channels.
*   **ReleaseBlock:Beta**, which blocks shipping to the beta and stable
    channels.
*   **ReleaseBlock:Stable**, which blocks shipping to the stable channel.

The ReleaseBlock field must be used in conjunction with the Milestone and OS
field. The combination of blocking state, milestone, and OS determine which
releases we hold; e.g. OS:Android Milestone:59 ReleaseBlock:Beta means we will
not ship M59 Android builds to either the beta or stable channel until the bug
is addressed. Android M59 dev releases, or releases on any other platform, would
not be blocked.

Because these labels are used to manage critical path, they should **not** be
used unless we genuinely believe we cannot ship to a given channel with the
issue present. Do not mark an issue as a blocker "to ensure someone looks at
it." Priority and milestone labels should be used for this purpose.

These rules apply to bugs and regressions; teams may use criteria for blocking
due to other work (e.g. tasks) as they see fit.

## Assessing Blockers

Issues should be evaluated for release blocking status using the following
matrix based on the issue's severity and prevalence:

|               | Low Impact          | Medium              | High                | Critical         |
| ------------- | ------------------- | ------------------- | ------------------- | ---------------- |
| **Few Users** |                     |                     | ReleaseBlock:Stable | ReleaseBlock:Dev |
| **Some**      |                     | ReleaseBlock:Stable | ReleaseBlock:Beta   | ReleaseBlock:Dev |
| **Most**      | ReleaseBlock:Stable | ReleaseBlock:Beta   | ReleaseBlock:Beta   | ReleaseBlock:Dev |
| **All**       | ReleaseBlock:Stable | ReleaseBlock:Beta   | ReleaseBlock:Dev    | ReleaseBlock:Dev |

### Severity

Severity is defined as the impact to a user who experiences the bug.

*   **Critical**: A bug with extreme consequence to the user, e.g. a regression
    in privacy (leaking user data), loss of user data, crash on startup, etc.
    These bugs must be fixed immediately and thus should block any release where
    they are present.
*   **High**: A bug with large impact to the user, e.g. a CSS rendering issue
    causing content to disappear, videos not playing, extreme jank, etc. There
    is no simple workaround for the issue.
*   **Medium**: A bug with moderate impact to the user, e.g. a CSS rendering
    issue causing content to be misaligned, moderate jank, non-startup crash,
    memory regressions, etc. There may be a workaround for the issue.
*   **Low**: A bug with little impact to the user, generally cosmetic in nature
    and easy to work around.

### Prevalence

Prevalence is defined as the volume of users who will experience the bug.

*   **Few Users (<5%)**: The bug requires many steps to trigger, or is dependent
    on timing, e.g. two simultaneous taps on different parts of the screen.
*   **Some (5% - 35%)**: The bug affects a minor workflow, or requires a series
    of steps to trigger.
*   **Most (35% - 75%)**: The bug affects a major workflow, e.g. sync,
    downloading files, etc.
*   **All (75% - 100%)**: The bug affects core product functionality, e.g.
    scrolling a page.

Note that prevalence should be evaluated based on the population of users they
affect - e.g. a bug affecting all Android users (but not Windows users) would
still be considered to affect all users, and a bug affecting all Enterprise
Windows users (but not all consumer Windows users) could also be considered to
affect all users.

### Assessing uncertainty

In practice, the data available for assessing severity and prevalence of bugs is
usually imperfect, so best judgement and rules of thumb need to be employed.

Engineers assessing bugs can use their knowledge of the underlying system to
intuit whether the observed symptom might be the "tip of an iceberg" of a wider
bug which might have much wider severity and prevalence. The evaluation isn't
required to be limited to the so-far exactly observed symptoms, but should also
be biased upward on the basis of well-founded fears. For example, scary race
conditions or symptoms that indicate a core system with many dependencies is
being undermined.

A rule of thumb is that such scary "iceberg" problems are more likely for
changes which have not yet been exposed to a large population of users --
especially, bugs in Dev channel, or bugs in Beta channel affecting a relatively
small or quiet population (for example, Android WebView has a tiny beta
population, and non-English-speaking users have more difficulty getting their
feedback heard). On the other hand, if a bug already has been present on 100% of
stable channel for weeks or months before it was first noticed, that's evidence
that the problem is not so scary or urgent after all. Therefore, recent
regressions should have an upward bias in severity/prevalence assessment, while
nonrecent ones should have a downward one.

If this sort of consideration is a factor, that should be explicitly mentioned
in the bug update.

### Customization

The definitions provided above are examples; teams are encouraged to customize
where it makes sense, e.g. the web platform team may consider developer impact
for severity and feature usage for prevalence.

## Blocker Management

**Everyone should feel free to add, modify, or remove release blocking labels
where appropriate, so long as you follow the guidelines below.** If a TPM or
test engineer has marked a bug as a release blocker, but a developer knows for
sure that the issue should not block the release, the developer should remove
the release blocking label; similarly anyone should feel free to add a release
blocking label to a bug they feel warrants holding a release. That said, there
are some general guidelines to follow:

*   Be specific and descriptive in your comments when tagging, or untagging, an
    issue as a release blocker. **You must explain your reason for doing so.**
    Including your rationale around impact and prevalence will make it much
    easier for anyone reviewing the bug to understand why the bug should (or
    should not) block the release. It will also help anyone re-assessing the bug
    if we receive new information later.
*   **When in doubt, be conservative and mark bugs as blockers!** It's better to
    tag a bug as a release blocking issue and have the label removed later than
    to ship the bug to users and have to respin due to unanticipated
    consequences. You can always loop in the release management team (by CC'ing
    onto the bug, pinging, or e-mailing) the platform owners listed on the
    [ChromiumDash](https://chromiumdash.appspot.com/schedule) for their input if
    you need assistance.
*   These guidelines should be used with the data we have available at the time.
    **If we need more data to make a good decision, get help in finding it!**
    When new information arises, re-assess the bug using the new details as soon
    as possible.
*   The release management team (release engineers and TPMs) have the final say
    when it comes to release blocking issues - they can tag, and untag, issues
    as they see fit.
*   For fixing your release blockers, please consider a revert of the culprit CL
    as your first option if it is safe to do so.

## Other Considerations

### New Features

Any bugs related to a feature that is behind a flag and is not enabled by
default should never block a release (they should be disabled instead). Features
enabled by default should follow the proposals listed above.

### Regressions

Regressions should follow the same guidelines as listed above; an issue should
not be tagged as a release blocker simply because it is a regression. While we'd
like to prevent regressions in general, there is a large backlog of bugs we need
to address, and we should focus on the most important. To ensure we maintain a
high bar for product quality, we should track the number of introduced versus
escaped regressions, and follow up if the number starts to rise.

In practice, it is still expected that the majority of release blockers filed
will be recent regressions, because on average they have higher severity,
prevalence and uncertainty than longstanding bugs.
