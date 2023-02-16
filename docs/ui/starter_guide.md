# Starter Guide

You just got tasked with implementing some new UI surface in Chrome, the UX
designer gave you a few mocks, and your tech lead told you they expect it should
only take you a few days. Help! You've never built UI before! What do you do?

Take a deep breath. We're here to help. Hopefully the following will get you
started.

## General Principles

* **Ignorance is ok.** UI is a specialized field, and if you haven't worked with
it before, there's no reason you should know everything. Even if you have done
UI development on other products or on the web, the Views toolkit has some
differences that might leave yourself scratching your head. Don't spend too long
trying to figure things out yourself;
[ask questions](https://chromium.googlesource.com/chromium/src/+/main/docs/ui/ask/index.md)
early and often, before you discover in code review that that lovely solution
you found is actually a pattern the team is trying to eliminate.

* **Don't cargo-cult.** Just as you may be ignorant, others may be too:
specifically, both the designers of your feature, and the authors of other UI in
Chrome. Blindly following is unsafe, so start by sending your feature to the
[desktop UI mailing list](http://go/cr-ui-process), even if you've been assured
it passes UX review. Then, while implementing, study the documented
[best practices](learn/index.md#best-practices), and ensure your code adheres to
them, even if other code you're referencing does not.

* **Support all users.** Chrome ships on
[multiple platforms](views/platform_style.md), so try to build and test on
multiple platforms yourself. Users have different themes, so make sure your
design works for the built-in light and dark themes
[as well as custom themes](create/examples/theme_aware.md). Users may have
different needs and abilities, so ensure your design takes
[accessibility](http://go/gar) sufficiently into account.

* **Build for the long term.** Chrome has a rapid release schedule in part so we
don't feel pressure to ship changes before they're ready. It's easy for
engineers to ship UI and leave it unmaintained, adding to the deifficulty of
future refactors and stylistic changes. So expect reviewers to set a high bar.
Ask how to structure your classes in keeping with MVC (Model-View-Controller)
principles. Write automated tests for your feature, including both functional
tests and pixel tests. And make sure your subteam, or another subteam, is
explicitly signed up to own the code you write going forward and has a triage
process to categorize and address bugs in it.

## Necessary Background

The Views toolkit was purpose-built to render Chrome's UI on desktop platforms.
It lacks many features of general-purpose UI toolkits, and adds features on
request, rather than speculatively. It's possible that over time, more UI will
be built with web technologies, as some of the original design constraints that
led to Views' creation become less applicable, and since it is difficult to
justify the engineering investment necessary to implement significant modern
UI toolkit features (e.g. declarative UI or stylesheets).

The best way to familiarize yourself with Views is to build a
[simple UI example](create/examples/login_dialog.md). This should give you
sufficient context to understand some
[important parts of the system](views/overview.md). Along the way, if you run
into jargon you don't understand, you can look for a definition in the
[glossary](learn/glossary.md) (or request one, if you don't see what you need).

At this point, you should be ready to start building your UI, following the
steps below.

## Building Your UI

1. Make sure there is a [bug](http://crbug.com/) on file with a description of
the UI you're building, and a link to any relevant design docs or mocks. Assume
readers aren't familiar with your subteam/product area already and provide
enough background that, without clicking through to other documentation, a new
code reviewer can understand the basic idea you're trying to accomplish.

1. If it hasn't already happened, send mail to the
[desktop UI mailing list](http://go/cr-ui-process) describing the proposed UX.
The primary purpose is for knowledgeable folks on this list to flag designs that
are atypical in Chrome or will be difficult to implement; these might require
further tweaks from the UX designers. This can be a frustrating process if you
assume UX mocks are set in stone, but the goal is not to derail your feature;
it's to ensure everyone is aware of what tradeoffs must be made to implement the
feature in a way that supports all users and is maintainable for the indefinite
future.

1. Ensure your UX mocks are semantic, not physical. That is, they should explain
what a feature does and how it relates to other similar UI (e.g. "use standard
dialog borders" or "match accent color used for radio buttons elsewhere"),
ideally by referencing standardized color or layout names/tokens; they should
not simply give you hardcoded values ("16 dp", "Google Grey 300"). Hardcoded
values cannot be systematized in a way that accommodates users with different
font sizes or themes, or changes over time in Chrome's systematic design
aesthetic. Semantics tell you how to obtain the desired values from classes in
Chrome that are designed to provide the appropriate physical values for your
context.

1. Look for similar UI implementations in Chrome and study how they function,
but don't blindly copy them. The majority of Chrome UI is years old and was
written before current best practices were established and documented, so while
existing code can be a good source of inspiration, it usually needs modification
before it's fit for reuse.

1. Add a [feature flag](/docs/how_to_add_your_feature_flag.md) to gate your UI,
and implement it locally. Some engineers prefer to send CLs as they implement
each small piece, while others prefer to get something mostly complete done
locally before polishing things for review; either way, ensure the changes you
send for review are small enough to be manageable but still provide sufficient
context for your reviewer to understand what's happening.

1. When preparing a CL for code review, run through
[this checklist](learn/bestpractices/prepare_for_code_review.md).

1. Before considering your feature complete, ensure it has automated testing. In
most cases, this can be accomplished using
[`TestBrowserUi`](/docs/testing/test_browser_dialog.md) or an appropriate
subclass; if the feature is built using MVC design principles, you can hopefully
also unit-test the business logic separately. If possible, opt in to pixel tests
using [Skia Gold](learn/glossary.md#skia-gold).

## Further Resources

* If you're a Noogler on the Chrome team, welcome! You should have gotten this
already, but
[here](https://sites.google.com/corp/google.com/chrome-top-level/more-resources/new-to-chrome)
are some hopefully-helpful docs as you ramp up on Chrome in general;
[here](/docs/contributing.md) are docs on the general contribution process.
* Is it possible to debug the UI that you're building? Yes!
[Here](learn/ui_debugging.md) are a few tools to help you.
