# Mandatory Code-Review and Native OWNERS

Beginning on March 24, 2021, committers@ of Chromium are no longer able to
circumvent code review and OWNERS approval on CLs. The full
[Code Review](code_reviews.md) documentation has been updated to reflect this.

Previously, these were circumventable by self-code-review and because the
enforcement was done by presubmit, although rarely done by external
contributors. Now, Gerrit will disallow both bypasses. Within Google, where
these bypasses were more common, Googlers can find Google-specific information
in the internal announcements and landing site.

Periodic updates and FAQs will be sent to chromium-dev@chromium.org
and updated on this page.

## FAQS

### Do I need a reviewer to merge CL's to another branch, even though they were already reviewed on main?

Yes, but within 14 days of the original change you can add Rubber Stamper bot (rubber-stamper@appspot.gserviceaccount.com) as the reviewer.

### I have a question, whom should I contact?

Send questions about this document to chromium-dev@chromium.org. Googlers can
use an internal-specific email alias that was announced, separately.

### How will major refactorings be handled? I regularly need to submit 100s of CLs across the trees; getting OWNERS approval from everyone will be too hard.

We have created a process for landing such changes:
[Chrome Large Scale Changes](/docs/process/lsc/large_scale_changes.md).

This process allows approved, large refactorings to bypass OWNERS for the
duration, using a special label `Owners-Override`. However, these changes will
still need a second human (anyone with committers `Code-Review +1` powers) to
vote.

### What should I do when I need to get Owners-Override for one-off CLs?

One-off CLs do not need to go through Large Scale Changes. If the CLs make
only mechanical changes associated with changes in //base/ APIs, //build/ APIs,
//content/ APIs, //url/ APIs or //third_party/blink/public/APIs, the API owners can set `Owners-Override`.

For other one-off CLs, [Chrome ATLs](../ATL_OWNERS) can set `Owners-Override`.

### How does Rubber Stamper bot work?

Rubber Stamper applies the Bot-Commit label to conforming CLs, allowing them to
bypass code review. It supports various benign files, clean cherry-picks, and
clean reverts that should be exempt from code review.

Rubber Stamper never provides OWNERS approval, by design. It's intended to be
used by those who have owners in the directory modified or who are sheriffs. If
it provided both code review and OWNERS approval, that would be an abuse vector:
that would allow anyone who can create a revert or cherry-pick to land it
without any other person being involved (e.g. the silent revert of security
patches).

When you need to get Owners-Override for sheriffing CLs, reach out to Active
Sheriffs or Release Program Managers. If they are not available, send an email
to lsc-owners-override@chromium.org.

### Will trivial files require code-review?

Rubber Stamper auto-reviewer (described above) reviews CLs that meet strict
criteria. (The list of file types is Google-internal.) For example: directories
with no code.

Essentially, if we can programmatically prove that the CL is benign, then we
should allow a bot to rubber-stamp it so that Gerrit allows submission. One can
imagine that the classes of CLs that fit in this category would grow over time.

### Will clean cherry-picks on release branches need review?

Yes, Rubber Stamper adds CR+1 (Browser) to clean cherry picks. Adding the bot as
a reviewer to your CL will cause it to scan and approve it.
rubber-stamper@appspot.gserviceaccount.com is the bot but just typing "Rubber
St..." will autocomplete the full address for you. However, it doesn't provide
OWNERS approval so, if you are cherry-picking a CL that you don't have OWNERS
on, you can get that approval from the Release Program Manager who approved the
cherry-pick.

### Does documentation require code review?

Documentation will require code review.

There has been much discussion on this topic but senior leaders came to a
majority conclusion that the quality increase in documentation from requiring
code review outweighed any productivity headwinds.

We will revisit this in the future to evaluate how it is working (or not, as the
case may be).

### How do we ensure top-level and parent directory OWNERS aren't overloaded?

We updated the developer documentation stating that CL authors should
prioritize OWNERS closer to the leaf nodes and not to use top-level owners
because those folks are likely overloaded and the likelihood of a high response
latency or the CL getting lost is high. OWNERS recommendations from Gerrit are
in-line with this.

### Does Gerrit block direct push?

Yes. For break-glass scenarios, there are several folks who have the ability to
direct push, including others' CLs.

### OWNERS enforcement and no-TBR are different things. Why did they roll out simultaneously?

While they are separate, both impact the integrity of Chrome source code and
artifacts and have tangible impacts on developer workflows. For example: TBR was
used to bypass OWNERS and the rollout of this policy prevented this bypass. In
consultation with senior leaders, we decided that rolling both out
simultaneously allowed for more streamlined communication and change management
for the contributor community.
