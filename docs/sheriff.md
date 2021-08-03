# Chromium Sheriffing

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
* Use Owners-Override label to override OWNERS checks freely as part of your
  sheriffing duties
* Pull in any other engineer or team you need to help you do these duties
* For clean reverts and cherry-picks, add the
  [Rubber Stamper bot](code_reviews.md#automated-code_review). All other
  changes require a +1 from another committer.

TBRs were removed in Q1 2021.

For more information on Chromium Trunk Sheriffs, including How Tos, Swapping
Shifts and rotation updates, please see [Chromium Trunk Sheriffing](http://goto.google.com/chrome-trunk-sheriffing)

## How to be a Sheriff

To be a sheriff, you must be both a Chromium committer and a Google employee.
For more detailed sheriffing instructions, please see the internal documentation
at
[go/chrome-sheriffing-how-to](https://goto.google.com/chrome-sheriffing-how-to).

## Contacting the Sheriffs

The currently oncall sheriffs can be viewed in the top-left corner of the
[Chromium Main Console](https://ci.chromium.org/p/chromium/g/main/console). You
can also get in touch with sheriffs using the
[#sheriffing Slack channel](https://chromium.slack.com/messages/CGJ5WKRUH/).
