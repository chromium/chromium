# Chromium Sheriffing

Author: ellyjones@

Audience: Chromium build sheriff rotation members

This document describes how to be a Chromium sheriff: what your responsibilities
are and how to go about them. It outlines a specific, opinionated view of how to
be an effective sheriff which is not universally practiced but may be useful for
you.

[TOC]

## Sheriffing Philosophy

Sheriffs have one overarching role: to ensure that the Chromium build
infrastructure is doing its job of helping developers deliver good software.
Every other sheriff responsibility flows from that one. In priority order,
sheriffs need to ensure that:

1. **The tree is open**, because when the tree is closed nobody can make
   progress;
2. **New test failures are not introduced**, because they weaken our assurance
   that we're shipping good code;
3. **Existing test failures are repaired**, for the same reason

As the sheriff, you not only have those responsibilities, but you have any
necessary authority to fulfill them. In particular, you have the authority to:

* Revert changes that you know or suspect are causing breakages
* Disable or otherwise mark misbehaving tests
* Use [TBRs] freely as part of your sheriffing duties
* Pull in any other engineer or team you need to help you do these duties

Do not be shy about asking for debugging help from subject-matter experts, and
do not hesitate to ask for guidance on Slack (see below) when you need it. In
particular, there are many experienced sheriffs, ops folks, and other helpful
pseudo-humans in [slack #sheriffing].

## Tools Of The Trade

### A Developer Checkout

Effective sheriffing requires an [up-to-date checkout][get-the-code], primarily
so that you can create CLs to mark tests, but also so you can attempt local
reproduction or debugging of failures if necessary. If you are a googler, you
will want to have [goma] working as well, since fast builds provide faster
turnaround times.

### Slack

Sheriffing is coordinated in the [slack #sheriffing] channel. If you don't yet
have [Slack] set up, it is worth setting it up to sheriff. If you don't want to
use Slack, you will at a minimum need a way to coordinate with your fellow
sheriffs and probably with the ops team.

These are important Slack channels for sheriffs:

* [#sheriffing][slack #sheriffing]: for sheriffing
  tasks and work tracking
* [#ops][slack #ops]: for [infra
  issues][contacting-troopers] with gerrit, git, the bots, monorail,
  sheriff-o-matic, findit, ...
* [#halp][slack #halp]: for general chromium
  build & development issues - "no question too newbie!"

A good way to use Slack for sheriffing is as follows: for each new task you do
(investigating a build failure, marking a specific test flaky/slow/etc, working
with a trooper, ...), post a new message in the #sheriffing channel, then
immediately start a thread based on that message. Post all your status updates
on that specific task in that thread, being verbose and using lots of
detail/links; be especially diligent about referencing bug numbers, CL numbers,
usernames, and so on, because these will be searchable later. For example, let's
suppose you see the mac-rel bot has gone red in browser tests:

    you: looking at mac-rel browser_tests failure
        [in thread]
        you: a handful of related-looking tests failed
        you: ah, looks like this was caused by CL 12345, reverting
        you: revert is CL 23456, TBR @othersheriff
        you: revert is in now, snoozing failure

Only use "also send to channel" when either the thread is old enough to have
scrolled off people's screens, or the message you are posting is important
enough to appear in the channel at the top level as well - this helps keep the
main channel clear of smaller status updates and makes it more like an index of
the threads.

### Monorail

[Monorail] is our bug tracker. Hopefully you are already familiar with it.
When sheriffing, you use Monorail to:

* Look for existing bugs
* File bugs when you revert CLs, explaining what happened and including any
  stack traces or error messages
* File bugs when you disable tests, again with as much information as practical

### The Waterfall

The [CI console page], commonly referred to as "the waterfall" because of how it
looks, displays the current state of many of the Chromium bots. In the old days
before Sheriff-o-matic (see below) this was the main tool sheriffs used, and it
is still extremely useful. Especially note the "links" panel, which can take you
not just to other waterfalls but to other sheriffing tools.

### Sheriff-o-Matic

[Sheriff-o-matic] attempts to
aggregate build or test failures together into groups. How to use
sheriff-o-matic effectively is described below, but the key parts of the UI are:

* The bug queue: this is a dashboard of bugs that might be relevant to the
  on-duty sheriffs, such as known infra issues, failures that are under active
  investigation by engineering teams, and so on.
* The "consistent failures" list: these are failures that have happened more
  than once. These are **sometimes** actual consistent failures, and sometimes
  the same suite or bot failing twice in two different ways, so it is not always
  to be trusted but can be useful.
* The "new failures" list: these are singleton failures that have not yet become
  "consistent". These tend to disappear if a bot or suite goes green on a
  subsequent pass, so this section can be fairly noisy.

Sheriff-o-matic can automatically generate CLs to do some sheriffing tasks, like
disabling specific tests. You can access this feature via the "Layout Test
Expectations" link in the sidebar; it is called "TA/DA".

### Builder (or "Bot") Pages

Each builder page displays a view of the history of that builder's most recent builds -
here is an example: [Win10 Tests
x64](https://ci.chromium.org/p/chromium/builders/ci/Win10%20Tests%20x64). You
can get more history on this page using the links to the bottom-left of the
build list, or appending a `?limit=n` to the builder page's URL.

If you click through to an individual build ([Win10 Tests x64 Build
36397](https://ci.chromium.org/p/chromium/builders/ci/Win10%20Tests%20x64/36397)
for example), the important features of the build page are:

1. The build revision, which is listed at the top - this is key for knowing if a
   given build had a specific CL or not.
2. The blamelist (one of the tabs at the top) - this lists all CLs that were in
   this build, but not in the previous build for this builder. Note that this
   only shows **Chromium** changes; any changes that were incorporated via a
   DEPS roll of another repo won't be listed here, and you will instead have to
   look at the changelog for that DEP manually. There are usually links to views
   of those changes in the commit messages of DEPS roll CLs.
3. The list of steps, each of which has a link to its output and metadata.
   Especially important is the "lookup builder gn args" step, which tells you
   how to replicate the builder's build config (although probably you will want
   to use your own value of `goma_dir`).
4. Within individual steps, the "stdout" link and sometimes also the "shard"
   links, which let you read the builder's actual console logs during that step.
   Note that the sharded tasks are actually executing on different machines and
   are aggregated back into the builder's results.

### The Flake Portal (The "New Flakiness Dashboard")

The [new flakiness dashboard] is much faster than the old one but has a
different set of features. **Note that to effectively use this tool you must log
in** via the link in the top right.

To look at the history of a suite on a specific builder here:

1. In the "Flakes" view (the default), use Search By Tags
2. Add a filter for `builder` == (eg) `Win10 Tests x64`
3. Add a filter for `test_type` == (eg) `browser_tests`

To look at how flaky a specific test is across all builders:

1. In the "Flakes" view, use Search By Test
2. Fill in the test name into the default filter

This gives you a UI listing the failures for the named test broken down by
builder. At this point you should check to see where the flakes start
revision-wise. If that helps you identify a culprit CL revert it and move on;
otherwise, [disable the test][test-disable].

For more advice on dealing with flaky tests, look at the "Test Failed" section
below under "Diagnosing Build Failures".

### The Old Flakiness Dashboard

The [old flakiness dashboard] is often **extremely slow** and has a tendency not
to provide full results, but it sometimes can diagnose kinds of flakiness that
are not as easy to see in the new dashboard. It is not generally used these
days, but if you need it, you use it like this:

1. Pick a builder - ideally one matching the test failure you're investigating,
   or as similar to it as possible. After this, the dashboard will likely hang
   for some seconds, or possibly crash, but reloading the page will load the
   dashboard with the right builder set.
2. Pick the relevant test type. Do not pick a 'with patch' suite - these are
   from people doing CQ runs with their changes.
3. Find-in-page to locate the test you have in mind.

If you instead want to see how flaky a given test is across *all* builders, like
if you're trying to diagnose whether a specific test flakes on macOS generally:

1. Type a glob pattern or substring of the test name in the "Show tests on all
   platforms" field - this will clear out the "builder" dropdown.
2. Scroll down **past** the default results - the query results you asked for
   are below them.

In either view, you are looking for grey or black cells, which indicate
flakiness. Clicking one of these cells will let you see the actual log of these
failures; you should eyeball a couple of these to make sure that it's the same
kind of flake. It's also a good idea to look through history (scroll right) to
see if the flakes started at a specific point, in which case you can look for
culprit CLs around there. In general, for a flaky test, you should either:

* Revert a culprit CL, if you can find it, or
* [Disable the test][test-disable] it as narrowly as possible to fix the flake
  (eg, if the test is broken on Windows, only disable it on Windows)

For more advice on dealing with flaky tests, look at the "Test Failed" section
below under "Diagnosing Build Failures".

### <a name="tree"></a>Tree Status Page

The [tree status page] tracks and lets you
set the state of the Chromium tree. You'll need this page to reopen the tree,
and sometimes to find out who previously closed it if it was manually closed.
The tree states are:

* "Open": green banner. The CQ runs and commits land as normal
* "Closed": red banner. The CQ will not run; commits can still land via direct
  submit (which is how you'll land fix commits) or CQ bypass
* "Throttled": yellow banner. semi-synonymous with "closed". In theory this
  means "ask the sheriff before landing a change", but in practice this state
  is not used now and few people know what it means.

Note that various pieces of automation parse the tree state directly from the
textual message, and some pieces update it automatically as well - e.g., if the
tree is automatically closed by a builder, it will automatically reopen if that
builder goes green. To satisfy that automation, the message should always be
formatted as "Tree is $state ($details)", like:

* "Tree is open ('s all good!)"
* "Tree is closed (sheriff investigating win bot infra failure)"

Another key phrase is "channel is sheriff", which roughly means "nobody is
on-duty as sheriff right now"; you can use this if (eg) you are the only sheriff
on duty and you need to be away for more than 15min or so:

* "Tree is open (channel is sheriff, back at 1330 UTC)"

## The Sheriffing Loop

You are on duty during your normal work hours; don't worry about adjusting your
work hours for maximum coverage or anything like that.

Note that you are expected **not** to do any of your normal project work while
you are on sheriff duty - you are expected to be spending 100% of your work time
on the sheriffing work listed here.

While you are on duty and at your desk, you should be in the "sheriffing loop",
which goes like this:

### 1. Is The Tree Closed?

If yes:

* Start a new thread immediately in Slack - don't wait until you have started
  investigating or "have something to say" to do this!
* Start figuring out what went wrong and working on fixing it
* Once it's fixed, or you're pretty confident it's fixed, reopen the tree via the [Tree Status Page](#tree)

Do not wait for a slow builder to cycle green before reopening if you are
reasonably confident you have landed a fix - "90% confidence" is an okay
threshold for a reopen.

### 2. Are there consistent failures in sheriff-o-matic?

If yes:

* Start a new thread in Slack for one of the failures
* Investigate the failure and figure out why it happened - see the "diagnosing
  build failures" section below
* Fix what went wrong, remembering to file a bug to document both the failure
  and your fix
* Snooze the failure, with a link to the bug if your investigation resulted in a
  bug

### 3. Are there other red bots on the waterfall?

If yes:

* Start a new thread in Slack for one of the bots
* Investigate the bot - has it been red for a long time, or newly so? Is it an
  important bot to someone?
* Fix, notify, or bother people as appropriate

### 4. Does anything in the sheriff-o-matic bug queue need action?

If yes:

* Find the relevant Slack thread or start a new one
* Investigate the bug and see what needs to happen - ping the owner, see if the
  priority & assignment are right, or try fixing it yourself. Check if it still
  needs to be in the queue!

### 5. All's well?

If none of the above conditions obtain, it's time to do some longer-term project
health work!

* Look for disabled/flaky tests and try to figure out how to re-enable them
* Look through test expectation files and see if expectations are obsolete or
  can be removed
* Re-triage old or forgotten bugs
* Manually hunt through the flakiness dashboards for flaky tests and mark them
  as such or file bugs for them
* Honestly, maybe just take a break, then restart the loop :)

**Don't go back to doing your regular project work** - sheriffing is a full-time
job while your shift is happening.

## Diagnosing Build Failures

This section discusses how to figure out what's wrong with a failed build.
Generally a build fails for one of four reasons, each of which has its own
section below.

### Compile Failed

You can spot this kind of failure because the "Compile" step is marked red on
the bot's page. For this kind of failure the cause is virtually always a CL in
the bot's blamelist; find that CL and revert it. If you can't find the CL, try
reproducing the build config for the bot locally and seeing if you can reproduce
the compile failure. If you don't have that build setup (eg, the broken bot is
an iOS bot and you are a Linux developer and thus unable to build for iOS), get
in touch with members of that team for help reproing / fixing the failure.
Remember that you are empowered to pull in other engineers to help you fix the
tree!

### Test Failed

You can spot this kind of failure by a test step being marked red on the bot's
page. Note that test steps with "(experimental)" at the end don't count - these
can fail without turning the entire bot red, and can usually be safely ignored.
Also, if a suite fails, it is usually best to focus on the first failed test
within the suite, since a failing test will sometimes disturb the state of the
test runner for the rest of the tests.

Take a look at the first red step. The buildbot page will probably say something
like "Deterministic failures: Foo.Bar", and then "Flaky failures: Baz.Quxx
(ignored)". You too can ignore the flaky failures for the moment - the
deterministic failures are the ones that actually made the step go red. Note
that here "deterministic" means "failed twice in a row" and "flaky" means
"failed once", so a deterministic failure can still be caused by a flake.

From here there are a couple of places to go:

* If a set of related-looking tests have all failed, probably something in the
  blamelist caused it; the other option is that some framework or service that
  all of them depend upon flaked at the same time. One example of this kind of
  framework flake is the infamous [bug 869227](https://crbug.com/869227).
* If a single test failed, check the blamelist for any obvious culprits;
  otherwise, check the flakiness dashboard for that test and see if the test
  should be marked as flaky.
* If a LOT of tests failed, look for common symptoms in the output of a handful
  of them at random; this will probably point you at either a CL in the
  blamelist or some framework/service flake culprit. Also check the sheriff bug
  queue to see if there are possible causes listed there.

When debugging a `layout_tests` failure: use the `layout_test_results` step link
from the bot page; this will give you a useful UI that lets you see image diffs
and similar. This will be under "archive results" usually.

One thing to specifically look out for: if a test is often slow, it will
sometimes flakily time out on bots that are extra-slow, especially bots that run
sanitizers (MSAN/TSAN/ASAN) or debug bots. If you see flaky timeouts for a test,
but only on these bots, that test might just be slow. For Blink tests there is a
file called [SlowTests] that lists these tests and gives them more time to run;
for Chromium tests you can just mark these as `DISABLED_` in those configurations
if you want.

When marking a Blink test as slow/flaky/etc, it's often not clear who to send
the CL to, especially if the test is a WPT test imported from another repository
or hasn't been edited in a while. In those situations, or when you just can't
find the right owner for a test, feel free to TBR your CL to one of the other
sheriffs on your rotation and kick it into the Blink triage queue (ie: mark the
bug as Untriaged with component Blink).

### Infra Breakage

If a bot turns purple rather than red, that indicates an infra failure of some
type. These can have unpredictable effects, and in particular, if a bot goes red
after a purple run, that can often be caused by corrupt disk state on the bot.
Ask a trooper for help with this via [go/bugatrooper][bug-a-trooper]. Do not
immediately ping in [slack #ops] unless you have just filed (or are about
to file) a Pri-0 infra bug, or you have a Pri-1 bug that has been ignored for >2
hours.

### Other Causes

There are many other things that can go wrong, which are too individually rare
and numerous to be listed here. Ask for help with diagnosis in Slack #sheriffing
and hopefully someone else will be able to help you figure it out.

[CI console page]: https://ci.chromium.org/p/chromium/g/chromium/console
[SlowTests]: https://cs.chromium.org/chromium/src/third_party/blink/web_tests/SlowTests
[TBRs]: https://chromium.googlesource.com/chromium/src/+/master/docs/code_reviews.md#TBR-To-Be-Reviewed
[bug-a-trooper]: https://goto.google.com/bugatrooper
[contacting-troopers]: https://chromium.googlesource.com/infra/infra/+/master/doc/users/contacting_troopers.md
[get-the-code]: https://www.chromium.org/developers/how-tos/get-the-code
[goma]: http://shortn/_Iox00npQJW
[test-disable]: https://www.chromium.org/developers/tree-sheriffs/sheriff-details-chromium#TOC-How-do-I-disable-a-flaky-test-
[new flakiness dashboard]: https://analysis.chromium.org/p/chromium/flake-portal
[old flakiness dashboard]: https://test-results.appspot.com/dashboards/flakiness_dashboard.html
[sheriff-o-matic]: https://sheriff-o-matic.appspot.com/chromium
[slack #halp]: https://chromium.slack.com/messages/CGGPN5GDT/
[slack #ops]: https://chromium.slack.com/messages/CGM8DQ3ST/
[slack #sheriffing]: https://chromium.slack.com/messages/CGJ5WKRUH/
[slack]: https://chromium.slack.com
[tree status page]: https://chromium-status.appspot.com/
