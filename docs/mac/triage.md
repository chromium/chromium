# Chrome Mac Team Triage Process

This document outlines how the Chrome Mac Team triages bugs. Triage is the
process of converting raw bug reports filed by developers or users into
actionable, prioritized tasks assigned to engineers.

*** promo If you are not on the Chrome Mac Team and want to send a bug to them
for triage, please add the [Mac-TriageQueue
hotlist](https://issues.chromium.org/hotlists/5648764) and allow the Mac triage
rotation to assign them.
***

The Mac bug triage process is split into two phases. First-phase triage is done
daily in week-long shifts by a single person. Second-phase triage is done in a
standing meeting on Mondays by the Mac team as a group.

## Quick Reference
*** promo All necessary hotlists and saved searches are bundled into the [Mac
triage bookmark group](https://issues.chromium.org/bookmark-groups/861056).
***

1. During the week, the primary works from the ["Mac triage candidates" saved
search](https://issues.chromium.org/savedsearches/6676543). The primary has
two main goals:

    - To ensure that each bug is looked at.

    - To add bugs that should be seen by the whole team to the [Mac-TriageQueue
      hotlist](https://issues.chromium.org/hotlists/5648764)
 > As of March 2024, the candidates saved search is greatly inflated by bugs
 > that became unassigned as a part of the Buganizer transition. Due to this,
 > it's not expected that the primary gets through the whole search. Instead,
 > the primary should focus on bugs filed or updated within the past one or two
 > weeks. We expect that we will shrink the queue to a reasonable size within
 > the next few months
2. During the triage meeting, the team as a whole looks at the triage queue. At
the end of the triage meeting, it's expected that every bug in the queue will be
in one of the following states:

    - [Awaiting feedback](https://issues.chromium.org/hotlists/5433459)
    - Assigned
    - Closed
    - Passed to another team
    - [Bypassed](https://issues.chromium.org/hotlists/5432664)


## First-phase triage

First-phase triage is, first and foremost, a filter.  Not every bug that is
filed on a Mac, or even exclusively occurs on the Mac is best handled by the Mac
team. If the bug is obviously very domain-specific (eg: "this advanced CSS
feature is behaving strangely", or "my printer is printing everything upside
down"), feel free to skip this iteration step and send the bug straight to the
involved team's component. Our triage filter is coarse enough that this isn't
always sufficient to get the bug out of our queue; in these cases the bug should
also be [bypassed](https://issues.chromium.org/hotlists/5432664)

If the primary determines that the Mac team is responsible for this bug (or it
isn't immediately apparent), the next step is to ensure the symptoms and
reproduction steps of the bug are well-understood.
> If the bug is clearly of interest to the wider team, or seems like it could
> use input from domain experts, it makes sense to put it directly into the
> triage queue at this point.

The main work of this phase is iterating with the bug reporter to get crash IDs,
repro steps, traces, and other data we might need to nail down the bug.  Useful
hotlists at this step are:

* Needs-Feedback, which marks the bug as waiting for a response from the
  reporter
* Needs-TestConfirmation, which requests that Test Engineering attempt the bug's
  repro steps
* Needs-Bisect, which requests that Test Engineering bisect the bug down to a
  first bad release

The latter two hotlists work much better when there are reliable repro steps for
a bug, so endeavour to get those first - TE time is precious and we should make
good use of it.

Once the bug is sufficiently understood, it should end up in one of the
following states:

* In the [Mac team triage queue](https://issues.chromium.org/hotlists/5648764)
* WontFix, if they are invalid bug reports or working as intended
* Duplicate, if they are identical to an existing bug
* Assigned to an obvious assignee
* Moved to another team's component
* [Bypassed](https://issues.chromium.org/hotlists/5432664)

We wait **28 days** for user feedback on Needs-Feedback bugs; after 28 days
without a response to a question we move bugs to WontFix.

Some useful debugging questions here:

* What are your exact OS version and Chrome version?
* Does it happen all the time?
* Does it happen in Incognito? (this checks for bad cached data, cookies, etc)
* Does it happen with extensions disabled?
* Does it happen in a new profile?
* Does it happen in a new user-data-dir?
* If it's a web bug, is there a reduced test case? We generally can't act on "my
  website is broken" type issues
* Can you attach a screenshot/screen recording of what you mean?
* Can you paste the crash IDs from chrome://crashes?
* Can you get a sample of the misbehaving process with Activity Monitor?
* Can you upload a trace from chrome://tracing?
* Can you paste the contents of chrome://gpu?
* Can you paste the contents of chrome://version?

## Second-phase triage

Second-phase triage is for "complicated" bugs that benefit from the full team's
perspective. In principle, anything with clear and unambiguous next steps should
not make it to the triage queue.

The primary "drives" by presenting the triage queue in Chromium's issue tracker,
and the team goes through each bug one by one, taking action by consensus. If
the queue is exhausted, the team proceeds to look at the triage candidate queue.

Some bugs require more feedback from either the reporter, or cc'ed members of
other teams; in that case we may choose to keep it in the queue for a week or
two for monitoring. Otherwise, the set of outcomes is similar to first-phase
triage:
* WontFix, if they are invalid bug reports or working as intended
* Duplicate, if they are identical to an existing bug
* Assigned to an obvious assignee
* Moved to another team's component
* [Bypassed](https://issues.chromium.org/hotlists/5432664)
