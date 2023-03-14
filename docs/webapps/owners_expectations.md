# Expectations for `chrome/browser/web_applications/OWNERS`

The `chrome/browser/web_applications` directory can have some high-risk areas for changes. We have an OWNERs policy to ensure the code and/or design can safely achieve its goals.

Becoming an owner is an extra responsibility and we should just have enough to balance the load, but it is not necessary for everyone.

These guidelines are open to modification. Feel free to contact the team for exceptions and process changes.

## Expectations of owners

*   See [/docs/code_reviews.md#expectations-of-owners](../code_reviews.md#expectations-of-owners), but instead of 'last 90 days', 6 months is fine.
*   Familiar with the project [README](README.md), architecture, common system constructs, testing, & the bespoke integration tests system.
*   Routes questions to appropriate specialists for sensitive areas (e.g. windows os integration code, app service integration, etc).

Expected code review feedback examples (to give an idea of the types of things OWNERs should be thinking about when reviewing code):

*   "This code is modifying attributes of a WebApp without a lock in an async sequence - please wrap this operation in a command so it has a lock and it can be held across all steps."
*   "In this test use an `OsIntegrationTestOverride` here so you can check if the OS integration occurred correctly (and to ensure we aren't causing OS integration on trybots)."
*   "Make sure you disable OS integrations by using an `OsIntegrationManager::ScopedSuppressForTesting` Or maybe use our common testing base-class?"
*   "This is adding a whole new database, and we already have too many sources of truth. If we use the wrong architecture for data storage, we can't easily fix it with a refactor, it needs an expensive and risky migration. Can we use an existing one?"
*   "We already have a place where we store information per browser window/tab (WebAppController/WebAppTabHelper). What do you think about putting that logic there?"
*   "This functionality seems like it should sit in its own manager as it's a nicely contained responsibility that can be tested independently. Would that work with your requirements here?"

## Becoming an owner

To become an owner

*   Fulfill the above expectations.
*   Contact an owner to do shadowed reviews. Over the next week or so the owner will send you a few reviews that you will do a first pass, and afterwards the owner will give constructive feedback.
    *   The owner will add their comments on the review, send them privately, and/or give them in-person. In-person feedback is often more helpful, as it allows faster Q&A.
*   After the owner is comfortable & any feedback is addressed, they will create a change to add you to the OWNERS file and justify the addition in the review commit.
    *   This usually requires more than one shadow review.
*   Done!
