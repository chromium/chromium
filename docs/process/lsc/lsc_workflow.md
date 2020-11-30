# Chrome LSC Workflow {#chrome-lsc-workflow}

This document describes the workflow for getting a large-scale change approved ([Chrome LSC](large_scale_changes.md)).

*** note
Status: _**DRAFT**_

Editors: [jclinton@google.com](mailto:jclinton@google.com)
***

## 1 Complete the LSC template ([link](https://docs.google.com/document/d/10S8ESUvwhEOOBEKr-hn97y8eRTYczavizsUNv5Gvcg8/edit)) {#1-complete-the-lsc-template-link}

This short document will describe the large-scale change you want to make: What the change is, why you want to make it, estimated size (#CLs, #files), etc. Once you start sending out CLs, a link to that document should also be included in the description of each CL (more details at [Chrome LSC Template](https://docs.google.com/document/d/10S8ESUvwhEOOBEKr-hn97y8eRTYczavizsUNv5Gvcg8/edit)).

## 2 Request a domain review by filing a [bug](not.live.yet) {#2-request-a-domain-review-by-filing-a-bug}

Find [a domain reviewer](large_scale_changes.md#for-domain-reviewers) – someone who is knowledgeable in the area you are changing. Request a domain review for the LSC document you created in step 1 by filing a bug (template: [bug template](not.live.yet)) and assigning it to this person. The bug template contains the instructions for the domain reviewer (briefly, if the domain reviewer approves the proposed LSC, they should add an "LGTM" comment to the bug). Put a link to the bug in the table at the top of the doc.

## 3 Wait for approval by lsc-review@ {#3-wait-for-approval-by-lsc-review@}

Once the domain review is complete, the domain reviewer will assign the bug to [lsc-review@chromium.org](https://groups.google.com/a/chromium.org/d/forum/lsc-review), who will review your LSC request. You should expect an initial response within two business days. How long it takes until the request is approved depends on how complex the change is and how much needs to be discussed; for simple changes, it should only take a few days. **Final approval will be indicated by the LSC bug being marked FIXED**.

## 4 Start sending your CLs! {#4-start-sending-your-cls}

Once your LSC is approved (the bug from step 2 is marked FIXED) you may begin sending your CLs for review. You probably want to use [`git cl split`](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/git-cl.html) to do this. (Please don’t attach the LSC bug to your CLs!)

If your LSC was approved for "global owners approval", you should get in direct contact with someone who can mass code-review your CLs to coordinate sending the firehose of CLs their way. Good luck!

*** note
**TODO jclinton**: info on command line tool usage for mass-Owners-Override and mass-Code-Review
***

## Questions {#questions}

If at any point you have questions about this workflow, please contact [lsc-review@chromium.org](mailto:lsc-review@chromium.org) and we'll be happy to help.
