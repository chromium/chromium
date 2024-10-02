# Chrome Security FAQ

[TOC]

## Process

<a name="TOC-Which-bugs-are-valid-for-rewards-under-the-Chrome-Vulnerability-Rewards-program-"></a>
### Which bugs are valid for rewards under the Chrome Vulnerability Rewards program?

Please see [the VRP FAQ page](vrp-faq.md).

<a name="TOC-Why-are-security-bugs-hidden-in-the-Chromium-issue-tracker-"></a>
### Why are security bugs hidden in the Chromium issue tracker?

We must balance a commitment to openness with a commitment to avoiding
unnecessary risk for users of widely-used open source libraries.

<a name="TOC-Can-you-please-un-hide-old-security-bugs-"></a>
### Can you please un-hide old security bugs?

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
### Can I get advance notice about security bugs?

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

<a name="TOC-How-can-I-know-which-fixes-to-include-in-my-downstream-project-"></a>
### How can I know which fixes to include in my downstream project?

Chrome is built with mitigations and hardening which aim to prevent or reduce
the impact of security issues. We classify bugs as security issues if they are
known to affect a version and configuration of Chrome that we ship to the
public. Some classes of bug might present as security issues if Chrome was
compiled with different flags, or linked against a different C++ standard
library, but do not with the toolchain and configuration that we use to build
Chrome. We discuss some of these cases elsewhere in this FAQ.

If we become aware of them, these issues may be triaged as `Type=Vulnerability,
Security_Impact-None` or as `Type=Bug` because they do not affect the production
version of Chrome. They may or may not be immediately visible to the public in
the bug tracker, and may or may not be identified as security issues. If fixes
are landed, they may or may not be merged from HEAD to a release branch. Chrome
will only label, fix and merge security issues in Chrome, but attackers can
still analyze public issues, or commits in the Chromium project to identify bugs
that might be exploitable in other contexts.

Chromium embedders and other downstream projects may build with different
compilers, compile options, target operating systems, standard library, or
additional software components. It is possible that some issues Chrome
classifies as functional issues will manifest as security issues in a product
embedding Chromium - it is the responsibility of any such project to understand
what code they are shipping, and how it is compiled. We recommend using Chrome's
[configuration](https://source.chromium.org/chromium/chromium/src/+/main:build/config/)
whenever possible.

<a name="TOC-Can-I-see-these-security-bugs-so-that-I-can-back-port-the-fixes-to-my-downstream-project-"></a>
### Can I see these security bugs so that I can back-port the fixes to my downstream project?

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
### How does the Chrome team determine severity of security bugs?

See the [severity guidelines](severity-guidelines.md) for more information.
Only security issues are considered under the security vulnerability rewards
program. Other types of bugs, which we call "functional bugs", are not.

## Threat Model

<a name="TOC-Timing-Attacks"></a>
### Are timing attacks considered security vulnerabilities?

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

<a name="TOC-What-if-a-Chrome-component-breaks-an-OS-security-boundary-"></a>
### What if a Chrome component breaks an OS security boundary?

If Chrome or any of its components (e.g. updater) can be abused to
perform a local privilege escalation, then it may be treated as a
valid security vulnerability.

Running any Chrome component with higher privileges than intended is
not a security bug and we do not recommend running Chrome as an
Administrator on Windows, or as root on POSIX.

<a name="TOC-Why-isn-t-passive-browser-fingerprinting-including-passive-cookies-in-Chrome-s-threat-model-"></a>
<a name="TOC-What-is-Chrome-s-threat-model-for-fingerprinting-"></a>
### What is Chrome's threat model for fingerprinting?

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

<a name="TOC-I-found-a-phishing-or-malware-site-not-blocked-by-Safe-Browsing.-Is-this-a-security-vulnerability-"></a>
### I found a phishing or malware site not blocked by Safe Browsing. Is this a security vulnerability?

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

<a name="TOC-I-can-download-a-file-with-an-unsafe-extension-and-it-is-not-classified-as-dangerous-"></a>
### I can download a file with an unsafe extension and it is not classified as dangerous - is this a security bug?

Chrome tries to warn users before they open files that might modify their
system. What counts as a dangerous file will vary depending on the operating
system Chrome is running on, the default set of file handlers, Chrome settings,
Enterprise policy and verdicts on both the site and the file from [Safe
Browsing](https://code.google.com/apis/safebrowsing/). Because of this it will
often be okay for a user to download and run a file. However, if you can clearly
demonstrate how to bypass one of these protections then we’d like to hear about
it. You can see if a Safe Browsing check happened by opening
chrome://safe-browsing before starting the download.

<a name="TOC-what-about-dangerous-file-types-not-listed-in-the-file-type-policy-"></a>
### What about dangerous file types not listed in the file type policy?

The [file type
policy](https://source.chromium.org/chromium/chromium/src/+/main:components/safe_browsing/content/resources/download_file_types.asciipb?q=download_file_types.asciipb%20-f:%2Fgen%2F&ss=chromium)
controls some details of which security checks to enable for a given file
extension. Most importantly, it controls whether we contact Safe Browsing about
a download, and whether we show a warning for all downloads of that file type.
Starting in M74, the default for unknown file types has been to contact Safe
Browsing. This prevents large-scale abuse from a previously unknown file type.
Starting in M105, showing a warning for all downloads of an extension became
reserved for exceptionally dangerous file types that can compromise a user
without any user interaction with the file (e.g. DLL hijacking). If you discover
a new file type that meets that condition, we’d like to hear about it.

<a name="TOC-i-found-a-local-file-or-directory-that-may-be-security-sensitive-and-is-not-blocked-by-file-system-access-api-"></a>
### I found a local file or directory that may be security-sensitive and is not blocked by File System Access API - is this a security bug?

The File System Access API maintains a [blocklist](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/file_system_access/chrome_file_system_access_permission_context.cc;l=266-346)
of directories and files that may be sensitive such as systems file, and if user
chooses a file or a directory matching the list on a site using File System
Access API, the access is blocked.

The blocklist is designed to help mitigate accidental granting by users by
listing well-known, security-sensitive locations, as a defense in-depth
strategy. Therefore, the blocklist coverage is not deemed as a security bug,
especially as it requires user's explicit selection on a file or a directory
from the file picker.

<a name="TOC-I-can-download-a-file-with-an-unsafe-extension-but-a-different-extension-or-file-type-is-shown-to-the-user-"></a>
### I can download a file with an unsafe extension but a different extension or file type is shown to the user - is this a security bug?

See [file types](#TOC-The-wrong-description-for-a-file-type-is-added-by-Chrome-).

<a name="TOC-Extensions-for-downloaded-files-are-not-shown-in-a-file-dialog-"></a>
### Extensions for downloaded files are not shown in a file dialog - is this a security bug?

See [file types](#TOC-The-wrong-description-for-a-file-type-is-added-by-Chrome-).

<a name="TOC-The-wrong-description-for-a-file-type-is-added-by-Chrome-"></a>
### The wrong description for a file type is added by Chrome - is this a security bug?

Chrome tries to let users know what they will be saving and downloading before
they do so. Often operating systems will obscure a file’s type or extension and
there is little we can do about that. Chrome shows information to help users
make these decisions, both in Chrome-owned UI and in information that Chrome
passes to OS-owned UI. If this information can be manipulated from a web site to
mislead a user, then we’d like to hear about it.
[Example](https://crbug.com/1137247).

<a name="TOC-I-can-download-a-file-and-OS-indicators-for-its-provenance-are-not-applied-"></a>
### I can download a file and OS indicators for its provenance are not applied - is this a security bug?

Chrome attempts to label files downloaded from the internet with metadata using
operating system APIs where these are available – for instance applying the Mark
of the Web on Windows. This is often not possible (for instance on non-NTFS file
systems on Windows, or for files inside downloaded archives) or disabled by
policy. If a web site can cause Chrome to download a file without Chrome then
adding this metadata as usual, we’d like to hear about it.

<a name="TOC-I-can-cause-a-hard-or-soft-link-to-be-written-to-a-directory-bypassing-normal-OS-blocks-"></a>
### I can cause a hard or soft link to be written to a directory bypassing normal OS blocks - is this a security bug?

Chrome should not allow filesystem links to be created by initiating a download.
[Example](https://crbug.com/1140417). [Example](https://crbug.com/1137247#c12).

<a name="TOC-I-can-hijack-a-user-gesture-and-trick-a-user-into-accepting-a-permission-or-downloading-a-file-"></a>
### I can hijack a user gesture and trick a user into accepting a permission or downloading a file - is this a security bug?

Chrome tries to design its prompts to select safe defaults. If a prompt can
accidentally be accepted without the user having an opportunity to make a
decision about the prompt then we’d like to know. Examples might include poor
defaults so that a user holding down an enter key might accept a dialog they
would want to dismiss. [Example](https://crbug.com/854455#c11).

Note that a user navigating to a download will cause a file to be
[downloaded](https://crbug.com/1114592).

<a name="TOC-security-properties-not-inherited-using-contextual-menu-"></a>
### Sandbox/CSP/etc... security properties are not inherited when navigating using the middle-click/contextual-menu - is this a security bug?

The security properties of the document providing the URL are not used/inherited
when the user deliberately opens a link in a popup using one of:

- Ctrl + left-click (Open link in new tab)
- Shift + left-click (Open link in new window)
- Middle-click (Open a link in a new tab)
- Right-click > "Open link in ..."

These methods of following a link have more or less the same implications as the
user copying the link's URL and pasting it into a newly-opened window. We treat
them as user-initiated top-level navigations, and as such will not apply or
inherit policy restrictions into the new context

Example of security related properties:

- Content-Security-Policy
- Cross-Origin-Embedder-Policy
- Cross-Origin-Opener-Policy
- Origin
- Referrer
- Sandbox
- etc...

These browser's actions/shortcuts are specific to Chrome. They are different
from the behavior specified by the web-platform, such as using executing
`window.open()` or opening a link with the `target=_blank` attribute.

<a name="TOC-What-is-the-threat-model-for-Chrome-for-Testing"></a>
### What is the threat model for Chrome for Testing?

[Chrome for Testing](https://developer.chrome.com/blog/chrome-for-testing) is a
distribution of current and older versions of Chrome. It does not auto-update.
Therefore, it may lack recent fixes for security bugs. Security bugs can more
easily be exploited once their fixes are [published in the main Chromium source
code repository](updates.md) and so it is unsafe to use Chrome for Testing to
access any untrusted website.  You should use Chrome for Testing only for
browser automation and testing purposes, consuming only trustworthy content.
`chrome-headless-shell` also lacks auto-updates and so, for the same reason,
should only be used to consume trusted content.

## Areas outside Chrome's Threat Model

<a name="TOC-Are-privacy-issues-considered-security-bugs-"></a>
### Are privacy issues considered security bugs?

No. The Chrome Privacy team treats privacy issues, such as leaking information
from Incognito, fingerprinting, and bugs related to deleting browsing data as
functional bugs.

Privacy issues are not considered under the security vulnerability rewards
program; the [severity guidelines](severity-guidelines.md) outline the types of
bugs that are considered security vulnerabilities in more detail.

<a name="TOC-What-are-the-security-and-privacy-guarantees-of-Incognito-mode-"></a>
### What are the security and privacy guarantees of Incognito mode?

Bugs in Incognito mode are tracked as privacy bugs, not security bugs.

The [Help Center](https://support.google.com/chrome/?p=cpn_incognito) explains
what privacy protections Incognito mode attempts to enforce. In particular,
please note that Incognito is not a “do not track” mode, and it does not hide
aspects of your identity from web sites. Chrome does offer a way to send Do Not
Track request to servers; see chrome://settings/?search=do+not+track

When in Incognito mode, Chrome does not store any new history, cookies, or other
state in non-volatile storage. However, Incognito windows will be able to access
some previously-stored state, such as browsing history.

<a name="TOC-Are-XSS-filter-bypasses-considered-security-bugs-"></a>
### Are XSS filter bypasses considered security bugs?

No. Chromium once contained a reflected XSS filter called the [XSSAuditor](https://www.chromium.org/developers/design-documents/xss-auditor)
that was a best-effort second line of defense against reflected XSS flaws found
in web sites. The XSS Auditor was [removed in Chrome 78](https://groups.google.com/a/chromium.org/forum/#!msg/blink-dev/TuYw-EZhO9g/blGViehIAwAJ).
Consequently, Chromium no longer takes any special action in response to an
X-XSS-Protection header.

<a name="TOC-Are-denial-of-service-issues-considered-security-bugs-"></a>
### Are denial of service issues considered security bugs?

No. Denial of Service (DoS) issues are treated as **abuse** or **stability**
issues rather than security vulnerabilities.

*    If you find a reproducible crash (e.g. a way to hit a `CHECK`),
     we encourage you to [report it](https://issues.chromium.org/new).
*    If you find a site that is abusing the user experience (e.g. preventing you
     from leaving a site), we encourage you to [report
     it](https://issues.chromium.org/new).

DoS issues are not considered under the security vulnerability rewards program;
the [severity guidelines](severity-guidelines.md) outline the types of bugs that
are considered security vulnerabilities in more detail.

<a name="TOC-Why-aren-t-physically-local-attacks-in-Chrome-s-threat-model-"></a>
### Why aren't physically-local attacks in Chrome's threat model?

People sometimes report that they can compromise Chrome by installing a
malicious DLL in a place where Chrome will load it, by hooking APIs (e.g. [Issue
130284](https://crbug.com/130284)), or by otherwise altering the configuration
of the device.

We consider these attacks outside Chrome's threat model, because there is no way
for Chrome (or any application) to defend against a malicious user who has
managed to log into your device as you, or who can run software with the
privileges of your operating system user account. Such an attacker can modify
executables and DLLs, change environment variables like `PATH`, change
configuration files, read any data your user account owns, email it to
themselves, and so on. Such an attacker has total control over your device,
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
### Why aren't compromised/infected machines in Chrome's threat model?

Although the attacker may now be remote, the consequences are essentially the
same as with physically-local attacks. The attacker's code, when it runs as
your user account on your machine, can do anything you can do. (See also
[Microsoft's Ten Immutable Laws Of
Security](https://web.archive.org/web/20160311224620/https://technet.microsoft.com/en-us/library/hh278941.aspx).)

Other cases covered by this section include leaving a debugger port open to
the world, remote shells, and so forth.

<a name="TOC-Does-entering-JavaScript:-URLs-in-the-URL-bar-or-running-script-in-the-developer-tools-mean-there-s-an-XSS-vulnerability-"></a>
### Does entering JavaScript: URLs in the URL bar or running script in the developer tools mean there's an XSS vulnerability?

[No](https://crbug.com/81697). Chrome does not attempt to prevent the user from
knowingly running script against loaded documents, either by entering script in
the Developer Tools console or by typing a JavaScript: URI into the URL bar.
Chrome and other browsers do undertake some efforts to prevent *paste* of script
URLs in the URL bar (to limit
[social-engineering](https://blogs.msdn.microsoft.com/ieinternals/2011/05/19/socially-engineered-xss-attacks/))
but users are otherwise free to invoke script against pages using either the URL
bar or the DevTools console.

<a name="TOC-Does-executing-JavaScript-from-a-bookmark-mean-there-s-an-XSS-vulnerability-"></a>
### Does executing JavaScript from a bookmark or the Home button mean there's an XSS vulnerability?

No. Chromium allows users to create bookmarks to JavaScript URLs that will run
on the currently-loaded page when the user clicks the bookmark; these are called
[bookmarklets](https://en.wikipedia.org/wiki/Bookmarklet).

Similarly, the Home button may be configured to invoke a JavaScript URL when clicked.

<a name="TOC-Does-executing-JavaScript-in-a-PDF-file-mean-there-s-an-XSS-vulnerability-"></a>
### Does executing JavaScript in a PDF file mean there's an XSS vulnerability?

No. PDF files have the ability to run JavaScript, usually to facilitate field
validation during form fill-out. Note that the set of bindings provided to
the PDF are more limited than those provided by the DOM to HTML documents, nor
do PDFs get any ambient authority based upon the domain from which they are
served (e.g. no document.cookie).

<a name="TOC-Are-PDF-files-static-content-in-Chromium-"></a>
### Are PDF files static content in Chromium?

No. PDF files have some powerful capabilities including invoking printing or
posting form data. To mitigate abuse of these capabiliies, such as beaconing
upon document open, we require interaction with the document (a "user gesture")
before allowing their use.

<a name="TOC-What-about-URL-spoofs-using-Internationalized-Domain-Names-IDN-"></a>
### What about URL spoofs using Internationalized Domain Names (IDN)?

We try to balance the needs of our international userbase while protecting users
against confusable homograph attacks. Despite this, there are a list of known
IDN display issues we are still working on.

*    Please see [this document](https://docs.google.com/document/d/1_xJz3J9kkAPwk3pma6K3X12SyPTyyaJDSCxTfF8Y5sU)
for a list of known issues and how we handle them.
*    [This document](https://chromium.googlesource.com/chromium/src/+/main/docs/idn.md)
describes Chrome's IDN policy in detail.

<a name="TOC-Chrome-silently-syncs-extensions-across-devices.-Is-this-a-security-vulnerability-"></a>
### Chrome silently syncs extensions across devices. Is this a security vulnerability?

This topic has been moved to the [Extensions Security FAQ](https://chromium.googlesource.com/chromium/src/+/main/extensions/docs/security_faq.md).

<a name="TOC-Why-arent-null-pointer-dereferences-considered-security-bugs-"></a>
### Why aren't null pointer dereferences considered security bugs?

Null pointer dereferences with consistent, small, fixed offsets are not considered
security bugs. A read or write to the NULL page results in a non-exploitable crash.
If the offset is larger than 32KB, or if there's uncertainty about whether the
offset is controllable, it is considered a security bug.

All supported Chrome platforms do not allow mapping memory in at least the first
32KB of address space:

- Windows: Windows 8 and later disable mapping the first 64k of address space;
  see page 33 of [Exploit Mitigation Improvements in Windows
  8][windows-null-page-mapping] [[archived]][windows-null-page-mapping-archived].
- Mac and iOS: by default, the linker reserves the first 4GB of address space
  with the `__PAGEZERO` segment for 64-bit binaries.
- Linux: the default `mmap_min_addr` value for supported distributions is at
  least 64KB.
- Android: [CTS][android-mmap_min_addr] enforces that `mmap_min_addr` is set to
  exactly 32KB.
- ChromeOS: the [ChromeOS kernels][chromeos-mmap_min_addr] set the default
  `mmap_min_addr` value to at least 32KB.
- Fuchsia: the [userspace base address][fuchsia-min-base-address] begins at 2MB;
  this is configured per-platform but set to the same value on all platforms.

[windows-null-page-mapping]: https://media.blackhat.com/bh-us-12/Briefings/M_Miller/BH_US_12_Miller_Exploit_Mitigation_Slides.pdf
[windows-null-page-mapping-archived]: https://web.archive.org/web/20230608131033/https://media.blackhat.com/bh-us-12/Briefings/M_Miller/BH_US_12_Miller_Exploit_Mitigation_Slides.pdf
[android-mmap_min_addr]: https://android.googlesource.com/platform/cts/+/496152a250d10e629d31ac90b2e828ad77b8d70a/tests/tests/security/src/android/security/cts/KernelSettingsTest.java#43
[chromeos-mmap_min_addr]: https://source.chromium.org/search?q=%22CONFIG_DEFAULT_MMAP_MIN_ADDR%3D%22%20path:chromeos%2F&ss=chromiumos%2Fchromiumos%2Fcodesearch:src%2Fthird_party%2Fkernel%2F
[fuchsia-min-base-address]: https://cs.opensource.google/fuchsia/fuchsia/+/main:zircon/kernel/arch/arm64/include/arch/kernel_aspace.h;l=20;drc=eeceea01eee2615de74b1339bcf6e6c2c6f72769

<a name="TOC-Indexing-a-container-out-of-bounds-hits-a-libcpp-verbose-abort--is-this-a-security-bug-"></a>
### Indexing a container out of bounds hits a __libcpp_verbose_abort, is this a security bug?

`std::vector` and other containers are now protected by libc++ hardening on all
platforms [crbug.com/1335422](https://crbug.com/1335422). Indexing these
containers out of bounds is now a safe crash - if a proof-of-concept reliably
causes a crash in production builds we consider these to be functional rather than
security issues.

<a name="TOC-Are-stack-overflows-considered-security-bugs-"></a>
### Are stack overflows considered security bugs?

No. Guard pages mean that stack overflows are considered unexploitable, and
are regarded as [denial of service bugs](#TOC-Are-denial-of-service-issues-considered-security-bugs-).
The only exception is if an attacker can jump over the guard pages allocated by
the operating system and avoid accessing them, e.g.:

*    A frame with a very large stack allocation.
*    C variable length array with an attacker-controlled size.
*    A call to `alloca()` with an attacker-controlled size.

<a name="TOC-Are-tint-ICE-considered-security-bugs-"></a>
### Are tint shader compiler Internal Compiler Errors considered security bugs?

No. When tint fails and throws an ICE (Internal Compiler Error), it will
terminate the process in an intentional manner and produce no shader output.
Thus there is not security bug that follows from it.

<a name="TOC-Are-enterprise-admins-considered-privileged-"></a>
### Are enterprise admins considered privileged?

Chrome [can't guard against local
attacks](#TOC-Why-aren-t-physically-local-attacks-in-Chrome-s-threat-model-).
Enterprise administrators often have full control over the device. Does Chrome
assume that enterprise administrators are as privileged and powerful as other
local users? It depends:

* On a fully managed machine, for example a [domain-joined Windows
  machine](https://docs.microsoft.com/en-us/windows-server/identity/ad-fs/deployment/join-a-computer-to-a-domain),
  a device managed via a Mobile Device Management product, or a device with
  Chrome managed via machine-level [Chrome Browser Cloud
  Management](https://support.google.com/chrome/?p=cloud_management),
  the administrator effectively has privileges to view and mutate any state on
  the device. Chrome [policy implementations](../enterprise/add_new_policy.md)
  should still guide enterprise admins to the most user-respectful defaults
  and policy description text should clearly describe the nature of the
  capabilities and the user impact of them being granted.
* On an unmanaged machine, Chrome profiles [can be managed via cloud
  policy](https://support.google.com/chrome/?p=manage_profiles)
  if users sign into Chrome using a managed account. These policies are called
  *user policies*. In this scenario, the Chrome enterprise administrator should
  have privileges only to *view and mutate state within the profile that they
  administer*. Any access outside that profile requires end-user consent.

Chrome administrators can force-install Chrome extensions without permissions
prompts, so the same restrictions must apply to the Chrome extension APIs.

Chrome has a long history of policy support with many hundreds of policies. We
recognize that there may exist policies or policy combinations that can provide
capabilities outside of the guidance provided here. In cases of clear violation
of user expectations, we will attempt to remedy these policies and we will apply
the guidance laid out in this document to any newly added policies.

See the [Web Platform Security
guidelines](https://chromium.googlesource.com/chromium/src/+/main/docs/security/web-platform-security-guidelines.md#enterprise-policies)
for more information on how enterprise policies should interact with Web
Platform APIs.

<a name="TOC-Can-I-use-EMET-to-help-protect-Chrome-against-attack-on-Microsoft-Windows-"></a>
### Can I use EMET to help protect Chrome against attack on Microsoft Windows?

There are [known compatibility
problems](https://sites.google.com/a/chromium.org/dev/Home/chromium-security/chromium-and-emet)
between Microsoft's EMET anti-exploit toolkit and some versions of Chrome. These
can prevent Chrome from running in some configurations. Moreover, the Chrome
security team does not recommend the use of EMET with Chrome because its most
important security benefits are redundant with or superseded by built-in attack
mitigations within the browser. For users, the very marginal security benefit is
not usually a good trade-off for the compatibility issues and performance
degradation the toolkit can cause.

<a name="TOC-dangling-pointers"></a>
### Dangling pointers

Chromium can be instrumented to detect [dangling
pointers](https://chromium.googlesource.com/chromium/src/+/main/docs/dangling_ptr.md):

Notable build flags are:
- `enable_dangling_raw_ptr_checks=true`
- `use_raw_ptr_asan_unowned_impl=true`

Notable runtime flags are:
- `--enable-features=PartitionAllocDanglingPtr`

It is important to note that detecting a dangling pointer alone does not
necessarily indicate a security vulnerability. A dangling pointer becomes a
security vulnerability only when it is dereferenced and used after it becomes
dangling.

In general, dangling pointer issues should be assigned to feature teams as
ordinary bugs and be fixed by them. However, they can be considered only if
there is a demonstrable way to show a memory corruption. e.g. with a POC causing
crash with ASAN **without the flags above**.

## Certificates & Connection Indicators

<a name="TOC-Where-are-the-security-indicators-located-in-the-browser-window-"></a>
### Where are the security indicators located in the browser window?

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
chrome. This visual detail can't be imitated by the page itself since the page
is confined to the viewport.

<a name="TOC-Why-does-Chrome-show-a-lock-even-if-my-HTTPS-connection-is-being-proxied-"></a>
### Why does Chrome show a lock, even if my HTTPS connection is being proxied?

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
### Why can’t I select Proceed Anyway on some HTTPS error screens?

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
### How does key pinning interact with local proxies and filters?

To enable certificate chain validation, Chrome has access to two stores of trust
anchors (i.e., certificates that are empowered as issuers). One trust anchor
store is for authenticating public internet servers, and depending on the
version of Chrome being used and the platform it is running on, the
[Chrome Root Store](https://chromium.googlesource.com/chromium/src/+/main/net/data/ssl/chrome_root_store/faq.md#what-is-the-chrome-root-store)
might be in use. The private store contains certificates installed by the user
or the administrator of the client machine. Private intranet servers should
authenticate themselves with certificates issued by a private trust anchor.

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
[MITM](https://en.wikipedia.org/wiki/Man-in-the-middle_attack)) connections,
even to pinned sites. “Data loss prevention” appliances, firewalls, content
filters, and malware can use this feature to defeat the protections of key
pinning.

We deem this acceptable because the proxy or MITM can only be effective if the
client machine has already been configured to trust the proxy’s issuing
certificate — that is, the client is already under the control of the person who
controls the proxy (e.g. the enterprise’s IT administrator). If the client does
not trust the private trust anchor, the proxy’s attempt to mediate the
connection will fail as it should.

<a name="TOC-When-is-key-pinning-enabled-"></a>
### When is key pinning enabled?

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
[https://pinning-test.badssl.com/](https://pinning-test.badssl.com/). If key
pinning is active the load will _fail_ with a pinning error.

<a name="TOC-How-does-certificate-transparency-interact-with-local-proxies-and-filters-"></a>
### How does Certificate Transparency interact with local proxies and filters?

Just as [pinning only applies to publicly-trusted trust
anchors](#TOC-How-does-key-pinning-interact-with-local-proxies-and-filters-),
Chrome only evaluates Certificate Transparency (CT) for publicly-trusted trust
anchors. Thus private trust anchors, such as for enterprise middle-boxes and AV
proxies, do not need to be publicly logged in a CT log.

<a name="TOC-Why-are-some-web-platform-features-only-available-in-HTTPS-page-loads-"></a>
### Why are some web platform features only available in HTTPS page-loads?

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

See the [Web Platform Security
guidelines](https://chromium.googlesource.com/chromium/src/+/main/docs/security/web-platform-security-guidelines.md#encryption)
for more information on security guidelines applicable to web platform APIs.

<a name="TOC-Which-origins-are-secure-"></a>
### Which origins are "secure"?

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
### What's the story with certificate revocation?

Chrome's primary mechanism for checking certificate revocation status is
[CRLSets](https://dev.chromium.org/Home/chromium-security/crlsets).
Additionally, by default, [stapled Online Certificate Status Protocol (OCSP)
responses](https://en.wikipedia.org/wiki/OCSP_stapling) are honored.

As of 2024, Chrome enforces most security-relevant certificate revocations that
are visible via Certificate Revocation Lists (CRLs) published to the
[CCADB](https://www.ccadb.org/) via CRLSets. There is some inherent delay in
getting revocation information to Chrome clients, but most revocations should
reach most users within a few days of appearing on a CA's CRL.

Chrome clients do not, by default, perform "online" certificate revocation
status checks using CRLs directly or via OCSP URLs included in certificates.
This is because online checks offer limited security value unless a client, like
Chrome, refuses to connect to a website if it cannot get a valid response,

Unfortunately, there are many widely-prevalent causes for why a client
might be unable to get a valid certificate revocation status response to
include:
* timeouts (e.g., an OCSP responder is online but does not respond within an
  acceptable time limit),
* availability issues (e.g., the OCSP responder is offline),
* invalid responses (e.g., a "stale" or malformed status response), and
* local network attacks misrouting traffic or blocking responses.

Additional concern with OCSP checks are related to privacy. OCSP
requests reveal details of individuals' browsing history to the operator of the
OCSP responder (i.e., a third party). These details can be exposed accidentally
(e.g., via data breach of logs) or intentionally (e.g., via subpoena). Chrome
used to perform revocation checks for Extended Validation certificates, but that
behavior was disabled in 2022 for [privacy reasons](https://groups.google.com/a/mozilla.org/g/dev-security-policy/c/S6A14e_X-T0/m/T4WxWgajAAAJ).

The following enterprise policies can be used to change the default revocation
checking behavior in Chrome, though these may be removed in the future:
* [enable soft-fail OCSP](https://chromeenterprise.google/policies/#EnableOnlineRevocationChecks)
* [hard-fail for local trust anchors](https://chromeenterprise.google/policies/#RequireOnlineRevocationChecksForLocalAnchors).

## Passwords & Local Data

<a name="TOC-What-about-unmasking-of-passwords-with-the-developer-tools-"></a>
### What about unmasking of passwords with the developer tools?

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

<a name="TOC-Is-Chrome-s-support-for-userinfo-in-HTTP-URLs-e.g.-http:-user:password-example.com-considered-a-vulnerability-"></a>
### Is Chrome's support for userinfo in HTTP URLs (e.g. http://user:password@example.com) considered a vulnerability?

[Not at this time](https://crbug.com/626951). Chrome supports HTTP and HTTPS
URIs with username and password information embedded within them for
compatibility with sites that require this feature. Notably, Chrome will
suppress display of the username and password information after navigation in
the URL box to limit the effectiveness of spoofing attacks that may try to
mislead the user. For instance, navigating to
`http://trustedsite.com@evil.example.com` will show an address of
`http://evil.example.com` after the page loads.

Note: We often receive reports calling this an "open redirect". However, it has
nothing to do with redirection; rather the format of URLs is complex and the
userinfo may be misread as a host.

<a name="TOC-Why-does-the-Password-Manager-ignore-autocomplete-off-for-password-fields-"></a>
### Why does the Password Manager ignore `autocomplete='off'` for password fields?

Ignoring `autocomplete='off'` for password fields allows the password manager to
give more power to users to manage their credentials on websites. It is the
security team's view that this is very important for user security by allowing
users to have unique and more complex passwords for websites. As it was
originally implemented, autocomplete='off' for password fields took control away
from the user and gave control to the web site developer, which was also a
violation of the [priority of
constituencies](https://www.schemehostport.com/2011/10/priority-of-constituencies.html).
For a longer discussion on this, see the [mailing list
announcement](https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/zhhj7hCip5c).

<a name="TOC-Signout-of-Chrome"></a>
### Signing out of Chrome does not delete previously-synced data?

If you have signed into Chrome and subsequently sign out of Chrome, previously
saved passwords and other data are not deleted from your device unless you
select that option when signing out of Chrome.

If you change your Google password, synced data will no longer be updated in
Chrome instances until you provide the new password to Chrome on each device
configured to sync. However, previously synced data [remains available](https://crbug.com/792967)
on each previously-syncing device unless manually removed.

<a name="TOC-Why-doesn-t-the-Password-Manager-save-my-Google-password-if-I-am-using-Chrome-Sync-"></a>
### Why doesn't the Password Manager save my Google password if I am using Chrome Sync?

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
### Does the Password Manager store my passwords encrypted on disk?

Chrome generally tries to use the operating system's user storage mechanism
wherever possible and stores them encrypted on disk, but it is platform
specific:

*    On Windows, Chrome uses the [Data Protection API
     (DPAPI)](https://msdn.microsoft.com/en-us/library/ms995355.aspx) to bind
     your passwords to your user account and store them on disk encrypted with
     a key only accessible to processes running as the same logged on user.
*    On macOS and iOS, Chrome previously stored credentials directly in the user's
     Keychain, but for technical reasons, it has switched to storing the
     credentials in "Login Data" in the Chrome users profile directory, but
     encrypted on disk with a key that is then stored in the user's Keychain.
     See [Issue 466638](https://crbug.com/466638) and [Issue 520437](https://crbug.com/520437) for further explanation.
*    On Linux, Chrome previously stored credentials directly in the user's
     Gnome Secret Service or KWallet, but for technical reasons, it has switched to
     storing the credentials in "Login Data" in the Chrome user's profile directory,
     but encrypted on disk with a key that is then stored in the user's Gnome
     Secret Service or KWallet. If there is no available Secret Service or KWallet,
     the data is not encrypted when stored.
*    On Android, Chrome doesn't store in the profile anymore, instead it uses Google
     Play Services to access passwords stored on a device.
*    On ChromeOS passwords are only obfuscated since all profile data is encrypted
     by the OS.

<a name="TOC-If-theres-a-way-to-see-stored-passwords-without-entering-a-password--is-this-a-security-bug-"></a>
### If there's a way to see stored passwords without entering a password, is this a security bug?

No. If an attacker has control of your login on your device, they can get to
your passwords by inspecting Chrome disk files or memory. (See
[why aren't physically-local attacks in Chrome's threat
model](#TOC-Why-aren-t-physically-local-attacks-in-Chrome-s-threat-model-)).

On some platforms we ask for a password before revealing stored passwords,
but this is not considered a robust defense. It’s historically to stop
users inadvertently revealing their passwords on screen, for example if
they’re screen sharing. We don’t do this on all platforms because we consider
such risks greater on some than on others.


<a name="TOC-On-some-websites-I-can-use-a-passkey-without-passing-a-lock-screen-or-biometric-challenge-is-this-a-security-bug"></a>
### On some websites, I can use passkeys without passing a lock screen or biometric challenge. Is this a security bug?

Probably not. When a website requests a passkeys signature, it can choose
whether the authenticator should perform user verification (e.g. with a local
user lock screen challenge). Unless the website sets user verification parameter
in the request to 'required', the passkey authenticator can choose to skip the
lock screen challenge. Authenticators commonly skip an optional challenge if
biometrics are unavailable (e.g. on a laptop with a closed lid).

If you can demonstrate bypassing the user verification challenge where the
request user verification parameter is set to 'required', please
[report it](https://issues.chromium.org/issues/new?noWizard=true&component=1363614&template=1922342).

## Other

<a name="TOC-What-is-the-security-story-for-Service-Workers-"></a>
### What is the security story for Service Workers?

See our dedicated [Service Worker Security
FAQ](https://chromium.googlesource.com/chromium/src/+/main/docs/security/service-worker-security-faq.md).

<a name="TOC-What-is-the-security-story-for-Extensions-"></a>
### What is the security story for Extensions?

See our dedicated [Extensions Security FAQ](https://chromium.googlesource.com/chromium/src/+/main/extensions/docs/security_faq.md).

<a name="TOC-What-is-the-security-model-for-Chrome-Custom-Tabs-"></a>
### What's the security model for Chrome Custom Tabs?

See our [Chrome Custom Tabs security FAQ](custom-tabs-faq.md).

<a name="TOC-How-is-security-different-in-Chrome-for-iOS--"></a>
### How is security different in Chrome for iOS?

Chrome for iOS does not use Chrome's standard rendering engine. Due to Apple's
iOS platform restrictions, it instead uses Apple's WebKit engine and a more
restricted process isolation model. This means its security properties are
different from Chrome on all other platforms.

The differences in security are far too extensive to list exhaustively, but some
notable points are:

* Chromium's [site
  isolation](https://www.chromium.org/Home/chromium-security/site-isolation/)
  isn't used; WebKit has its own alternative implementation with different costs
  and benefits.
* WebKit has [historically been slower at shipping security
  fixes](https://googleprojectzero.blogspot.com/2022/02/a-walk-through-project-zero-metrics.html).
* Chrome's network stack, [root
  store](https://www.chromium.org/Home/chromium-security/root-ca-policy/) and
  associated technology are not used, so
  the platform will make different decisions about what web servers to trust.
* Sandboxing APIs are not available for native code.

Given that the fundamentals of the browser are so different, and given these
limitations, Chrome for iOS has historically not consistently implemented some
of Chrome's [standard security guidelines](rules.md). This includes the
important [Rule of Two](rule-of-2.md). Future Chrome for iOS features should
meet all guidelines except in cases where the lack of platform APIs make it
unrealistic. (The use of WebAssembly-based sandboxing is currently considered
unrealistic though this could change in future.)

If the Rule of Two cannot be followed, features for Chrome for iOS should
nevertheless follow it as closely as possible, and adopt additional mitigations
where they cannot:

* First consider adding a validation layer between unsafe code and web contents,
  or adopting memory-safe parsers at the boundary between the renderer and the
  browser process. Consider changing the design of the feature so the riskiest
  parsing can happen in javascript injected in the renderer process.
* Any unsafe unsandboxed code that is exposed to web contents or other
  untrustworthy data sources must be extensively tested and fuzzed.

The Chrome team is enthusiastic about the future possibility of making a version
of Chrome for iOS that meets our usual security standards if richer platform
facilities become widely available: this will require revisiting existing
features to see if adjustment is required.

<a name="TOC-Are-all-Chrome-updates-important--"></a>
### Are all Chrome updates important?

Yes - see [our updates FAQ](updates.md).

<a name="TOC-What-older-Chrome-versions-are-supported--"></a>
### What older Chrome versions are supported?

We always recommend being on the most recent Chrome stable version - see
[our updates FAQ](updates.md).

<a name="TOC-Im-making-a-Chromium-based-browser-how-should-I-secure-it-"></a>
### I'm making a Chromium-based browser. How should I secure it?

If you want to make a browser based on Chromium, you should stay up to date
with Chromium's security fixes. There are adversaries who weaponize fixed
Chromium bugs ("n-day vulnerabilities") to target browsers which haven’t yet
absorbed those fixes.

Decide whether your approach is to stay constantly up to date with Chromium
releases, or to backport security fixes onto some older version, upgrading
Chromium versions less frequently.

Backporting security fixes sounds easier than forward-porting features, but in
our experience, this is false. Chromium releases 400+ security bug fixes per
year ([example
query](https://bugs.chromium.org/p/chromium/issues/list?q=type%3DBug-Security%20has%3Arelease%20closed%3Etoday-730%20closed%3Ctoday-365%20allpublic&can=1)).
Some downstream browsers take risks by backporting only Medium+ severity fixes,
but that's still over 300 ([example
query](https://bugs.chromium.org/p/chromium/issues/list?q=type%3DBug-Security%20has%3Arelease%20closed%3Etoday-730%20closed%3Ctoday-365%20allpublic%20Security_Severity%3DMedium%2CHigh%2CCritical&can=1)).
Most are trivial cherry-picks; but others require rework and require versatile
engineers who can make good decisions about any part of a large codebase.

Our recommendation is to stay up-to-date with Chrome's released versions. You
should aim to release a version of your browser within just a few days of each
Chrome [stable
release](https://chromereleases.googleblog.com/search/label/Stable%20updates).
If your browser is sufficiently widely-used, you can [apply for advance notice
of fixed vulnerabilities](https://www.chromium.org/Home/chromium-security/) to
make this a little easier.

Finally, if you choose the backporting approach, please explain the security
properties to your users. Some fraction of security improvements cannot be
backported. This can happen for several reasons, for example: because they
depend upon architectural changes (e.g. breaking API changes); because the
security improvement is a significant new feature; or because the security
improvement is the removal of a broken feature.
