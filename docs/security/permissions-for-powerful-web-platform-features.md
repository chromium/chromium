# Controlling Access to Powerful Web Platform Features

_Author: [dominickn@chromium.org](mailto:dominickn@chromium.org)_
_Contributors: [rorymcclelland@chromium.org](mailto:rorymcclelland@chromium.org)_

# Overview

[Fugu](https://blog.chromium.org/2018/11/our-commitment-to-more-capable-web.html)
 is a renewed effort to bring
 [powerful new capabilities](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=Proj%3DFugu+&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids)
to the web -- e.g. filesystem read/write access. Allowing users to control
which sites are able to access such APIs is crucial for maintaining the
security and privacy properties of the web. The impact of restrictions on the
developer ergonomics and user utility of the API and the web platform overall
must also be considered.

This document explores approaches to guarding powerful APIs, e.g. using
[installed web app state](https://developers.google.com/web/progressive-web-apps/)
or some other proxy for high user
[engagement](https://www.chromium.org/developers/design-documents/site-engagement).
The following general principles summarise the overall approach of the
Chromium project to evaluating how powerful new features should be controlled
on the web:

+   __Access to powerful APIs__ should be available to the entire web platform
    of secure contexts, with control managed exclusively by choosers, prompts,
    or other user consent UX at time-of-use.
+   __API-specific restrictions__ on the scope of access may also be used to
    guard against potential abuse (e.g. constraints on available directories /
    files, no access without a currently open foreground tab).
+   Usage of powerful APIs should be clearly __disclosed__ to users, ideally
    using a central hub that offers users __control__ over what sites can use
    which capabilities.
+   Installing a web app is associated with __persistence__, and thus
    persistent and/or background access to powerful APIs may only be granted
    (possibly subject to additional requirements) to installed web apps.
    Non-installed sites may still request and be granted permission to use
    powerful APIs, but should not have their access persisted.
+   Installation or engagement alone __should not act as a vote of trust__ for
    either granting access or enabling the ability to ask for access to
    powerful APIs.
+   Separately, efforts should be made to __curtail the existing persistency__
    on the web platform outside of installed web apps, e.g. time-limiting
    permission grants, more aggressively expiring cookies, and restricting
    background task execution.

The remainder of this document explains the reasoning behind these principles,
and summarises why alternative proposals were not taken up.

# Definition of Terms

+   [__Powerful Web Platform APIs__](https://www.chromium.org/Home/chromium-security/prefer-secure-origins-for-powerful-new-features)
    are capabilities which carry inherent security or privacy risks when used,
    but also provide users of web apps with significant utility. A canonical
    example is native file system access (i.e. allowing web sites to directly
    read and write from certain locations on the user's device). Many such
    capabilities already exist on the web in every browser (e.g. access to
    camera and microphone hardware), while new capabilities are under constant
    development.
+   [__Installation__](https://www.w3.org/TR/appmanifest/#installable-web-applications)
    is the process where a web site may be elevated to run with a more native
    UX treatment on a given platform and device. It is usually tied with being
    granted a presence in the platform launcher (e.g. the desktop).
+   [__Progressive Web Apps__](https://developers.google.com/web/progressive-web-apps/)
    (PWAs) are web sites which are designed to be installable.
+   [__Engagement__](https://www.chromium.org/developers/design-documents/site-engagement)
    is a mechanism for measuring how much users interact with a site. Higher
    engagement may be a signal that the user derives significant utility from
    a site.


# Principles for Access to Powerful APIs

This section outlines the general principles that the Chromium team believes
are critical when designing access to powerful new web platform APIs. These
principles will be considered when evaluating how new APIs are designed.

## Control and Transparency

+   Users must be able to see when the powerful APIs in use, and what sites
    are using them.
+   Users may revoke access to powerful APIs at any time, and preferably,
    the revocation UI should be intuitive to locate based on how permission is
    granted and/or disclosed to the user.
+   The principle of least privilege should apply as broadly as possible:
    users should not have to grant access to more resources than necessary to
    achieve their goals.

## User Ergonomics

+   The web platform should be fully functional without requiring
    installation.
+   Sites must not be able to easily socially-engineer the user into
    granting permissions they wouldn't otherwise want to.
+   As much as is practical, users should not be bombarded with permission
    prompts, as this leads to decision fatigue.

## Developer Predictability

+   Developers must be able to know when they can and cannot access
    powerful APIs.
+   Access mechanisms should be cross-browser compatible.

# Proposal

### Baseline: secure contexts, top-level frames, user gesture

Minimally, all new powerful APIs must only be available in
[secure contexts](https://www.chromium.org/Home/chromium-security/deprecating-powerful-features-on-insecure-origins).
Ideally, availability is restricted to top-level frames and requires a user
gesture to trigger. When a webpage is running in a secure context in
a top-level frame with an active user gesture, we call this situation
a __baseline context__.

[Browser extensions](https://developer.chrome.com/extensions) are also
included as a baseline context, as they can already make use of web platform
APIs (subject to the same access checks as web sites).

The
[permission delegation](https://docs.google.com/document/d/1x5QejvpyQ71LPWhMLsaM1lWCfSsBsSQ8Dap9kJ6uLv0/preview?ts=5b857603#heading=h.ib6rctasbt3y)
mechanism may be used to extend privileges to iframes on a page if it makes
sense for a powerful capability to be delegated in this way.

### The entire web platform may access new powerful APIs

In general, any baseline context may access powerful APIs, regardless of its
windowing state (in the tabbed browser or in a standalone app window),
installation state (installed or not), or user engagement (highly interacted
with or not). This avoids the __fragmentation of the web__ into different,
sometimes unpredictable states, and encourages careful consideration of new
API surfaces such that they are exposed in a way that is safe for the web at
large.

### Session-based access is granted by direct user consent

In general, access to powerful APIs must be mediated by direct, informed user
consent while the requesting site is open in a foreground tab via mechanisms
which may include:

+   choosers
+   prompts

These mechanisms must clearly disclose the origin of the request, and follow
Chromium's
[guidelines on displaying URLs](https://chromium.googlesource.com/chromium/src/+/master/docs/security/url_display_guidelines/url_display_guidelines.md).
Implementations may be tested using tools such as
[Trickuri](https://github.com/chromium/trickuri).

As much as possible, APIs should avoid a "double prompt", e.g. a permission
prompt requesting access to the file system, followed by a chooser to pick the
file/directory to access. There is little security or privacy benefit to such
a double prompt, and it detrimentally affects user and developer ergonomics.

There are cases where double prompts are unavoidable, e.g. a web site may
request access to contact information, and if the user grants access, the
browser may need to request OS-level permission to service the request.

In some cases, Chromium may implicitly grant access to an API if it is not
particularly dangerous or does not make sense to guard behind a permission
consent. An example of this is the
[Badging API](https://github.com/WICG/badging/blob/master/explainer.md), which
only works for installed web apps, and results in a subtle badging effect on
the installed app icon that is not invasive or privacy-sensitive. These cases
should be relatively rare considering the powerful APIs that are covered by
this document.

The scope of access to APIs follows the web's same-origin model.

### Persistent and/or background access is restricted to installed apps

The only definite capability granted by installation is __persistence__. By
installing, the user has explicitly indicated that they want the web app to
have a persistent presence on their system.

New powerful APIs should exclusively use session-based permissions for web
sites that are not installed. In particular, there should be no access while
the site is not open in a tab, and access cannot be requested from a
non-foreground tab. When the site is closed or navigated away from, it loses
any granted access to powerful APIs it had, and must re-ask for access the
next time the user visits.

Installed web sites _may_ instead receive a permanent grant, which is removed
when the site is uninstalled. In this way, installed web sites _may_ be
granted the ability to access capabilities in the background, depending on the
particular details of each capability. It also avoids overloading the
installation decision with consequences that users may not expect.

Persistency for installed web sites may have other requirements, but
non-installed sites may never receive persistent grants to access powerful new
APIs.

Some powerful APIs act as a proxy for persistence (e.g. a web site with
permission to write files to disk). We distinguish persistence via a currently
granted capability from __persistent access to the capability itself__; it is
the latter privilege which is granted by being installed.

### Disclose access and provide obvious user controls

It should be obvious to users when sites are using powerful APIs, and they
should be given the tools needed to effectively manage and revoke access when
necessary.

### Improve permissions and installation UX to avoid decision fatigue

Removing persistent access from the drive-by web may drive up
[decision fatigue](https://en.wikipedia.org/wiki/Decision_fatigue) due to
overprompting. However, we anticipate that many of these capabilities will
have relatively niche applications that the drive-by web should not commonly
access.

Repeated granting of access to powerful APIs can be used as a signal for
installation. For instance, after two successful powerful permission grants,
Chromium could present the user with the option to install the app on the
third permission request.

### Administrator policies may override prompts and enforce persistence

Powerful new capabilities may be paired with
[Chromium policies](https://cloud.google.com/docs/chrome-enterprise/policies)
which permit administrators to enforce persisted access to capabilities
without prompts. Capabilities may also be restricted or blocked by such
policies. This is in line with how many existing permissions have admin policy
overrides.

### Explore ways of curtailing the lifetime of existing persistence

Currently, web sites which are not installed have access to significant
persistence mechanisms:

+   all existing permissions have their decisions persisted indefinitely
+   cookie and local storage lifetime may be indefinite

To better align the existing web to the proposal presented here, we suggest a
parallel effort to apply new lifetime limits on existing persistence
mechanisms. For example, some of the following measures could be explored:

+   forget any granted permissions if the site has not been visited in X weeks
    +   this could be challenging to apply for some capabilities such as push
        notifications, where there is a use case for a site being able to send
        informative notifications without ever needing to be opened.
+   ignore cookie Max-Age headers for non-installed sites, and erase cookies
    when the associated URL is no longer in browser history.
+   restrict durable/persistent storage to installed apps.
+   limit the storage quota available to sites unless they are installed (and
    conversely, raise storage quota limits for installed sites).
+   restrict what Service Workers may do in the background unless they control
    a site that is installed.

# Case Study -- Native File System Access

We describe a high level case study based on the principles in this document
for granting a web site access to a) read any file in a certain directory; b)
write files to a directory.

## Granting access

+   The user must give direct consent via a file picker.
+   The browser should disclose that access to the chosen file or directory
    will be granted to the web site.
+   A non-installed site will trigger the picker each page load that the API
    to read or write to a directory is invoked.
+   An installed site will have its access to reading or writing persisted.

## Additional considerations for writing to a directory

+   Existing Downloads UI and protections (e.g. malicious file scanning)
    could be employed to ensure sites cannot use writing to a directory as a
    bypass.
+   Where such scanning isn't accessible, confirmation prompts for opening
    dangerous files in the Downloads UI may be employed instead to ensure the
    user knows if a potentially malicious file type is being stored.
+   Non-installed sites may have additional restrictions on which folders
    are available to choose (e.g. only allowing the user's Downloads directory,
    or excluding the user's Documents directory).

# Alternatives Considered

## Guard API Access Behind Installation

Apps on any desktop or mobile platform require installation to run, and when
installed, apps are automatically granted many privileges. We could extend
this concept to the web by restricting powerful APIs like native file system
access only to installed web apps. That is, the drive-by web could not even
ask for permission to access an API -- the site would need to be installed.

A key argument for using installation in this manner is that some APIs are
simply so powerful that the drive-by web should not be able to ask for them.
However, this document takes the position that installation alone as a
restriction is undesirable.

### Pros:

+   This is a simple model that is both user controllable and
    developer-accessible (via installation APIs such as
    [beforeinstallpromptevent](https://developers.google.com/web/fundamentals/app-install-banners/)).
+   Installation is already synonymous with some amount of elevated privilege
    on most platforms.

### Cons:

+   Fragments the web platform into installed and not installed, with
    different APIs available depending on installed state.
+   Disempowers the drive-by web, and undermines the "try-before-you-buy"
    ability that the web affords today. Users may also not be willing to
    install a site from which they cannot ascertain any benefit.
+   Forces users to install a site to use it, even if they don't want to
    install.
+   May encourage web sites to prompt users on every visit to install the PWA
    to get access to powerful features.
+   Creates confusing scenarios when web apps are running in tabs:
    +   If a web app is installed but the user opens the site in a
        tab, does it get access to the capability or not?
    +   What about if the web app starts off in a standalone window, but
        the user reparents it into a tab? Or if a user sets an installed app to
        open in a tab?
+   Platforms where installation grants privilege all incur additional
    friction during installation that the web currently does not exhibit.
    Examples include:
    +   requiring something to be downloaded,
    +   requiring a confirmation prompt, installer, or some explicit
        privilege or grant during installation,
    +   requiring an explicit display of permissions that are implicitly
        granted.
+   The implicit granting of privilege by installation has proven to be a
    security and privacy challenge on many platforms, e.g. Android native apps
    can access many
    [powerful features](https://developer.android.com/guide/topics/permissions/overview#normal-dangerous)
    with no permission prompt and without user recourse to revoke access.
+   The amount of friction generated by PWA installation over the drive-by
    web is unclear, and gating APIs behind installation increases the
    incentive for tricking users into installing.

### Security considerations:

Restricting APIs to installed web apps is not a meaningful security improvement
for users for several reasons:

+   While it eliminates the drive-by web as an attack surface, per-API
    security mitigations (e.g. restricting which directories are accessible
    for reading and writing) would still be necessary to protect users of
    installed web apps.
+   The effectiveness of installation as a gate on access to powerful APIs
    weakens as the installed web app model becomes more successful.
+   Developers are incentivised to ask for installation to utilise powerful
    capabilities, contributing to the erosion of installation as an effective
    security mitigation.
+   Per-API permission requests would still be necessary in the installed
    state, making installation effectively equivalent to an implicit permission
    grant to ask for permission to access powerful features. We should simply
    ask users directly if they wish to grant permission, rather than use such a
    two-tiered requirement.

## Guard API Access Behind Engagement

This is a more general concept than installation: that continual, significant
usage of a web site should allow that site to access more powerful APIs.

### Pros:

+   Continual usage of a web site can be taken as a signal of trust

### Cons:

+   Engagement is not standardised or exposed to the web platform, making
    it a highly unpredictable and unergonomic mechanism of controlling access.
    +   Standardisation and a web-exposed API would both be requirements for
        using engagement in this way. Chromium's current engagement
        implementation is local-only and not web-exposed. There are serious
        privacy questions about exposing such data to the web.
+   Solving the first-run problem is non-trivial: engagement requires usage
    to accumulate, but there are apps which legitimately require access to a
    powerful API immediately to function (e.g. an editor -> files, or a
    weather site -> location).

### Security considerations:

+   There is no real evidence to support the implicit assumption under
    this model that continual usage of a web site correlates with user consent
    to access powerful features.
    +   Consider users who frequently use some web site for which location
        data is useful, but denies that site persistent, background access to
        geolocation permission.
    +   A proportion of web traffic goes to sites which users may interact
        with frequently, but may not necessarily want to grant powerful
        capabilities.

Similar to installation, the Chromium team does not regard engagement as a
robust way of controlling access to APIs.
