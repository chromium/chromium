# Accountability of Authors and Reviewers in the Age of Agents

Agentic engineering tools have made creating and evaluating technical artifacts
(CLs, design docs, etc.) much cheaper. For a massive and mission-critical
project like Chromium, expert human judgement is still essential for product
quality and maintainability, and so human review (at some level) remains a
critical part of our engineering practices.

This is resulting in a **shifting of engineering costs** from authors (who we
have lots of, and tend to get the most credit for the work) to expert reviewers
(who are harder to come by), resulting in an unsustainable burden on our
project. While there are no easy answers, one tool is good **communication
between authors and reviewers for effectively balancing their respective
responsibilities and workloads**.

## Principles

1.  **Authors are fundamentally accountable for the correctness and quality of
    their work.**

    Regardless of the tools used, authors retain full accountability for their
    work. Eg. for understanding the implications of their change, addressing any
    regressions promptly, making emergency fixes, and writing postmortems. This
    is encoded in our [AI coding policy](../agents/ai_policy.md). Authors are
    also responsible for making their changes easy to understand and review,
    including prefactoring (refactoring existing code prior to adding a new
    feature), design docs, tradeoff evaluation and risk analysis. Our product
    quality hinges on each and every author acting like an owner.

2.  **Reviewers are accountable for providing timely feedback and approvals.**

    Our productivity hinges on every reviewer providing constructive feedback or
    approval as [quickly as
    possible](code_reviews.md#expectations-for-all-reviewers). Delayed reviews
    have a negative compounding effect on productivity - i.e. waiting a week for
    a review buys the reviewer just one week of something else they can
    accelerate, while it costs the author both a week of delay AND both the
    author and reviewer the cognitive burden of needing to re-familiarize
    themselves with the state of the work.

3.  **Authors are accountable for learning from feedback.**

    In addition to maximizing the quality of the immediate work, the more
    important part of code/doc review is to help authors grow to depend less on
    reviewers - thereby raising the capacity of the whole project. Authors must
    consider not just how to apply reviewer feedback in the immediate artifact,
    but how to reduce the need for such feedback in the future.

4.  **Reviewers are accountable for maximizing the ROI of their reviewing
    time.**

    The time of our experts is our project's most scarce resource. Reviewers
    must carefully focus their time to provide the most benefit to the project.
    For example, when review is helping a new author grow and learn and
    ultimately take on more review responsibility themselves, it tends to be a
    net-positive use of reviewer expertise. If review comments are just being
    fed into an agent to update an artifact and then forgotten, this can be a
    net-negative use of scarce reviewer time.

## Guidance for reviewers

If you get a doc or CL which, on first glance, has obvious problems, don't feel
you need to spend your time finding and correcting all the errors (violating
both principle #1 and #4). At the same time, you should not simply delay (#2) or
decline to review (depriving authors of #3). Instead your responsibility to
maximize value to the project at large means you should find a way to **make
your review investment contingent on a proportionate investment from the
author**. For example, replying with a statement like the following:

> This CL (or document) seems to have several pretty obvious problems, such as X
> and Y. As such this is not something I can afford to invest in trying to
> understand in its current state. Please take a careful pass over this work,
> thoroughly checking it for accuracy and completeness yourself. You may find it
> useful to review documents A, B and/or talks C, D, or to have your agent
> generate prototypes and analysis of the tradeoffs involved in approach X, Y or
> Z. Let me know when you have built confidence in the architecture, conciseness
> and quality of this work and then I will be happy to take another look.

In some cases the root problem may be that the author is attempting to tackle a
problem that they are realistically not yet equipped to be accountable for
themselves. In such a case you may want to make suggestions like:

*   Writing a *short* design doc first, focusing on exploring tradeoffs to some
    specific questions, backed by agent-generated prototypes which compare
    different approaches.
*   Breaking the problem down to focus first on an initial step that is more
    achievable.
*   Building expertise by working to solve some simpler problems in the same
    area, suggesting specific "starter bugs" which may be worth working on.
*   Reaching out for help from an expert who can take over the job of breaking
    the problem down into more tractable sub-problems.

## Guidance for authors

Understand that your job is not to find working code to solve a problem, agents
can do that without you now. It's your job to learn how to effectively direct
the engineering of a large complex system. Rather than take pride in how many
CLs you land, try to learn to take pride in:

*   How small and easy to review can you make your design docs and CLs while
    achieving their goal? Avoid overly verbose code, tests, and commit messages
    that obscure a change's purpose.
*   How many CLs did you land which required no substantial changes in review?
    (nits are fine)
*   How many CLs did you land which clearly improved codebase simplicity or
    product quality?
*   Of the review feedback you've been given, how much of it have you (along
    with your agent) internalized such that you will not need to get the same
    feedback again, and could even give it to others when appropriate?
*   How often were you able to chime in on someone else's review in a way that
    was helpful to the OWNER? How has your work on this CL helped you be a step
    closer to being ready to be an OWNER in this area yourself?

Appreciate that reviewers are overwhelmed and so will need to shift more work
onto you. This is good, this is how you avoid being replaced by a robot. Help
focus their attention by leaving concise notes for reviewers (particularly on
larger changes) covering:

*   Overall concept and structure of the change.
*   Areas where the reviewer's expertise is most needed.
*   Notable alternatives that were considered, when applicable.
*   Risk assessment, calling out any behavior changes (especially if not
    [flag-guarded](flag_guarding_guidelines.md)).

When communicating with reviewers, also think how you can uplevel the
conversation to learn from them on how they'd approach finding answers, rather
than just ask to be given answers. Eg:

*   Before you review in detail, do you have any suggestions for risks and
    tradeoffs I should be evaluating?
*   Are there any extra steps I could take to make this problem easier to reason
    about?
*   What might I look for in order to get a better handle on how these types of
    problems have been solved in the past?
*   Make sure you are sending reviews to people who will be comfortable pushing
    back on you (especially if you are more senior).

In all cases, in addition to such candor it is important for both authors and
reviewers to have empathy for the challenge their counterpart is facing.
Ultimately we all want Chromium to remain a welcoming and supportive open source
community, while also increasing our productivity, product quality and codebase
health.
