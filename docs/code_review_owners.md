# Mandatory Code-Review and OWNERS

Beginning in Q1 2021, committers@ of Chromium will no longer be able
to circumvent code review and OWNERS approval on CLs.

Currently, these are circumventable by self-code-review and because the
enforcement is done by presumit, although rarely done by external
contributors. In Q1, Gerrit will disallow both bypasses. As part of the
transition, an audit service will automatically file bugs for CLs that
land with only self-approval, launching in Q4 2020. Within Google, where
these bypasses are more common, Googlers can find Google-specific
information in the internal announcements and landing site.

Periodic updates and FAQs will be sent to chromium-dev@chromium.org
and updated on this page.

## FAQS

Q: Do I need a reviewer to merge CL's to another branch, even though they were
already reviewed on main?

A: Yes, but within 7 days of the original change you can add the Rubber-Stamper
bot (rubber-stamper@appspot.gserviceaccount.com) as the reviewer.
