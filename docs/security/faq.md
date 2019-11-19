# Chrome Security FAQ

[TOC]

<a name="TOC-Why-are-security-bugs-hidden-in-the-Chromium-issue-tracker-"></a>
## Why are security bugs hidden in the Chromium issue tracker?

We must balance a commitment to openness with a commitment to avoiding
unnecessary risk for users of widely-used open source libraries.

<a name="TOC-Can-you-please-un-hide-old-security-bugs-"></a>
## Can you please un-hide old security bugs?

Our goal is to open security bugs to the public once the bug is fixed and the
fix has been shipped to a majority of users. However, many vulnerabilities
affect products besides Chromium, and we don’t want to put users of those
products unnecessarily at risk by opening the bug before fixes for the other
affected products have shipped.

Therefore, we make all security bugs public within approximately 14 weeks of the
fix landing in the Chromium repository. The exception to this is in the event of
the bug reporter or some other responsible party explicitly requesting anonymity
or protection against disclosing other particularly sensitive data included in
the vulnerability report (e.g. username and password pairs).

<a name="TOC-Can-I-get-advance-notice-about-security-bugs-"></a>
## Can I get advance notice about security bugs?

Vendors of products based on Chromium, distributors of operating systems that
bundle Chromium, and individuals and organizations that significantly contribute
to fixing security bugs can be added to a list for earlier access to these bugs.
You can email us at security@chromium.org to request to join the list if you
meet the above criteria. In particular, vendors of anti-malware, IDS/IPS,
vulnerability risk assessment, and similar products or services do not meet this
bar.

Please note that the safest version of Chrome/Chromium is always the latest
stable version — there is no good reason to wait to upgrade, so enterprise
deployments should always track the latest stable release. When you do this,
there is no need to further assess the risk of Chromium vulnerabilities: we
strive to fix vulnerabilities quickly and release often.

<a name="TOC-Can-I-see-these-security-bugs-so-that-I-can-back-port-the-fixes-to-my-downstream-project-"></a>
## Can I see these security bugs so that I can back-port the fixes to my downstream project?

Many developers of other projects use V8, Chromium, and sub-components of
Chromium in their own projects. This is great! We are glad that Chromium and V8
suit your needs.

We want to open up fixed security bugs (as described in the previous answer),
and will generally give downstream developers access sooner. **However, please
be aware that backporting security patches from recent versions to old versions
cannot always work.** (There are several reasons for this: The patch won't apply
to old versions; the solution was to add or remove a feature or change an API;
the issue may seem minor until it's too late; and so on.) We believe the latest
stable versions of Chromium and V8 are the most stable and secure. We also
believe that tracking the latest stable upstream is usually less work for
greater benefit in the long run than backporting. We strongly recommend that you
track the latest stable branches, and we support only the latest stable branch.

<a name="TOC-Severity-Guidelines"></a>
## How does the Chrome team determine severity of security bugs?

See the [severity guidelines](severity-guidelines.md) for more information.
Only security issues are considered under the security vulnerability rewards
program. Other types of bugs, which we call "functional bugs", are not.

<a name="TOC-Are-privacy-issues-considered-security-bugs-"></a>
## Are privacy issues considered security bugs?

No. The Chrome Privacy team treats privacy issues, such as leaking information
from Incognito, fingerprinting, and bugs related to deleting browsing data as
functional bugs.

Privacy issues are not considered under the security vulnerability rewards
program; the [severity guidelines](severity-guidelines.md) outline the types of
bugs that are considered security vulnerabilities in more detail.

<a name="TOC-Timing-Attacks"></a>
## Are timing attacks considered security vulnerabilities?

Some timing attacks are considered security vulnerabilities, and some are
considered privacy vulnerabilities. Timing attacks vary significantly in terms
of impact, reliability, and exploitability.

Some timing attacks weaken mitigations like ASLR (e.g.
[Issue 665930](https://crbug.com/665930)). Others attempt to circumvent the same
origin policy, for instance, by using SVG filters to read pixels
cross-origin (e.g. [Issue 686253](https://crbug.com/686253) and
[Issue 615851](https://crbug.com/615851)).

Many timing attacks rely upon the availability of high-resolution timing
information [Issue 508166](https://crbug.com/508166); such timing data often has
legitimate usefulness in non-attack scenarios making it unappealing to remove.

Timing attacks against the browser's HTTP Cache (like
[Issue 74987](https://crbug.com/74987)) can potentially leak information about
which sites the user has previously loaded. The browser could attempt to protect
against such attacks (e.g. by bypassing the cache) at the cost of performance
and thus user-experience. To mitigate against such timing attacks, end-users can
delete browsing history and/or browse sensitive sites using Chrome's Incognito
or Guest browsing modes.

Other timing attacks can be mitigated via clever design changes. For instance,
[Issue 544765](https://crbug.com/544765) describes an attack whereby an attacker
can probe for the presence of HSTS rules (set by prior site visits) by timing
the load of resources with URLs "fixed-up" by HSTS. Prior to Chrome 64, HSTS
rules [were shared](https://crbug.com/774643) between regular browsing and
Incognito mode, making the attack more interesting. The attack was mitigated by
changing Content-Security-Policy such that secure URLs will match rules
demanding non-secure HTTP urls, a fix that has also proven useful to help to
unblock migrations to HTTPS. Similarly, [Issue 707071](https://crbug.com/707071)
describes a timing attack in which an attacker could determine what Android
applications are installed; the attack was mitigated by introducing randomness
in the execution time of the affected API.

<a name="TOC-What-are-the-security-and-privacy-guarantees-of-Incognito-mode-"></a>
## What are the security and privacy guarantees of Incognito mode?

Bugs in Incognito mode are tracked as privacy bugs, not security bugs.

The [Help Center](https://support.google.com/chrome/?p=cpn_incognito) explains
what privacy protections Incognito mode attempts to enforce. In particular,
please note that Incognito is not a “do not track” mode, and it does not hide
aspects of your identity from web sites. Chrome does offer a way to send Do Not
Track request to servers; see chrome://settings/?search=do+not+track

When in Incognito mode, Chrome does not store any new history, cookies, or other
state in non-volatile storage. However, Incognito windows will be able to access
some previously-stored state, such as browsing history.

<a name="TOC-Are-denial-of-service-issues-considered-security-bugs-"></a>
## Are denial of service issues considered security bugs?

No. Denial of Service (DoS) issues are treated as **abuse** or **stability**
issues rather than security vulnerabilities.

*    If you find a reproducible crash, we encourage you to [report
     it](https://bugs.chromium.org/p/chromium/issues/entry?template=Crash%20Report).
*    If you find a site that is abusing the user experience (e.g. preventing you
     from leaving a site), we encourage you to [report
     it](https://crbug.com/new).

DoS issues are not considered under the security vulnerability rewards program;
the [severity guidelines](severity-guidelines.md) outline the types of bugs that
are considered security vulnerabilities in more detail.

<a name="TOC-Are-XSS-filter-bypasses-considered-security-bugs-"></a>
## Are XSS filter bypasses considered security bugs?

No. Chromium once contained a reflected XSS filter called the [XSSAuditor](https://www.chromium.org/developers/design-documents/xss-auditor)
that was a best-effort second line of defense against reflected XSS flaws found
in web sites. The XSS Auditor was [removed in Chrome 78](https://groups.google.com/a/chromium.org/forum/#!msg/blink-dev/TuYw-EZhO9g/blGViehIAwAJ).

<a name="TOC-Why-aren-t-physically-local-attacks-in-Chrome-s-threat-model-"></a>
## Why aren't physically-local attacks in Chrome's threat model?

People sometimes report that they can compromise Chrome by installing a
malicious DLL in a place where Chrome will load it, by hooking APIs (e.g. [Issue
130284](https://crbug.com/130284)), or by otherwise altering the configuration
of the PC.

We consider these attacks outside Chrome's threat model, because there is no way
for Chrome (or any application) to defend against a malicious user who has
managed to log into your computer as you, or who can run software with the
privileges of your operating system user account. Such an attacker can modify
executables and DLLs, change environment variables like `PATH`, change
configuration files, read any data your user account owns, email it to
themselves, and so on. Such an attacker has total control over your computer,
and nothing Chrome can do would provide a serious guarantee of defense. This
problem is not special to Chrome ­— all applications must trust the
physically-local user.

There are a few things you can do to mitigate risks from people who have
physical control over **your** computer, in certain circumstances.

*    To stop people from reading your data in cases of device theft or loss, use
     full disk encryption (FDE). FDE is a standard feature of most operating
     systems, including Windows Vista and later, Mac OS X Lion and later, and
     some distributions of Linux. (Some older versions of Mac OS X had partial
     disk encryption: they could encrypt the user’s home folder, which contains
     the bulk of a user’s sensitive data.) Some FDE systems allow you to use
     multiple sources of key material, such as the combination of both a
     password and a key file on a USB token. When available, you should use
     multiple sources of key material to achieve the strongest defense. Chrome
     OS encrypts users’ home directories.
*    If you share your computer with other people, take advantage of your
     operating system’s ability to manage multiple login accounts, and use a
     distinct account for each person. For guests, Chrome OS has a built-in
     Guest account for this purpose.
*    Take advantage of your operating system’s screen lock feature.
*    You can reduce the amount of information (including credentials like
     cookies and passwords) that Chrome will store locally by using Chrome's
     Content Settings (chrome://settings/content) and turning off the form
     auto-fill and password storage features
     ([chrome://settings/search#password](chrome://settings/search#password)).

There is almost nothing you can do to mitigate risks when using a **public**
computer.

*    Assume everything you do on a public computer will become, well, public.
     You have no control over the operating system or other software on the
     machine, and there is no reason to trust the integrity of it.
*    If you must use such a computer, use Incognito mode and close all Incognito
     windows when you are done browsing to limit the amount of data you leave
     behind. Note that Incognito mode **provides no protection** if the system has
     already been compromised as described above.

<a name="TOC-Why-aren-t-compromised-infected-machines-in-Chrome-s-threat-model-"></a>
## Why aren't compromised/infected machines in Chrome's threat model?

This is essentially the same situation as with physically-local attacks. The
attacker's code, when it runs as your user account on your machine, can do
anything you can do. (See also [Microsoft's Ten Immutable Laws Of
Security](https://web.archive.org/web/20160311224620/https://technet.microsoft.com/en-us/library/hh278941.aspx).)

<a name="TOC-What-about-unmasking-of-passwords-with-the-developer-tools-"></a>
## What about unmasking of passwords with the developer tools?

One of the most frequent reports we receive is password disclosure using the
Inspect Element feature (see [Issue 126398](https://crbug.com/126398) for an
example). People reason that "If I can see the password, it must be a bug."
However, this is just one of the [physically-local attacks described in the
previous
section](#TOC-Why-aren-t-physically-local-attacks-in-Chrome-s-threat-model-),
and all of those points apply here as well.

The reason the password is masked is only to prevent disclosure via
"shoulder-surfing" (i.e. the passive viewing of your screen by nearby persons),
not because it is a secret unknown to the browser. The browser knows the
password at many layers, including JavaScript, developer tools, process memory,
and so on. When you are physically local to the computer, and only when you are
physically local to the computer, there are, and always will be, tools for
extracting the password from any of these places.

<a name="TOC-Does-entering-JavaScript:-URLs-in-the-URL-bar-or-running-script-in-the-developer-tools-mean-there-s-an-XSS-vulnerability-"></a>
## Does entering JavaScript: URLs in the URL bar or running script in the developer tools mean there's an XSS vulnerability?

[No](https://crbug.com/81697). Chrome does not attempt to prevent the user from
knowingly running script against loaded documents, either by entering script in
the Developer Tools console or by typing a JavaScript: URI into the URL bar.
Chrome and other browsers do undertake some efforts to prevent *paste* of script
URLs in the URL bar (to limit
[social-engineering](https://blogs.msdn.microsoft.com/ieinternals/2011/05/19/socially-engineered-xss-attacks/))
but users are otherwise free to invoke script against pages using either the URL
bar or the DevTools console.

Similarly, users may create bookmarks pointed at JavaScript URLs that will run
on the currently-loaded page when the user clicks the bookmark; these are called
[bookmarklets](https://en.wikipedia.org/wiki/Bookmarklet).

<a name="TOC-Is-Chrome-s-support-for-userinfo-in-HTTP-URLs-e.g.-http:-user:password-example.com-considered-a-vulnerability-"></a>
## Is Chrome's support for userinfo in HTTP URLs (e.g. http://user:password@example.com) considered a vulnerability?

[Not at this time](https://crbug.com/626951). Chrome supports HTTP and HTTPS
URIs with username and password information embedded within them for
compatibility with sites that require this feature. Notably, Chrome will
suppress display of the username and password information after navigation in
the URL box to limit the effectiveness of spoofing attacks that may try to
mislead the user. For instance, navigating to
`http://trustedsite.com@evil.example.com` will show an address of
`http://evil.example.com` after the page loads.

<a name="TOC-Why-isn-t-passive-browser-fingerprinting-including-passive-cookies-in-Chrome-s-threat-model-"></a>
<a name="TOC-What-is-Chrome-s-threat-model-for-fingerprinting-"></a>
## What is Chrome's threat model for fingerprinting?

> **Update, August 2019:** Please note that this answer has changed. We have
> updated our threat model to include fingerprinting.

Although [we do not consider fingerprinting issues to be *security
vulnerabilities*](#TOC-Are-privacy-issues-considered-security-bugs-), we do now
consider them to be privacy bugs that we will try to resolve. We distinguish two
forms of fingerprinting.

* **Passive fingerprinting** refers to fingerprinting techniques that do not
require a JavaScript API call to achieve. This includes (but is not limited to)
mechanisms like [ETag
cookies](https://en.wikipedia.org/wiki/HTTP_ETag#Tracking_using_ETags) and [HSTS
cookies](https://security.stackexchange.com/questions/79518/what-are-hsts-super-cookies).
* **Active fingerprinting** refers to fingerprinting techniques that do require
a JavaScript API call to achieve. Examples include most of the techniques in
[EFF's Panopticlick proof of concept](https://panopticlick.eff.org).

For passive fingerprinting, our ultimate goal is (to the extent possible) to
reduce the information content available to below the threshold for usefulness.

For active fingerprinting, our ultimate goal is to establish a [privacy
budget](https://github.com/bslassey/privacy-budget) and to keep web origins
below the budget (such as by rejecting some API calls when the origin exceeds
its budget). To avoid breaking rich web applications that people want to use,
Chrome may increase an origin's budget when it detects that a person is using
the origin heavily. As with passive fingerprinting, our goal is to set the
default budget below the threshold of usefulness for fingerprinting.

These are both long-term goals. As of this writing (August 2019) we do not
expect that Chrome will immediately achieve them.

For background on fingerprinting and the difficulty of stopping it, see [Arvind
Narayanan's site](https://33bits.wordpress.com/about/) and [Peter Eckersley's
discussion of the information theory behind
Panopticlick](https://www.eff.org/deeplinks/2010/01/primer-information-theory-and-privacy).
There is also [a pretty good analysis of in-browser fingerprinting
vectors](https://dev.chromium.org/Home/chromium-security/client-identification-mechanisms).

<a name="TOC-Where-are-the-security-indicators-located-in-the-browser-window-"></a>
## Where are the security indicators located in the browser window?

The topmost portion of the browser window, consisting of the **Omnibox** (or
**Location Bar**), navigation icons, menu icon, and other indicator icons, is
sometimes called the browser **chrome** (not to be confused with the Chrome
Browser itself). Actual security indicators can only appear in this section of
the window. There can be no trustworthy security indicators elsewhere.

Furthermore, Chrome can only guarantee that it is correctly representing URLs
and their origins at the end of all navigation. Quirks of URL parsing, HTTP
redirection, and so on are not security concerns unless Chrome is
misrepresenting a URL or origin after navigation has completed.

Browsers present a dilemma to the user since the output is a combination of
information coming from both trustworthy sources (the browser itself) and
untrustworthy sources (the web page), and the untrustworthy sources are allowed
virtually unlimited control over graphical presentation. The only restriction on
the page's presentation is that it is confined to the large rectangular area
directly underneath the chrome, called the **viewport**. Things like hover text
and URL preview(s), shown in the viewport, are entirely under the control of the
web page itself. They have no guaranteed meaning, and function only as the page
desires. This can be even more confusing when pages load content that looks like
chrome. For example, many pages load images of locks, which look similar to the
meaningful HTTPS lock in the Omnibox, but in fact do not convey any meaningful
information about the transport security of that page.

When the browser needs to show trustworthy information, such as the bubble
resulting from a click on the lock icon, it does so by making the bubble overlap
chrome. In the case of the lock bubble, it is a small triangular bump in the
border of the bubble that overlays the chrome. This visual detail can't be
imitated by the page itself since the page is confined to the viewport.

<a name="TOC-Why-does-Chrome-show-a-green-lock-even-if-my-HTTPS-connection-is-being-proxied-"></a>
## Why does Chrome show a green lock, even if my HTTPS connection is being proxied?

Some types of software intercept HTTPS connections. Examples include anti-virus
software, corporate network monitoring tools, and school censorship software. In
order for the interception to work, you need to install a private trust anchor
(root certificate) onto your computer. This may have happened when you installed
your anti-virus software, or when your company's network administrator set up
your computer. If that has occurred, your HTTPS connections can be viewed or
modified by the software.

Since you have allowed the trust anchor to be installed onto your computer,
Chrome assumes that you have consented to HTTPS interception. Anyone who can add
a trust anchor to your computer can make other changes to your computer, too,
including changing Chrome. (See also [Why aren't physically-local attacks in
Chrome's threat model?](#TOC-Why-aren-t-physically-local-attacks-in-Chrome-s-threat-model-).)

<a name="TOC-Why-can-t-I-select-Proceed-Anyway-on-some-HTTPS-error-screens-"></a>
## Why can’t I select Proceed Anyway on some HTTPS error screens?

A key guarantee of HTTPS is that Chrome can be relatively certain that it is
connecting to the true web server and not an impostor. Some sites request an
even higher degree of protection for their users (i.e. you): they assert to
Chrome (via Strict Transport Security —
[HSTS](https://tools.ietf.org/html/rfc6797) — or by other means) that any
server authentication error should be fatal, and that Chrome must close the
connection. If you encounter such a fatal error, it is likely that your network
is under attack, or that there is a network misconfiguration that is
indistinguishable from an attack.

The best thing you can do in this situation is to raise the issue to your
network provider (or corporate IT department).

Chrome shows non-recoverable HTTPS errors only in cases where the true server
has previously asked for this treatment, and when it can be relatively certain
that the current server is not the true server.

<a name="TOC-How-does-key-pinning-interact-with-local-proxies-and-filters-"></a>
## How does key pinning interact with local proxies and filters?

To enable certificate chain validation, Chrome has access to two stores of trust
anchors (i.e. certificates that are empowered as issuers). One trust anchor
store is the system or public trust anchor store, and the other other is the
local or private trust anchor store. The public store is provided as part of
the operating system, and intended to authenticate public internet servers. The
private store contains certificates installed by the user or the administrator
of the client machine. Private intranet servers should authenticate themselves
with certificates issued by a private trust anchor.

Chrome’s key pinning feature is a strong form of web site authentication that
requires a web server’s certificate chain not only to be valid and to chain to a
known-good trust anchor, but also that at least one of the public keys in the
certificate chain is known to be valid for the particular site the user is
visiting. This is a good defense against the risk that any trust anchor can
authenticate any web site, even if not intended by the site owner: if an
otherwise-valid chain does not include a known pinned key (“pin”), Chrome will
reject it because it was not issued in accordance with the site operator’s
expectations.

Chrome does not perform pin validation when the certificate chain chains up to a
private trust anchor. A key result of this policy is that private trust anchors
can be used to proxy (or
[MITM](https://en.wikipedia.org/wiki/Man-in-the-middle_attack)) connections, even
to pinned sites. “Data loss prevention” appliances, firewalls, content filters,
and malware can use this feature to defeat the protections of key pinning.

We deem this acceptable because the proxy or MITM can only be effective if the
client machine has already been configured to trust the proxy’s issuing
certificate — that is, the client is already under the control of the person who
controls the proxy (e.g. the enterprise’s IT administrator). If the client does
not trust the private trust anchor, the proxy’s attempt to mediate the
connection will fail as it should.

<a name="TOC-When-is-key-pinning-enabled-"></a>
## When is key pinning enabled?

Key pinning is enabled for Chrome-branded, non-mobile builds when the local
clock is within ten weeks of the embedded build timestamp. Key pinning is a
useful security measure but it tightly couples client and server configurations
and completely breaks when those configurations are out of sync. In order to
manage that risk we need to ensure that we can promptly update pinning clients
in an emergency and ensure that non-emergency changes can be deployed in a
reasonable timeframe.

Each of the conditions listed above helps ensure those properties:
Chrome-branded builds are those that Google provides and they all have an
auto-update mechanism that can be used in an emergency. However, auto-update on
mobile devices is significantly less effective thus they are excluded. Even in
cases where auto-update is generally effective, there are still non-trivial
populations of stragglers for various reasons. The ten-week timeout prevents
those stragglers from causing problems for regular, non-emergency changes and
allows stuck users to still, for example, conduct searches and access Chrome's
homepage to hopefully get unstuck.

In order to determine whether key pinning is active, try loading
[https://pinningtest.appspot.com](https://pinningtest.appspot.com). If key
pinning is active the load will _fail_ with a pinning error.

<a name="TOC-How-does-certificate-transparency-interact-with-local-proxies-and-filters-"></a>
## How does Certificate Transparency interact with local proxies and filters?

Just as [pinning only applies to publicly-trusted trust
anchors](#TOC-How-does-key-pinning-interact-with-local-proxies-and-filters-),
Chrome only evaluates Certificate Transparency (CT) for publicly-trusted trust
anchors. Thus private trust anchors, such as for enterprise middle-boxes and AV
proxies, do not need to be publicly logged in a CT log.

<a name="TOC-Can-I-use-EMET-to-help-protect-Chrome-against-attack-on-Microsoft-Windows-"></a>
## Can I use EMET to help protect Chrome against attack on Microsoft Windows?

There are [known compatibility
problems](https://sites.google.com/a/chromium.org/dev/Home/chromium-security/chromium-and-emet)
between Microsoft's EMET anti-exploit toolkit and some versions of Chrome. These
can prevent Chrome from running in some configurations. Moreover, the Chrome
security team does not recommend the use of EMET with Chrome because its most
important security benefits are redundant with or superseded by built-in attack
mitigations within the browser. For users, the very marginal security benefit is
not usually a good trade-off for the compatibility issues and performance
degradation the toolkit can cause.

<a name="TOC-Why-are-some-web-platform-features-only-available-in-HTTPS-page-loads-"></a>
## Why are some web platform features only available in HTTPS page-loads?

The full answer is here: we [Prefer Secure Origins For Powerful New
Features](https://www.chromium.org/Home/chromium-security/prefer-secure-origins-for-powerful-new-features).
In short, many web platform features give web origins access to sensitive new
sources of information, or significant power over a user's experience with their
computer/phone/watch/etc., or over their experience with it. We would therefore
like to have some basis to believe the origin meets a minimum bar for security,
that the sensitive information is transported over the Internet in an
authenticated and confidential way, and that users can make meaningful choices
to trust or not trust a web origin.

Note that the reason we require secure origins for WebCrypto is slightly
different: An application that uses WebCrypto is almost certainly using it to
provide some kind of security guarantee (e.g. encrypted instant messages or
email). However, unless the JavaScript was itself transported to the client
securely, it cannot actually provide any guarantee. (After all, a MITM attacker
could have modified the code, if it was not transported securely.)

<a name="TOC-Which-origins-are-secure-"></a>
## Which origins are "secure"?

Secure origins are those that match at least one of the following (scheme, host,
port) patterns:

*    (https, *, *)
*    (wss, *, *)
*    (*, localhost, *)
*    (*, 127/8, *)
*    (*, ::1/128, *)
*    (file, *, —)
*    (chrome-extension, *, —)

That is, secure origins are those that load resources either from the local
machine (necessarily trusted) or over the network from a
cryptographically-authenticated server. See [Prefer Secure Origins For Powerful
New
Features](https://sites.google.com/a/chromium.org/dev/Home/chromium-security/prefer-secure-origins-for-powerful-new-features)
for more details.

<a name="TOC-What-s-the-story-with-certificate-revocation-"></a>
## What's the story with certificate revocation?

Chrome's primary mechanism for checking the revocation status of HTTPS
certificates is
[CRLsets](https://dev.chromium.org/Home/chromium-security/crlsets).

Chrome also supports Online Certificate Status Protocol (OCSP). However, the
effectiveness of OCSP is is essentially 0 unless the client fails hard (refuses
to connect) if it cannot get a live, valid OCSP response. No browser has OCSP
set to hard-fail by default, for good reasons explained by Adam Langley (see
[https://www.imperialviolet.org/2014/04/29/revocationagain.html](https://www.imperialviolet.org/2014/04/29/revocationagain.html) and
[https://www.imperialviolet.org/2014/04/19/revchecking.html](https://www.imperialviolet.org/2014/04/19/revchecking.html)).

Stapled OCSP with the Must Staple option (hard-fail if a valid OCSP response is
not stapled to the certificate) is a much better solution to the revocation
problem than non-stapled OCSP. CAs and browsers are working toward that solution
(see the
[Internet-Draft](https://tools.ietf.org/html/draft-hallambaker-tlssecuritypolicy-03)).

Additionally, non-stapled OCSP poses a privacy problem: in order to check the
status of a certificate, the client must query an OCSP responder for the status
of the certificate, thus exposing a user's HTTPS browsing history to the
responder (a third party).

That said, you can use enterprise policies to [enable soft-fail
OCSP](https://cloud.google.com/docs/chrome-enterprise/policies/?policy=EnableOnlineRevocationChecks)
and hard-fail OCSP for [local trust
anchors](https://cloud.google.com/docs/chrome-enterprise/policies/?policy=RequireOnlineRevocationChecksForLocalAnchors).

Chrome performs online checking for [Extended
Validation](https://cabforum.org/about-ev-ssl/) certificates if it does not
already have a non-expired CRLSet entry covering the domain. If Chrome does not
get a response, it simply downgrades the security indicator to Domain Validated.

See also [Issue 361820](https://crbug.com/361820) for more discussion of the
user-facing UX.

<a name="TOC-Why-does-the-Password-Manager-ignore-autocomplete-off-for-password-fields-"></a>
## Why does the Password Manager ignore `autocomplete='off'` for password fields?

Ignoring `autocomplete='off'` for password fields allows the password manager to
give more power to users to manage their credentials on websites. It is the
security team's view that this is very important for user security by allowing
users to have unique and more complex passwords for websites. As it was
originally implemented, autocomplete='off' for password fields took control away
from the user and gave control to the web site developer, which was also a
violation of the [priority of
constituencies](http://www.schemehostport.com/2011/10/priority-of-constituencies.html).
For a longer discussion on this, see the [mailing list
announcement](https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/zhhj7hCip5c).

<a name="TOC-Signout-of-Chrome"></a>
## Signing out of Chrome does not delete previously-synced data?

If you have signed into Chrome and subsequently sign out of Chrome, previously
saved passwords and other data are not deleted from your device unless you
select that option when signing out of Chrome.

If you change your Google password, synced data will no longer be updated in
Chrome instances until you provide the new password to Chrome on each device
configured to sync. However, previously synced data [remains available](https://crbug.com/792967)
on each previously-syncing device unless manually removed.

<a name="TOC-Why-doesn-t-the-Password-Manager-save-my-Google-password-if-I-am-using-Chrome-Sync-"></a>
## Why doesn't the Password Manager save my Google password if I am using Chrome Sync?

In its default mode, Chrome Sync uses your Google password to protect all the
other passwords in the Chrome Password Manager.

In general, it is a bad idea to store the credential that protects an asset in
the same place as the asset itself. An attacker who could temporarily compromise
the Chrome Password Manager could, by stealing your Google password, obtain
continuing access to all your passwords. Imagine you store your valuables in a
safe, and you accidentally forget to close the safe. If a thief comes along,
they might steal all of your valuables. That’s bad, but imagine if you had also
left the combination to the safe inside as well. Now the bad guy has access to
all of your valuables and all of your future valuables, too. The password
manager is similar, except you probably would not even know if a bad guy
accessed it.

To prevent this type of attack, Chrome Password Manager does not save the Google
password for the account you sync with Chrome. If you have multiple Google
accounts, the Chrome Password Manager will save the passwords for accounts other
than the one you are syncing with.

<a name="TOC-Does-the-Password-Manager-store-my-passwords-encrypted-on-disk-"></a>
## Does the Password Manager store my passwords encrypted on disk?

Chrome generally tries to use the operating system's user storage mechanism
wherever possible and stores them encrypted on disk, but it is platform
specific:

*    On Windows, Chrome uses the [Data Protection API
     (DPAPI)](https://msdn.microsoft.com/en-us/library/ms995355.aspx) to bind
     your passwords to your user account and store them on disk encrypted with
     a key only accessible to processes running as the same logged on user.
*    On macOS, Chrome previously stored credentials directly in the user's
     Keychain, but for technical reasons, it has switched to storing the
     credentials in "Login Data" in the Chrome users profile directory, but
     encrypted on disk with a key that is then stored in the user's Keychain.
     See [Issue 466638](https://crbug.com/466638) for further explanation.
*    On Linux, Chrome previously stored credentials directly in the user's
     Gnome Keyring or KWallet, but for technical reasons, it has switched to
     storing the credentials in "Login Data" in the Chrome user's profile directory,
     but encrypted on disk with a key that is then stored in the user's Gnome
     Keyring or KWallet. If there is no available Keyring or KWallet, the data is
     not encrypted when stored.
*    On iOS, passwords are currently stored directly in the iOS Keychain and
     referenced from the rest of the metadata stored in a separate DB. The plan
     there is to just store them in plain text in the DB, because iOS gives
     strong guarantees about only Chrome being able to access its storage. See
     [Issue 520437](https://crbug.com/520437) to follow this migration.

<a name="TOC-I-found-a-phishing-or-malware-site-not-blocked-by-Safe-Browsing.-Is-this-a-security-vulnerability-"></a>
## I found a phishing or malware site not blocked by Safe Browsing. Is this a security vulnerability?

Malicious sites not yet blocked by Safe Browsing can be reported via
[https://www.google.com/safebrowsing/report_phish/](https://www.google.com/safebrowsing/report_phish/).
Safe Browsing is primarily a blocklist of known-unsafe sites; the feature warns
the user if they attempt to navigate to a site known to deliver phishing or
malware content. You can learn more about this feature in these references:

*    [https://developers.google.com/safe-browsing/](https://developers.google.com/safe-browsing/)
*    [https://www.google.com/transparencyreport/safebrowsing/](https://www.google.com/transparencyreport/safebrowsing/)

In general, it is not considered a security bug if a given malicious site is not
blocked by the Safe Browsing feature, unless the site is on the blocklist but is
allowed to load anyway. For instance, if a site found a way to navigate through
the blocking red warning page without user interaction, that would be a security
bug. A malicious site may exploit a security vulnerability (for instance,
spoofing the URL in the **Location Bar**). This would be tracked as a security
vulnerability in the relevant feature, not Safe Browsing itself.

<a name="TOC-What-is-the-security-story-for-Service-Workers-"></a>
## What is the security story for Service Workers?

See our dedicated [Service Worker Security
FAQ](https://chromium.googlesource.com/chromium/src/+/master/docs/security/service-worker-security-faq.md).

<a name="TOC-What-about-URL-spoofs-using-Internationalized-Domain-Names-IDN-"></a>
## What about URL spoofs using Internationalized Domain Names (IDN)?

We try to balance the needs of our international userbase while protecting users
against confusable homograph attacks. Despite this, there are a list of known
IDN display issues we are still working on.

*    Please see [this document](https://docs.google.com/document/d/1_xJz3J9kkAPwk3pma6K3X12SyPTyyaJDSCxTfF8Y5sU)
for a list of known issues and how we handle them.
*    [This document](https://www.chromium.org/developers/design-documents/idn-in-google-chrome)
describes Chrome's IDN policy in detail.

## TODO

*    https://dev.chromium.org/Home/chromium-security/client-identification-mechanisms
