# New Extension and Platform App APIs

[TOC]

## Summary

Chrome exposes special capabilities to Extensions and Platform Apps through
different APIs.  The core API system is shared between Extensions and Platform
Apps; APIs are defined and exposed in the same fashion for both.

Before implementing a new API, it has to go through an approval process.  This
approval process helps ensure that the API is well defined, does not introduce
any privacy, security, or performance concerns, and fits in with the overall
product vision.

## API Details

### Public or Private
__Default: Public__

Extension and App APIs can either be public (available for any extension or app
to potentially use, though frequently there are other constraints like
requiring a permission) or private (only available to extensions or apps with a
specific allowlisted ID).  In general, private APIs should only be used for
pieces of functionality internal to Chromium/Chrome itself (e.g., the
translation utility, printing, etc).  Public APIs should always be the default,
in order to foster openness and innovation.

Good reasons for a private API might be:
* __The API is needed by a core feature of Chrome/Chromium.__  Note that even
here, there can be exceptions, if the API could be reasonably implemented as
a way to benefit external extensions.

Bad reasons for a private API might be:
* __The API is only needed for a Google property (other than Chrome/Chromium).__
In the spirit of openness, we should, when possible, provide people with the
means to build alternatives.  Just because something is needed by a Google
property does not mean it wouldn’t be useful to a third-party.
* __The API is needed by a Google property (other than Chrome/Chromium), and
is too powerful to expose to any third-party extension or app.__  Generally, if
an API is too powerful to expose to a third-party extension, we don’t want to
expose it to any kind of (non-component) extension, as it increases Chrome’s
attack surface.  Typically, these security concerns can be addressed by finding
alternatives or tweaking the API surface.

* __This is just a quick-and-dirty API and we don’t want to go through a long
process.__  Quick-and-dirty hacks have a nasty habit of staying around for
years, and often carry with them their own maintenance burdens.  It’s very
frequently cheaper to design an API well and have it be stable than to have a
quick solution that has to be constantly fixed.

Unless there is a compelling reason to make an API private, it should default
to public.

### API Ownership
In general, __you__ (or rather, your team) will own the API you create.  The
Extensions and Apps teams do not own every API, nor would it be possible for
those teams to maintain them all.  This means that your team will be
responsible for maintaining the API going forward.

### A Note on Chrome App APIs

Platform apps have been [deprecated on all platforms](https://blog.chromium.org/2020/01/moving-forward-from-chrome-apps.html).
We are not actively expanding the Chrome Apps platform.

### Platforms

__Extensions Default: All Desktop Platforms__
Extensions are supported on all desktop platforms (Windows, Mac, Linux, and
ChromeOS).  By default, an extension API will be exposed on all these
platforms, but this can be configured to only be exposed on a subset.  However,
an API should only be restricted if there is strong reason to do so; otherwise,
platforms should have parity.

__Platform Apps: N/A__
Platform apps are deprecated on all platforms.

## Proposal Process

Starting the review process __early__ is encouraged, and if some of the
artifacts are missing or in progress, we’re happy to work with you on it.  It’s
better to have us review the API in principle to ensure it’s something we’re
comfortable adding to the platform, and then work out the details, than to
invest heavily only to find out that we don’t want to add the API to the
platform.  Feel free to file a bug without everything ready, or to email
extension-api-reviews@chromium.org for advice and feedback!

### File a Bug

In order to propose a new API, file a new bug with the appropriate
[template](#bug-templates), and fill in the required information.  This
should include:

__API Namespace:__ The namespace of your API.  This is how your API will be
exposed in JS. For instance, the chrome.tabs API has the 'tabs' namespace.

__Target Milestone:__ The target milestone for releasing this API.  It’s okay
for this to be a rough estimate.

__API OWNERS:__ Usually, you will be responsible for ownership of the API.
List appropriate usernames, team aliases, etc.

__API Overview Doc:__ A link to your completed
[API Overview document](https://docs.google.com/document/d/182XXEPwbh5dyMTO_Q3bZ9k4PYY19Woydu5b3XvPoSmc/edit#).

__Design Doc(s):__ Any additional design docs.  Depending on the complexity of
the API, this might not be necessary with the API Overview doc above.

__Supplementary Resources (optional):__ Any additional resources related to
this API.  For instance, if this API is part of a larger feature, any launch
bug, PRDs, docs, mocks, etc for that feature can be linked here (or through an
associated crbug.com issue).

__Note:__ This process does not eliminate the need for a larger design review
or launch bug, if one would otherwise be required.  See
go/chrome-dd-review-process for guidance (sorry, internal only.  If this is a
large feature, we recommend finding a member of the chrome team to help you
drive it and own it).  However, it should be possible to get feedback from many
of the required parties during that review process, which would expedite the
additional approvals needed.

Please email a link to the bug and proposal to
extension-api-reviews@chromium.org for increased visibility and any additional
feedback.

### Get Sign-Off

All APIs, public or private, will need sign off from a few different parties.
The [API Overview document](https://docs.google.com/document/d/182XXEPwbh5dyMTO_Q3bZ9k4PYY19Woydu5b3XvPoSmc/edit#)
has the a section for these sign offs. Please reach out to the relevant teams
to get sign off.

__API Review:__ The overall review of the API, including comments on exposed
methods, events, or properties, and an overall review for whether the API fits
in with the overall product vision.  This sign off will come from a member of
the Extensions or Apps team.

__Security Review:__ A review to ensure that the API will not pose any
unnecessary security risk.

__Privacy Review:__ A review to ensure that there are no privacy concerns
around leaking user data without permission.

__UI Review:__ A review of any UI implemented as part of your API.  This review
may not be necessary if there is no UI element to your API.

__Legal Review:__ A review to ensure there are no legal issues with the new API.

In general, we encourage you to get explicit sign off from all teams, even if
you think the scope and overlap may be small.  If it is indeed easy and
uncontroversial, the reviews should be simple and fast.

## Modifying an Existing API
Modifications to an existing API should go through a similar process.  Since
modifications to these APIs are frequently far-reaching, please do not skip the
proposal process!  However it might be possible to skip or expedite it in
certain specific cases. In particular:
* Small changes (like adding a new property to a method) to private APIs can
  skip the review process unless //extensions OWNERS explicitly ask for it.
  Small changes to non-private APIs are still required to go through this
  process but may not require a full design doc or API Overview doc.
* Medium-sized changes, like adding a single new method, are still required to
  go through the review process. Whether an API overview doc is required is up
  to the discretion of the API reviewers.
* Larger changes, like adding multiple new methods and events, should still
  include an API Overview (though it may be condensed).

## FAQ
__Do I need an API review for a private API?__  Yes! Private APIs are not
as scrutinized as public APIs because we don't need to be as worried about
API ergonomics, and we can be a little more lenient in security. However,
we still need to review the API to make sure that:
* The API is secure. Even though it runs in trusted extensions, it exposes
capabilities to a renderer process, and may also introduce vulnerabilities
elsewhere.
* The API meets privacy guidelines. We hold ourselves to a strict standard in
regards to what data we can collect.
* The API should be a private API. There are multiple alternatives, including a
public API, a web API, implementing code directly in C++, and others. Private
APIs are not always the appropriate choice.

__Does this replace a launch bug?__ No, the API proposal process is orthogonal
to a launch bug. Launch bugs are required for all major features in
Chrome/Chromium. If the API is part of a larger feature, a launch bug may still
be required. Additionally, a launch bug does not replace the need for an API
review, since the API review evaluates API-specific aspects (such as best
practices, API "fit", etc) that are not covered in the launch bug's
cross-functional reviews. However, if there __is__ a separate launch bug, some
cross-functional reviews (like security and privacy) may directly carry over.

## Bug Templates
__Note:__ All these templates default to public visibility.

[New Extension API](https://bugs.chromium.org/p/chromium/issues/entry?labels=Pri-2%2CType-Feature%2CNeeds-API-Review&components=Platform%3EExtensions%3EAPI&summary=New+Extension+API%3A+%3CAPI+Name%3E&description=%3Cb%3ENew+Extension+API+Proposal%3C%2Fb%3E%0A%0A%3Cb%3EAPI+Namespace%3A%3C%2Fb%3E+%5BAPI+Namespace+Here%5D%0A%3Cb%3EAPI+Owners%3A%3C%2Fb%3E+%5BTeam+Members%2C+Team+Aliases%5D%0A%3Cb%3EAPI+Overview+Doc%3A%3C%2Fb%3E+%5BLink+to+doc.+Template+is+at+https%3A%2F%2Fdocs.google.com%2Fdocument%2Fd%2F182XXEPwbh5dyMTO_Q3bZ9k4PYY19Woydu5b3XvPoSmc%2Fedit%23%5D%0A%3Cb%3EDesign+Doc%3A%3C%2Fb%3E+%5BLink+to+design+doc.+This+might+be+unnecessary+with+the+overview+doc+above%2C+in+which+case+this+can+be+N%2FA%5D%0A%3Cb%3ESupplementary+Resources%3A%3C%2Fb%3E+%5BAny+supplementary+resources+for+your+API+or+a+larger+feature+that+your+API+is+a+part+of%2C+such+as+PRDs%2C+docs%2C+mocks%2C+etc.%5D)

[New Platform App API](https://bugs.chromium.org/p/chromium/issues/entry?labels=Pri-2%2CType-Feature%2CNeeds-API-Review&components=Platform%3EApps%3EAPI&summary=New+Platform+App+API%3A+%3CAPI+Name%3E&description=%3Cb%3ENew+Platform+App+API+Proposal%3C%2Fb%3E%0A%0A%3Cb%3EAPI+Namespace%3A%3C%2Fb%3E+%5BAPI+Namespace+Here%5D%0A%3Cb%3EAPI+Owners%3A%3C%2Fb%3E+%5BTeam+Members%2C+Team+Aliases%5D%0A%3Cb%3EAPI+Overview+Doc%3A%3C%2Fb%3E+%5BLink+to+doc.+Template+is+at+https%3A%2F%2Fdocs.google.com%2Fdocument%2Fd%2F182XXEPwbh5dyMTO_Q3bZ9k4PYY19Woydu5b3XvPoSmc%2Fedit%23%5D%0A%3Cb%3EDesign+Doc%3A%3C%2Fb%3E+%5BLink+to+design+doc.+This+might+be+unnecessary+with+the+overview+doc+above%2C+in+which+case+this+can+be+N%2FA%5D%0A%3Cb%3ESupplementary+Resources%3A%3C%2Fb%3E+%5BAny+supplementary+resources+for+your+API+or+a+larger+feature+that+your+API+is+a+part+of%2C+such+as+PRDs%2C+docs%2C+mocks%2C+etc.%5D)

[Extension API Modification](https://bugs.chromium.org/p/chromium/issues/entry?labels=Pri-2%2CType-Feature%2CNeeds-API-Review&components=Platform%3EExtensions%3EAPI&summary=Extension+API+Modification%3A+%3CSummary%3E&description=%3Cb%3EExtension+API+Modification+Proposal%3C%2Fb%3E%0A%0A%3Cb%3EAPI+Namespace%3A%3C%2Fb%3E+%5BAPI+Namespace+Here%5D%0A%3Cb%3EAPI+Owners%3A%3C%2Fb%3E+%5BTeam+Members%2C+Team+Aliases%5D%0AThe+following+documents+may+not+be+necessary+depending+on+the+scope+of+your+proposal%3A%0A%3Cb%3EAPI+Overview+Doc%3A%3C%2Fb%3E+%5BLink+to+doc.+Template+is+at+https%3A%2F%2Fdocs.google.com%2Fdocument%2Fd%2F182XXEPwbh5dyMTO_Q3bZ9k4PYY19Woydu5b3XvPoSmc%2Fedit%23%5D%0A%3Cb%3EDesign+Doc%3A%3C%2Fb%3E+%5BLink+to+design+doc.+This+might+be+unnecessary+with+the+overview+doc+above%2C+in+which+case+this+can+be+N%2FA%5D%0A%3Cb%3ESupplementary+Resources%3A%3C%2Fb%3E+%5BAny+supplementary+resources+for+your+API+or+a+larger+feature+that+your+API+is+a+part+of%2C+such+as+PRDs%2C+docs%2C+mocks%2C+etc.%5D)

[Platform App API Modification](https://bugs.chromium.org/p/chromium/issues/entry?labels=Pri-2%2CType-Feature%2CNeeds-API-Review&components=Platform%3EApps%3EAPI&summary=Platform+App+API+Modification%3A+%3CSummary%3E&description=%3Cb%3EPlatform+App+API+Modification+Proposal%3C%2Fb%3E%0A%0A%3Cb%3EAPI+Namespace%3A%3C%2Fb%3E+%5BAPI+Namespace+Here%5D%0A%3Cb%3EAPI+Owners%3A%3C%2Fb%3E+%5BTeam+Members%2C+Team+Aliases%5D%0AThe+following+documents+may+not+be+necessary+depending+on+the+scope+of+your+proposal%3A%0A%3Cb%3EAPI+Overview+Doc%3A%3C%2Fb%3E+%5BLink+to+doc.+Template+is+at+https%3A%2F%2Fdocs.google.com%2Fdocument%2Fd%2F182XXEPwbh5dyMTO_Q3bZ9k4PYY19Woydu5b3XvPoSmc%2Fedit%23%5D%0A%3Cb%3EDesign+Doc%3A%3C%2Fb%3E+%5BLink+to+design+doc.+This+might+be+unnecessary+with+the+overview+doc+above%2C+in+which+case+this+can+be+N%2FA%5D%0A%3Cb%3ESupplementary+Resources%3A%3C%2Fb%3E+%5BAny+supplementary+resources+for+your+API+or+a+larger+feature+that+your+API+is+a+part+of%2C+such+as+PRDs%2C+docs%2C+mocks%2C+etc.%5D)
