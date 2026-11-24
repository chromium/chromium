# How To Think About and Structure Your Chromium Migration

**TL;DR:** Before starting on a [migration](#what-is-a-migration), ensure that
you've answered the following questions:

1.  [Have you identified the key risk factors to reaching
    100%?](#identify-risks)
2.  [How can you structure the migration to "fail fast" on those key risks (and
    preserve full rollback)?](#fail-fast)
3.  [Does value require reaching 100%, or can you define incremental landing
    points?](#incremental-landing-points)
4.  [How will you handle the operational grind?](#operational-grind)
5.  [Is your project actually a migration, or is it a
    "both-and"?](#migration-or-both-and)
6.  [Have you fully leveraged AI to de-risk the hard parts up
    front?](#leveraging-ai)

## Overview

To avoid Chromium ossifying, we periodically need to undertake large
cross-platform migrations from an existing subsystem to a new one.

However, because Chromium supports so many products and platforms, maintains a
high bar for relative performance and correctness during transitions, and sees
team priorities shift over long timelines, **migrations frequently end up
unfinished**. We then end up supporting both the old way and the new way in
production indefinitely. This carries a high ongoing cost for maintenance,
robustness, and flexibility, especially as partial migrations interact in a
combinatorial explosion of flows and can leave Chromium *worse off* than before
the migration started.

This doc offers guidance based on our collective experience undertaking
migrations in Chromium (often learned the hard way, since many of these
dynamics are only obvious in hindsight!). It is framed as a set of questions to
ask yourself at the start of a migration to give your effort the best chance of
success and to avoid leaving Chromium indefinitely supporting two competing
systems.

## What Do We Mean by a "Migration"? {#what-is-a-migration}

In this doc, a **migration** means transitioning Chromium from an **existing
subsystem** that provides some functionality (e.g., metrics collection) to a
**new subsystem** (supporting either the existing API surface or a new one),
where carrying both the old way and the new way implies non-trivial extra
maintenance and/or cognitive load, and where the migration is **finished** only
when the old subsystem is no longer in use and deleted.

*(Note: Incremental codebase-wide style or idiom improvements, such as "prefer
`std::views` when touching code", do not carry that dual-subsystem burden and
aren't the focus of this guide; see [the rubric](cl_rubric.md) for guidance on
those changes.)*

## Questions to Ask at the Start of Your Migration

### 1. Have you identified the key risk factors to reaching 100%? {#identify-risks}

In our experience, two obstacles account for many of the cases where a
migration gets blocked from reaching 100% and deleting the old code:

*   **A platform or legacy use case that simply _can't_ be ported:** For
    example, an obscure platform constraint or legacy client (or external
    callers relying on undocumented side effects or event ordering) that
    fundamentally doesn't fit the new model, only discovered deep into
    execution.
*   **"Code-complete, stuck in Finch forever":** Getting the code landed behind
    a flag (or launched on easy platforms), only to spend years unable to
    launch to 100% and delete the old code path due to performance, memory, or
    correctness regressions on the most constrained platforms or flows.

Before starting, put in the cycles up front to audit *all* platforms and
callers for unportable edge cases, and identify which platforms or flows will
be the hardest for meeting Chromium's performance, memory, and stability bar at
100% Finch rollout. Where the old subsystem is selected or fallen back to at
runtime (e.g., a legacy backend or persisted format where code search alone
cannot show what still uses it in the wild), add localized telemetry at the
legacy subsystem's entry point up front so you can see which configurations
actually hit it (and later verify usage has reached 0% before removing it).

### 2. How can you structure the migration to "fail fast" on those key risks (and preserve full rollback)? {#fail-fast}

When building a new feature, a natural instinct is to build and launch on the
simplest client or platform first to show momentum. In a migration, doing the
easiest targets first commits us to supporting the new way in production
*before* we know whether the hardest cases in [Question 1](#identify-risks) are
solvable.

Instead, structure the migration to **fail fast**:

*   Prove out (and benchmark in production) the hardest platforms, clients, or
    performance-sensitive flows first, so that you can bail out cleanly if
    those risks prove insurmountable.
*   Preserve the ability to do a full rollback to the old way until you are
    confident the new way can launch to 100% (or until you have reached a
    defined incremental landing point; see
    [Question 3](#incremental-landing-points)). In general, runtime feature
    flags (per Chromium's
    [Flag Guarding Guidelines](flag_guarding_guidelines.md)) should be the
    baseline mechanism for preserving rollback, especially as shorter release
    cycles leave less pre-Stable soak time to shake out regressions.
    Additionally, ensure that you design persisted state for safe rollback up
    front and trace what happens if a user rolls back after hitting the new
    format - aim to avoid data loss, corruption, or subtle state divergences.

### 3. Does value **require** reaching 100%, or can you define incremental landing points? {#incremental-landing-points}

Many migrations are designed as "all-or-nothing", where the real benefits only
materialize at 100% when the old subsystem is finally deleted. In a
multi-quarter effort, team priorities or staffing can shift halfway through; if
the migration is all-or-nothing, stopping early leaves Chromium strictly *worse
off* by carrying the permanent burden of two systems in production.

Ask up front:

*   Is your migration dependent on reaching 100% to unlock its key benefits?
    (If so, preserving rollback per [Question 2](#fail-fast) is especially
    critical.)
*   If not, how can you structure the migration into phases with **incremental
    landing points**, where if you have to stop after Phase 1, real benefits
    have already been locked in and the ongoing complexity of coexistence is
    low and bounded?
*   Can you define concrete metrics (often domain-specific, such as incoming
    bug rate or crash class frequency) to illustrate the problem or complexity
    you are aiming to reduce, and to measure both progress toward completion
    and the value delivered at each stage?

### 4. How will you handle the operational grind? {#operational-grind}

Even when a migration is technically proven, the standard 80/20 rule applies:
completing the first 80% of the migration often takes 20% of the time, while
the final 20% takes 80% of the time. To avoid stalling out in the long tail,
make sure your plan addresses four operational questions:

*   **How and when will you turn off the tap on the old way?** Without early
    guardrails (such as presubmits, `DEPS`, or visibility restrictions), other
    engineers across Chromium will keep adding *new* callers or features to the
    old subsystem while you are migrating existing ones.
*   **Who will actually write the long-tail CLs?** Expecting dozens of client
    teams with their own priorities to migrate their own working code rarely
    succeeds. Plan and staff for the migrating team to drive the caller
    migrations directly, only needing client owners for review. For Googlers
    (or Googlers assisting in migrations driven by external contributors), you
    must ensure that the migration follows
    [go/chrome-churn-policy](http://go/chrome-churn-policy). For non-Googlers
    looking to drive external migrations without a Googler partner, you should
    be in touch with [Chrome ATLs](mailto:chrome-atls@google.com) while early
    in the proposal phase. Note that once the upfront risks are retired (see
    [Question 6](#leveraging-ai)), you can and should lean into AI coding tools
    to accelerate progress on fanout (while abiding by Chromium's
    [AI coding](../agents/ai_policy.md) policy).
*   **How will you handle other projects concurrently touching the same
    codepaths?** It can arise that concurrent projects need to operate on the
    same shared surfaces, which can increase complexity and require cross-team
    sequencing and coordination. Try to identify engineers/teams with whom you
    should stay in contact during the migration to identify and mitigate this
    risk.
*   **How will you handle the cost of supporting both ways while the migration
    is in progress?** Account for the ongoing cost of testing, monitoring, and
    debugging both the old and new subsystems in production for the duration of
    the migration.

### 5. Is your project actually a migration, or is it a "both-and"? {#migration-or-both-and}

There are valid use cases for transitioning some platforms or clients to a new
way of doing `<X>` while others remain on the old way. A common example is a
new code path or architecture that *could* in principle be supported across all
platforms, but brings a compelling benefit only to one platform (meaning that
porting the remaining platforms would be a long tail of work that is unlikely
to actually be prioritized).

Be internally clear at the start about whether your realistic goal is
**removing** the old way everywhere (a migration) or **selectively adding** a
new way alongside the old way (a "both-and"). If it is a "both-and", your
proposal should:

*   Make it explicit that you are not proposing to remove the old way.
*   Define the exact target configurations for the new way.
*   Explicitly weigh the benefit of having those targets on the new way against
    the permanent cost of supporting both ways indefinitely.

### 6. Have you fully leveraged AI to de-risk the hard parts up front? {#leveraging-ai}

AI has the potential to impact the questions above both negatively and
positively. AI agents can make it tempting to immediately convert the easiest
80% of mechanical call sites and land a bunch of CLs on main. In a migration,
doing the easiest parts even faster just locks Chromium into a dual-system
state faster without retiring any of the real risks in Questions 1-4 (note that
once those risks **are** retired, AI is a great fit for driving the fanout in
[Question 4](#operational-grind)).

Ask how you can use AI **before** deep resource investment or landing
commitments on main:

*   Can you use AI up front to audit all existing callers and surface
    "unportable" legacy edge cases?
*   Can you use AI to prototype a rough conversion of the hardest or most
    performance-sensitive target (or even a 100% conversion) on a branch, so
    that you can test and benchmark it before committing on main?
