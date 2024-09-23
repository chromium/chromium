# Chrome Large Scale Changes

[TOC]

*** promo
**Note**: this is heavily inspired by a [Google-internal process (link for Googlers only)](https://goto.google.com/lsc) with Chrome-specific modifications.
***

## What is it? {#what-is-it}

`chromium/src` has millions of lines of code. Sometimes, we want to change code that has been used for years and is referenced from thousands of places throughout the codebase. These sorts of changes are referred to as LSCs (Large Scale Changes) and generally require complex planning.

This page and the referenced docs describe the policies and best practices that go into the planning, creating, and executing changes that impact many distinct packages.

## What is it good for? {#what-is-it-good-for}

Fixing old mistakes, crufty interfaces, and bad usage requires making many small changes across the codebase. Individually, these changes are tiny, but taken all together, they allow us to deprecate old interfaces and pay down our technical debt. The simpler we can make the code base, the more that developers can focus on their actual problems, instead of coping with artificial complexity because of accumulated cruft.

Although there are many ways that these changes can be handled, most will be handled by an ATL reviewer and a domain reviewer.

## What is it NOT good for? {#what-is-it-not-good-for}

Given the cost of large scale changes (review cost often being a significant factor), we want to prioritize changes that have high value.

For example, while cosmetic changes have some positive value, we want reviewers to spend their time on higher value changes. This means that changes which are primarily cosmetic are rarely worth doing unless they interact with other automation, such as fixing a spelling mistake that then makes other refactorings work more consistently. If in doubt, send us your proposal before spending too much time on it.

Additionally, if a cross-cutting change reasonably fits into a small number of CLs, this LSC process is unnecessary. Just [use Owners-Override](/docs/code_reviews.md#global-approvals) to keep the reviewers to a minimum.

## Who uses it? {#who-uses-it}

Anyone making "large" changes. (Currently this is roughly defined as changes covering more than 10 distinct directories with OWNERS files.) We are aiming to enable a process where the processes and practices (using [`git cl split`](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/git-cl.html)) for making sweeping changes are more broadly accessible enabling teams to more readily change the APIs they export while keeping a bound on how much time is spent coping with codebase churn.

## Practical matters {#practical-matters}

### Why do we have this process? {#why-do-we-have-this-process}

At the highest level, we want to make sure anything going out in tens/hundreds/thousands of CLs is moving the codebase in the right direction and that it's worth the work it takes to get such changes tested, reviewed, and submitted. ATLs will rarely say "This isn't impactful enough," although it may happen (especially in cases where there is little or no distinct benefit). The more common role of the review is to ensure that the necessary documentation is in place: you will be sending CLs that are seen by dozens or hundreds of engineers who may not have any knowledge about your change. Some CL description updates and documentation can go a long way to making the process smooth.

In Chrome, we will grant the LSC Requestor the Gerrit Global Owners Approver power for the duration of their change, i.e. a member of ATLs will approve the LSC Proposal which when then gives the LSC Requester the ability to bypass OWNERS in the interest of efficiency. (For external contributors, a Googler sponsor will act as the Global Owners Approver power holder. If you’re an external contributor, everything else is the same.) Each CL will still need a second human to review it to satisfy “two sets of eyes” code review policy requirements. This reduces the code-review burden on the rest of Chrome reviewers and increases the rate at which your change gets submitted. However, this also means that these kinds of LSCs have a high bar of being low-risk and non-controversial.

As the LSC Requestor, the process of getting the LSC fully submitted looks like:

1. Create all of the CLs in the set, using a hashtag to identify them. This can be done in small batches (using automation or manually), if desired.
2. Use a command-line tool to mark them all of them as Owners-Override +1 using Global Owners Approver power (or have your Googler sponsor do it for you)
3. Ask a Chromium peer to use the same command-line tool to mass Code-Review +1 the set (spot-checking the CLs for correctness)

After the LSC is fully submitted, the LSC Requestor will lose their Global Owners Approver power.

If global owners approval does not seem appropriate, we will ask you to send your changes for local approval by owners of the individual directories you're changing.

### Multiple repositories {#multiple-repositories}

To build Chrome, we have many other repos which are included via DEPS. Most of these are under the ownership of the Chrome org and this process (and have identical policies). But, some are not. For those that are under Chrome, this policy applies to all repos. V8 and Skia are not yet included in this process.

As a LSC Requestor, you can split your CLs across these boundaries.

### Historical examples of candidates for this new process {#historical-examples-of-candidates-for-this-new-process}

* [Migrate from base::Bind() to base::BindOnce() or base::BindRepeating()](https://bugs.chromium.org/p/chromium/issues/detail?id=714018)
* [Restrict abilities of MessageLoop::current() (and ultimately remove it)](https://bugs.chromium.org/p/chromium/issues/detail?id=825327)

## Best practices {#best-practices}

### Docs for authors {#docs-for-authors}

* [Chrome LSC Template](https://docs.google.com/document/d/10S8ESUvwhEOOBEKr-hn97y8eRTYczavizsUNv5Gvcg8/edit) - If you only look at one thing, this should be it.
* [Chrome LSC Workflow](lsc_workflow.md) - The high-level steps to get a new LSC approved.

### FAQ for authors {#faq-for-authors}

* **Do I have to use this process?** This depends on how large your change is—specifically, how many child CLs your large refactoring will split it into—and whether all sub-CLs affect the same OWNERS.

  | Number of child CLs | Action                                                                                                                     |
  |---------------------|----------------------------------------------------------------------------------------------------------------------------|
  | >= 30               | Follow the [Chrome LSC Workflow](lsc_workflow.md) (i.e. create an LSC doc, get domain approval, email lsc-review@, wait for approval). |
  | < 29                | No requirements                                                                                                            |

  * If you would like to request global owners approval for safe or trivial changes of any size, please also follow this process and wait for the LSC committee to respond to your document.
  * If all sub-CLs affect the same OWNERS, you do not need to follow the LSC process and instead can talk to an area owner for how to proceed with your change.
* **What do I have to do?** Follow the steps at [Chrome LSC Workflow](lsc_workflow.md).
* **How long will this process take?** You should expect an initial response from a cleanup approver within two days.
* **I'm in a hurry; Is there a fast track?** ATLs may fast-track low-risk changes, with a response estimated between a few hours to one business day. If you'd like to request fast tracking, email [lsc-review](https://groups.google.com/a/chromium.org/d/forum/lsc-review) with a link to the doc that you created end of [Chrome LSC Workflow](lsc_workflow.md) and we'll get to it as quickly as we can.
* **Why this process?** Changes that operate broadly across the codebase affect many engineers and teams and these changes may generate discussion on the change or the best way to roll it out. This process is in place to make sure that these changes are useful, well-communicated, and to minimize the risk of having to attempt difficult rollbacks.
* **I have an idea for a change, but I’m not sure who to ask about it?** Definitely email the lsc-review@chromium.org list. We should be able to give you at least a vague idea of whether your idea has merit and who to approach as a domain expert to move ahead.
* **My LSC touches third_party, do I have to do anything special?** These changes are likely to be rare. If it does occur, please obey the normal rules for third_party changes. Be aware that some parts of third_party have an external codebase as their source-of-truth, and so are mechanically generated (e.g. by Copybara [[external](https://opensource.google/projects/copybara), [internal, Googlers only](http://go/copybara)]). This means that your changes will be overwritten by the next import.
* **My proposal was accepted for "local approval", now what?** Get your CLs submitted. For changes at this scale, it's usually best to use Find Owners and Auto-Submit. If anything surprising comes up during this process, or you need to ask questions to someone familiar with the process, try your assigned committee reviewer (see your LSC document). As with any wide-scale change, you should also consider announcing it on a mailing list that covers the target audience.
* **My proposal was accepted for "global approval", now what?** Contact your ATL reviewer: they should have granted you (or your Googler sponsor) Global Approver power. The techniques for getting these submitted via global owner approval vary based on what is being changed, your approver will know what's what for your particular LSC. As with any wide-scale change, you should also consider announcing it on a mailing list. Any coworker or contributor can be your second-set of eyes to mass Code-Review +1 all of your CLs.
* **My proposal requires multiple large changes, do I need multiple LSC docs & approvals?** No. You can use a single LSC document to describe your entire series of changes even if you are asking for a mix of local and global approvals. Be sure to break each change out into an easily identifiable step in the document.
* **How do I deal with backsliding?** Backsliding (others submitting changes which reverse the intended change) is something which cannot be completely avoided in all cases. A useful techniques to reduce them is to use Tricium (see [go/luci/tricium [internal, Googlers only]](https://goto.google.com/luci/tricium) for details) or Presubmit.

### Docs for Domain Reviewers {#docs-for-domain-reviewers}

* [The Chrome LSC Workflow](lsc_workflow.md)

### FAQ for Domain Reviewers {#faq-for-domain-reviewers}

* **How do I pick a domain reviewer?** Your domain reviewer should be someone with the expertise and authority to approve the technical direction of the change you're proposing. For example, if you're proposing migrating all users from library A to library B, your domain reviewer might be an OWNER of library A who can confirm the direction of your proposed LSC. This can also be someone from your team, if they're a domain expert. The idea is to get permission from anyone who could say "No, please roll back all these migration CLs."
* **Why am I being asked to comment?** You’ve been selected as a relevant owner/expert for a large-scale change. We need your input as to whether or not the change is a good idea. You don’t need to review in detail (like a code review), but we want to make sure the relevant parties are convinced that the change is good. What we don’t want is to have to roll back after getting part way through a change involving hundreds of CLs.
* **What if I disagree?** That is exactly why you’re being asked: if you don’t like the change, we want to stop it before it gets split up and submitted.
* **What if I think someone else should be contacted?** Absolutely bring them into the discussion. The goal is to make sure that no LSC-approved change will have to be rolled back because of a lack of information or planning. We want to bring any and all relevant experts and stakeholders into the discussion early to minimize the risks.

### FAQ for Local CL Reviewers {#faq-for-local-cl-reviewers}

* **How do I review this CL?** Because these tiny cleanup CLs may differ from your normal reviews, you can likely look at them in the context of the entire change process and its approval.
* **I don’t like this change.** You can say no, but must have a good reason. These changes generally involve mass refactoring - if you refuse the change, you may block the payoff of that refactoring for all of Chrome. Be prepared to justify your objections. If your objections to a CL amounts to wanting to do it differently, consider whether it's really worth objecting to - there is value in simply getting things done, and at our scale, it's not always possible to find a solution that is optimal for everyone. \
 \
That said, as with any CL, the author should be able to provide justification for it, and part of the purpose of LSC policy is to ensure that justification exists and is sufficient. You can ask the author about the change, but first read the description and CL email carefully, including any linked supporting documentation. You may find it interesting to know more about what's going on, and it may save the need for a one-on-one conversation. \
 \
If you wish to share your concerns, see the contact info below.
* **I get too many cleanup CLs.** We’ll try to use global owners approvers as often as possible to avoid spreading the burden to teams who maintain code/libraries commonly affected by refactorings.
* **I don't actually work on that anymore.** Have you considered removing yourself from the OWNERS file? If you aren't willing and capable of doing reviews, you probably shouldn't be an OWNER.

## History and evolution {#history-and-evolution}

Chrome’s LSC process was introduced to reduce the use of self-code-review to bypass OWNERS as we moved to make code review mandatory. It (and this doc) were heavily copied from a Google-internal process that has been in place for more than a decade. We needed an improved workflow for submitting large scale changes (i.e. refactorings or architectural changes) across a big codebase (e.g. chromium/src). Because it had been so slow to get OWNERS approval for dozens of directories across dozens of CLs, folks in chromium/src had the convention of code-review from a main owner and then TBR= every other owner.

## Whom to contact {#whom-to-contact}

* lsc-policy@chromium.org for questions about best practices, documentation, or this FAQ.
* lsc-review@chromium.org for questions about a particular change.

## Additional resources {#additional-resources}

* [Chrome LSC Workflow](lsc_workflow.md) - Overview of the steps you need to do in order to start a new Chrome LSC.
* [Chrome LSC Template](https://docs.google.com/document/d/10S8ESUvwhEOOBEKr-hn97y8eRTYczavizsUNv5Gvcg8/edit) - Use this form to propose a change.
