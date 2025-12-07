# Chromium Gardening

Gardeners have one overarching role: to ensure that the Chromium build
infrastructure is doing its job of helping developers deliver good software.
Every other gardener responsibility flows from that one. In priority order,
gardeners need to ensure that:

1. **The tree is open**, because when the tree is closed nobody can make
   progress;
2. **New test failures are not introduced**, because they weaken our assurance
   that we're shipping good code;
3. **Existing test failures are repaired**, for the same reason

As the gardener, you not only have those responsibilities, but you have any
necessary authority to fulfill them. In particular, you have the authority to:

* Revert changes that you know or suspect are causing breakages
* Disable or otherwise mark misbehaving tests
* Use Owners-Override label to override OWNERS checks freely as part of your
  gardening duties
* Pull in any other engineer or team you need to help you do these duties
* For clean reverts and cherry-picks, add the
  [Rubber Stamper bot](code_reviews.md#automated-code_review). All other
  changes require a +1 from another committer.

TBRs were removed in Q1 2021.

There are a number of different gardening rotations. For more information on
gardening, including How Tos, Swapping Shifts and rotation updates, please see
[Chromium Trunk Gardening](https://goto.google.com/chrome-gardening)
(Google-internal link).

## How to be a Gardener

To be a gardener, you must be both a Chromium committer and a Google employee.
For more detailed gardening instructions, please see the internal documentation
at
[go/chrome-gardening-how-to](https://goto.google.com/chrome-gardening-how-to)
(Google-internal link).

## Contacting the Gardeners

The currently oncall gardeners can be viewed in the top-left corner of the
[Chromium Main Console](https://ci.chromium.org/p/chromium/g/main/console). You
can also get in touch with gardeners using the
[#gardening Slack channel](https://chromium.slack.com/messages/CGJ5WKRUH/).

## Please don't pass bugs back to the gardener that assigned them to you
As part of their role, gardeners will triage open test failures and flakes. If
possible, they will identify a culprit CL and revert it; however, sometimes this
is not feasible. In that case, gardeners will assign these bugs to appropriate
owners. They typically do this by looking for:

1. The test author, or last person to make significant changes, or
2. A proximal OWNER of the test

_Pro-tip: Gardeners, identify yourself in your comments, e.g., "[Gardener]
assigning to the test author for further triage."_

If you are assigned a bug by a gardener, please don't pass the bug back to that
person. Gardeners have likely never seen the code before (or since), and are
unlikely to be much help. Additionally, if >8 hours have passed, that person is
no longer gardener, and thus no longer responsible for triaging these bugs.

Instead, if you aren't the best owner for the bug, please help to triage it
more appropriately, since you're probably the test author, familiar with the
test, or an OWNER. If have no idea who a good owner for the test is (or were
assigned the bug in error\*), you can reapply the `Gardener-Chromium` label and
flip the status to `Untriaged`; this will put it back in the gardener queue for
the next gardener to take a look at. Please only do this as a last resort, since
the next gardener is unlikely to have any more information about the issue.

\*If you believe you were assigned the bug in error, it might be worth finding
out why the gardener passed it to you, and remedying it if possible - e.g. by
updating OWNERS files.
