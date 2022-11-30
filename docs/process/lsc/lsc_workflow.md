# Chrome LSC Workflow {#chrome-lsc-workflow}

This document describes the workflow for getting a large-scale change approved ([Chrome LSC](large_scale_changes.md)).

## 1 Complete the LSC template ([link](https://docs.google.com/document/d/10S8ESUvwhEOOBEKr-hn97y8eRTYczavizsUNv5Gvcg8/template/preview)) {#1-complete-the-lsc-template-link}

This short document will describe the large-scale change you want to make: What the change is, why you want to make it, estimated size (#CLs, #files), etc. Once you start sending out CLs, a link to that document should also be included in the description of each CL (more details at [Chrome LSC Template](https://docs.google.com/document/d/10S8ESUvwhEOOBEKr-hn97y8eRTYczavizsUNv5Gvcg8/edit)).

For Googlers, if the details are Google-private, you can do the same thing but with your google.com account. Share comment access to google.com, in this case.

## 2 Request a domain review {#2-request-a-domain-review-by-filing-a-bug}

Find [a domain reviewer](large_scale_changes.md#for-domain-reviewers) – someone who is knowledgeable in the area you are changing. Request a domain review for the LSC document you created in step 1 creating a document comment thread and assigning it to this person. The comment thread should include the instructions for the domain reviewer (briefly, if the domain reviewer approves the proposed LSC, they should add an "LGTM", their name, and the date to top of the doc).

## 3 Wait for approval by lsc-review@ {#3-wait-for-approval-by-lsc-review@}

Once the domain review is complete, you should email the doc to [lsc-review@chromium.org](https://groups.google.com/a/chromium.org/d/forum/lsc-review), who will review your LSC request. You should expect an initial response within two business days. How long it takes until the request is approved depends on how complex the change is and how much needs to be discussed; for simple changes, it should only take a few days. **Final approval will be indicated by a member of the LSC committee granting a reviewer Owners-Override approval power to land your LSC**. Put a link to the [chromium.org lsc-review Group thread](https://groups.google.com/a/chromium.org/g/lsc-review) in the table at the top of the doc.

## 4 Start sending your CLs! {#4-start-sending-your-cls}

Once your LSC is approved (Owners-Override is granted) you may begin sending your CLs for review. You probably want to use [`git cl split`](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/git-cl.html) to do this.

## Questions {#questions}

If at any point you have questions about this workflow, please contact [lsc-policy@chromium.org](mailto:lsc-policy@chromium.org) and we'll be happy to help.
