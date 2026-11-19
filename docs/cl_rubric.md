Rubric for evaluating cost/benefit of CLs
=========================================

We have a steady stream of proposed large-scale changes.

Some of them are LLM generated, some aren’t. We used to have these before LLMs
existed, so this is somewhat independent of LLMs, but it applies to
LLM-generated CLs as well.

Here are some things to try and quantify the cost-benefit ratio of such changes
a bit. In the end, it will almost always be a judgement call, but the hope is
that it will provide useful inputs to make this call more informed.

Often, people use their spare time to fix something that personally bothers
them. They reason that since this makes the authoring cost basically zero, the
benefit of “fixes a personal bother” makes the cost/benefit worth it. This is
only true if the other costs are also basically zero. (Which might be true, but
also mightn’t.)

Metrics
-------

### Estimating benefit

Sometimes easy, if it helps memory, performance, build time, or something else
easily quantified. Do quantify it if you’re in this bucket though!

But often: Tricky! How do you e.g. quantify the benefit from going from
manually-managed memory to `unique_ptr`? (Honest question! In that case, you
might look through past leak and use-after-free bugs, say something like “Leak
bugs are 3x more likely to not use `unique_ptr`, we have N uses of `delete` in
the codebase, in a small sample I looked at, 1 out of 20 `delete` users
accidentally leaked memory while it was only 1 out of 50 for `unique_ptr`, so
this will reduce number of leaks by N / 20 - N / 50 = (30 / 1000) N. 1 in 10
leaks is P1, so if N is 10000, this will prevent 3 P1s.”)

In the tricky case, one way is to find one or more domain experts who agree
with your assessment that this is a good change. Sometimes, they even have
metrics already.

### Authoring cost

This one’s easy to measure. Someone has to make the change. Sometimes that’s
writing a sed script, using an LLM, running a clang-based rewriter, using an
LLM to create a clang-based-rewriter, or just manual typing. (For large-scale
changes, the latter is rare though.) Just check how long it took you to produce
the change. The author of a CL sees primarily this cost.

### Reviewer cost

There’s the time to understand why this change is being made (one-time per
reviewer), and the time to do the review (from 1 s / file for mechanical
changes to 30 min / CL for subtle changes that need research for past CLs,
bugs, etc). We ask reviewers to be responsive, so if you send out CLs to many
reviewers, you’re giving relatively urgent work to many people.

For very mechanical changes, you can use the LSC process to amortize the
one-time cost by using only a small number of reviewers that opt in to getting
CLs from you.

### Chance of regressions

If a change isn’t trivially behavior-preserving (e.g. replacing a base::
algorithm with an std:: variant where we spent a lot of time making sure they
behave the same and have the same API), it’s possible that the change will
introduce a bug. You can lower costs here by considering how likely this is,
think of likely failure cases, and save the reviewers some work by putting the
likely ways your change could change behavior and how you thought about this
and how you made sure it’s fine in the CL description.

### Git blame cost

There’s two parts to this:

1. `git blame` gets noisier every time we make a change. While this is a
   popular concern, in the view of the author of this document, this is a) a
   technical problem that can be solved b) mostly solved
   (.git-blame-ignore-revs, tools/uberblame.py, …).

2. `git blame` is what many scripts use to assign bugs to people. If you touch
    a large number of lines, you’ll get a bunch of bugs assigned to you and
    you’ll spend some time reassigning them to other people. This is especially
    true if you touch something further down the stack.

### Cost to changing well-known patterns

If you’re changing something that hundreds of people have used for many years,
you’re causing some amount of retraining cost. If possible, find an automated
way to prevent the old pattern from resurfacing while including a pointer to
the new pattern, so authors can naturally learn it (e.g. a PRESUBMIT.py error
with a message pointing to some docs).

### Cost from causing merge conflicts

If you touch many files over a short amount of time, you make merges harder.

### Opportunity cost

Are there other things we could do instead that would be more worth it? (It’s
easy to use this point to convince yourself that nothing is worth doing though,
so if the answer is “uh, probably?” and not “yes, there’s this concrete list of
other things I’d like to do that are all better”, then this is probably good
enough.)

### Other externalities

There might be other per-project costs, such as build time increase.

Sideways changes
----------------

Code can be written in a multitude of ways, many of which work. Different
people will prefer different ways of writing the same thing. Refrain from the
temptation to change things from one way to another unless you have a better
reason than personal preference.

Examples of using the metrics
-----------------------------

### go/crbug-todo-migration

* <https://docs.google.com/document/d/1moEwYhXgUL-QzhC7K82QlQqffhyCBtUfB9EBi8B8IE4/edit?tab=t.0>
* Benefit: Can paste bug IDs into codesearch to see if they are referenced.
* Authoring cost: < 1 day
* Review cost: 6200 CLs, trivial to review. Total cost to reviewers is a couple hours, way less than a day.
* Chance of regressions: Comment-only, close to 0.
* Git blame cost: Low since comment-only.
* Cost due to changing patterns: 0
* Cost from causing merge conflicts: Somewhat high.
* Opportunity cost: “Uh, probably?”, so ok. Part of finishing a migration.

=> Cheap, somewhat useful, might as well.

### LLM-automated stale TODO removal

* Benefit: Don’t have to wonder if a TODO in the code is still relevant
* Authoring cost: Run an LLM in a loop
* Review cost: Have to check for every TODO if it’s still relevant right now.
* Regressions, blame, patterns: 0
* Conflicts: TODOs are not super dense, so ok
* Opportunity cost: Was used to create LLM tech, so was looking for something
  with low risk. There are many things that seem more useful (dead feature
  removal, `unsafe_todo`, …).

=> “I think these TODO removal CLs cost more in reviewer time than they give
back in code health improvement.” seemed like the general sentiment.

### More

Add your example here!

Resources
---------

Vaguely related:

* <https://chromium.googlesource.com/chromium/src/+/HEAD/docs/process/lsc/lsc_workflow.md>,
  in particular “2 Request a domain review”
* <https://chromium.googlesource.com/chromium/src/+/HEAD/docs/process/lsc/large_scale_changes.md>
* [go/blink-compat](https://docs.google.com/document/d/1RC-pBBvsazYfCNNUSkPqAVpSpNJ96U8trhNkfV0v9fk/edit?usp=sharing) (public)
* go/chrome-churn-policy (internal only, sorry)
