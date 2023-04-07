# Respectful Changes
## A Guide for Code Authors

_For the code reviewer counterpart, see
__[Respectful Code Reviews](cr_respect.md)__._

## Set up for success

#### Do the pre-work

Help challenging code reviews go smoothly by reaching out to prospective
reviewers before writing any code. Describing the problem and your approach
ahead of time reduces surprise and provides an opportunity for early input.
Ensure the decisions resulting from these exchanges, as well as the reasoning
behind them, are accessible to others (e.g. via bug or design doc).

#### Mind your reviewer

Make choices that spare your reviewer time or cognitive load, such as preferring
a series of short changes to a massive one, or uploading separate patches to
isolate rebases during review.

#### Satisfy preconditions

Ensure your code is ready for review before you send it: it should compile, have
adequate testing that passes, and respect the style guide (using _git cl
format/lint_ is encouraged). Consider validating this by performing a
self-review. This is respectful of reviewer time and can sometimes save you a
review round trip. If you're looking for an early review that's fine too, but
please say so.

#### Remember communication can be hard

Differences in understanding or opinions are to be expected in the context of
code reviews. Always assume competence and goodwill. Don't hesitate to suggest a
quick meeting (face-to-face or via VC); sometimes it's much faster to resolve an
issue that way than email ping pong.

## Request the review

#### Choose your reviewers

Give thought to whether you want to serialize or parallelize your reviews. If
you're new to the codebase, it's a good idea to do a first round with a single
local reviewer to clear the basic issues. Try to limit the number of owners you
solicit (only one per section), but ensure you pick sufficiently specialized
ones. Finally, be mindful of time zones and their effect on the review cycle
time. Picking the right reviewers comes with experience, but you can start by
looking at OWNERS files, asking a teammate, or using tools ('_git cl owners_',
[Chromite Butler](https://chrome.google.com/webstore/detail/chromite-butler/bhcnanendmgjjeghamaccjnochlnhcgj)).

#### Provide context

Change descriptions are the first impression your change makes, both on
reviewers and on code archeologists from the future. A [good description](contributing.md#Uploading-a-change-for-review)
aims to do two things. First, it conveys at a glance the high level view.
Second, it provides references to all the relevant information for a deep dive:
design docs, bugs, testing instructions. The bug\# is a useful reference, but
isn't sufficient on its own. Summarize **what** and **why** in the description.
You can additionally provide guidance on how to do the review in the e-mailed
message.

#### State your expectations

When sending the review, be clear to your reviewer about your expectations. In
terms of the review, this means specifying the kind of reviewing (e.g., high
level) as well as who should review what using which level of scrutiny. In terms
of timing, this means stating your deadline or lack thereof. For tight
deadlines, be convinced your urgency is real (hint: should be rare), and
communicate its reason, as well as your intent to land required follow up
refactorings.

## During the review

#### Expect responsiveness

Getting your code reviewed is about getting unblocked. You should expect
reviewer input within 1 business day. This should however be modulated based on
the size, complexity, urgency / importance of your change, as well as on
time zone differences. Beyond that, double check the reviewer's code review tool
nickname (e.g. "_jdoe (OOO til 4 Apr)_"), their calendar and ping them on IM. If
that fails, look for another reviewer.

#### Address all comments

Be convinced your reviewers feel all comments have been addressed before you
commit. Questions are addressed by providing an answer. Suggestions can be
addressed in one of three ways: adopt it immediately ("Done."), defer it to a
subsequent change (TODO with a bug \#) or push back with additional
information. Whenever more information is required, make sure everyone agrees on
the problem before you discuss the solution and consider expanding the
documentation.

#### Wait for LGTM from all your reviewers

As a general rule of thumb, if a reviewer has made a comment on your CL, even
though you may have addressed that comment in a new patchset, don't submit the
CL until you have their LGTM, unless the reviewer gave the OK to do so (e.g.
when the reviewer delegates the reviewing task to someone else). If you need to
land a CL urgently and one of your reviewers isn't available (e.g. OOO), submit
your CL, and send your reviewer a note; in the note, be sure to include the
reason why you had to land the CL, and show that you've considered their
opinions & are ready to promptly act on their additional comments in a followup
CL.

#### What to do if it's going wrong

Code reviews should not make you feel bad. If you find yourself in that
situation, or you feel the review's at an impasse, don't attempt to work around
a reviewer but take a step back. A face to face meeting or a VC can sometimes
help unblock a review. If this doesn't sound like an option, or simply if you
feel you need to talk about it, reach out to someone you trust.

## After the review

Code reviews are in large part about having others watch your back. Don't
hesitate to say "Thank you" once the review is completed. Additionally, if
you're new to code reviews, take a few moments to reflect on what went well or
didn't.
