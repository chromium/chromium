# Guidelines for URL Display

*This document covers the best practices and pitfalls for building UI to display URLs in browsers and other apps. It covers the main categories of problems and challenges that we’ve seen in building Chrome. The guidance is intended to be generally applicable, but includes some Chrome-specific notes throughout.*

[TOC]

## Background

The [URL](https://url.spec.whatwg.org) displayed in a user agent's address bar is often the only security context directly exposed to users, and therefore in many cases it is the only signal users can reasonably rely upon to determine whether or not they should trust a particular website.

### Components of URLs

A URL is made up of a number of components, the majority of which should not have any impact on security decisions:

![Components of a URL](url_components.png)

Usually, the security context that the user cares about is the **[registrable domain](#registrabledomain)** of the top-level page’s URL's origin, even when a given page is made up of components from many different origins. The registrable domain typically consists of a subdomain of an entry on the [Public Suffix list](https://publicsuffix.org/). For instance, bbc.co.uk is a registrable domain under the co.uk public suffix.  The **fully-qualified hostname** consists of a registrable domain, and optionally one or more **subdomain** labels.

The **scheme** of the URL determines the protocol by which the content from the URL is delivered. If the scheme refers to a non-secure protocol like HTTP, and especially if the protocol traverses an untrusted network, the **registrable domain** information may not accurately describe the true source of the content because the content may have been modified by a **man-in-the-middle** on the network. The **port** number only needs to be specified if it is not the default for the scheme (e.g., 80 for HTTP, 443 for HTTPS).

Other components of the URL (**subdomain**, **userinfo**, **path**, **query**, and **fragment**) are completely under the control of the website and may be crafted in an attempt to spoof the user by misrepresenting the registrable domain.

### Challenges and Threats

Malicious websites are motivated to misrepresent their provenance in order to trick visitors into performing an unsafe action (e.g., phishing, malware install) or to otherwise grant unwarranted trust in the information provided by the site (e.g., "fake news").

As the web platform becomes more capable (introducing new features like device access, etc.), the importance of evaluating the source of web content grows more important and the desire to misrepresent its origins by bad actors increases.

## Best Practices

### TL;DR: Advice on What to Display

**Where possible, avoid displaying URLs**, especially when the user is likely to be making a trust decision. Instead, display only the **[origin](#simplify)**. Additionally, if the connection is **not secure**, add an indicator to that effect:

![Chrome showing Not Secure indicator](not_secure.png)

If the URL display only applies to secure URLs (for example, a permission prompt that can only be requested by HTTPS pages), omit the scheme and non-default ports (443 for HTTPS).

More detailed guidance follows.

### Show Origins At All Times

To avoid spoofing, the current best practice is to always show the
origin. Whenever the user is interacting with web content, they might be making
a security decision ("Should I enter my password?", “Should I trust the
information on this site?”, etc.) and the origin is what we currently rely on to
help them make that decision.

It’s also important to show an origin when Chrome is explicitly asking the user to make a security decision.

Currently, in Chrome, origins are hidden in the following special cases:

* Chrome for Android, when a user scrolls down

* Fullscreen mode across platforms

* Installed PWAs on Android

    * The origin is shown in the install dialog

* Installed PWAs on Desktop after animation

    * The origin is shown in the install dialog

### Use Only Security-Reviewed Libraries to Canonicalize and Parse Strings into URLs

Parsing and canonicalization of URLs is extremely [error-prone](https://www.blackhat.com/docs/us-17/thursday/us-17-Tsai-A-New-Era-Of-SSRF-Exploiting-URL-Parser-In-Trending-Programming-Languages.pdf), and mistakes can introduce both spoofing vulnerabilities and lower-level problems that can lead to vulnerabilities as severe as remote code execution.

Rather than attempting to parse components from a string directly, you should always obtain components of the URL via a trusted object (e.g., GURL/KURL).

### Display URLs in Canonical Form

In virtually all cases, you should display URLs in **canonical** form, and you should rely on trusted libraries to perform such canonicalization. Canonicalization includes a number of steps that help ensure that URLs are in their simplest form and the one a user is most likely to be able to understand.

Canonicalization includes:

1. Normalizing the hostname (e.g., convert to lowercase, perform IDN-related normalizations)

2. Stripping default ports

3. Path simplification

4. Unnecessarily-encoded octets are decoded

For example, canonicalization converts `https://ExAmPle.com:443/one/%2e./Tw%2fo/` to the canonical form, `https://example.com/Tw%2fo/`.

Do not attempt to write your own canonicalizer.

### Simplify URLs Whenever Possible {#simplify}

* [Do NOT display the username and password components](https://url.spec.whatwg.org/#url-rendering) of URLs (e.g., `https://user:password@example.com/`) anywhere the user is making a security decision.

    * As these credentials may be sensitive, consider omitting them wherever possible (e.g., do not include username and password when generating printouts).

* When the primary purpose of displaying a URL is to have the user make a security decision, display the **origin**, or if scheme is always HTTPS, just the **domain**. Omit the path, query string, fragment, and any other components of the URL because they provide opportunities for spoofing.

    * Do not display the scheme if it will always be https://. If the scheme is not https://, prefer to show a security indicator icon (dangerous triangle icon + "Not Secure" string on http://) rather than the scheme itself.

    * In Chrome, we often remove subdomains "www" and “m” as a special case to simplify the origin, except when they are part of the registrable domain. You can use the [`kFormatUrlOmitTrivialSubdomains`](https://cs.chromium.org/chromium/src/components/url_formatter/url_formatter.h?q=kFormatUrlOmitTrivial&sq=package:chromium&g=0&l=63) flag for `url_formatter::FormatURL` ([example usage](https://cs.chromium.org/chromium/src/components/history/core/browser/history_backend.cc?type=cs&q=OmitTrivialSubdomains&sq=package:chromium&g=0&l=127)).

    * Omit default ports (80 for http, 443 for https).

    * If in a space-constrained environment, it's acceptable to use registrable domain instead of the full origin.

### Eliding URLs

* tl;dr: use [`url_formatter::ElideUrl`](https://source.chromium.org/chromium/chromium/src/+/main:components/url_formatter/elide_url.h;l=40;drc=9bf53ab9128027a4a3df5cc10485e7962ddfad4d;bpv=1;bpt=1?q=url_formatter::ElideUrl&sq=&ss=chromium%2Fchromium%2Fsrc)

* When the full hostname cannot be displayed, elide labels starting from the front. (Right-to-Left character support means that the *front* of the string may not appear at the *left*). (Note that Chrome's omnibox behavior on desktop is currently [buggy](https://bugs.chromium.org/p/chromium/issues/detail?id=527638) in this respect.)

* Ensure that at least the registrable domain can be shown, to avoid showing **...paypal.com** when loading `https://not-really-paypal.com`.

 ![URL elision examples](elision.jpg)

### Highlighting

If showing the full URL is deemed necessary, consider highlighting to help the user focus on the most security relevant information in the URL.

* Ideally, consider highlighting only the registrable domain component, to increase awareness of situations where a malicious site uses a misleading subdomain in an attempt to fool the user:

![URL highlighting example](highlighting.png)

(Note that Chrome's omnibox does [not](https://bugs.chromium.org/p/chromium/issues/detail?id=527638#c6) currently do this style of highlighting.)

### URL Length

In general, the *web platform* does not have limits on the length of URLs (although 2^31 is a common limit). *Chrome* limits URLs to a maximum length of **2MB** for practical reasons and to avoid causing denial-of-service problems in inter-process communication.

On most platforms, Chrome’s omnibox limits URL display to **32kB** (`kMaxURLDisplayChars`) although a **1kB** limit is used on VR platforms.

Ensure that the client behaves reasonably if the length of the URL exceeds any limits:

* Origin information appears at the start of the URL, so truncating the end is typically harmless.

* Rendering a URL as an empty string in edge cases is not ideal, but truncating poorly (or crashing unexpectedly and insecurely) could be worse.

* Attackers may use long URLs to abuse other parts of the system. [DNS syntax](https://en.wikipedia.org/wiki/Domain_Name_System#Domain_name_syntax) limits fully-qualified hostnames to **253 characters** and each [label](#label) in the hostname to **63 characters**, but Chromium's GURL class does not enforce this limit.

### Display Font

Selection of fonts can have a meaningful impact on the security of a URL display. Relevant factors include:

* Visual distinctiveness for visually-similar characters and sequences (**0** vs **o**, **1** vs **l**, **w** vs. **vv**, etc.), especially in the ASCII character set:

![Visually similar characters in a URL](visually_similar.png)

* Glyphs for all necessary characters, including those used in International Domain Names

If a font is missing a glyph for a character, it may render as a blank, leading to the possibility of a spoofing vulnerability like [714196](https://crbug.com/714196).

![Spoofing vulnerability from a missing glyph](blank_spoofing.png)

Depending on the platform, font fallback may be used.

### Display of International Characters

Non-English characters present a huge attack surface for spoofing attacks but are today *relatively* uncommon in the host component of URLs worldwide. As global use of the web continues to grow, use of International Domain Names is expected to grow and URL displays should not be overly English-centric.

The primary threat of non-ASCII text in URLs is a [homoglyph attack](https://en.wikipedia.org/wiki/Homoglyph) in which ASCII characters are swapped with visually-identical (or near-identical) characters.

#### IDN Display Restrictions in Chrome

To mitigate URL spoofing attacks, Chrome follows a set of [rules](https://www.chromium.org/developers/design-documents/idn-in-google-chrome) that determine whether a given International Domain Name is shown in Unicode characters or degrades to show the ASCII [Punycode](https://en.wikipedia.org/wiki/Punycode) string (starting with "xn--"). The Punycode string is not intended to be human-readable: display of Punycode is instead only intended to defeat homoglyph attacks.

Chrome’s [`FormatURLForSecurityDisplay`](https://cs.chromium.org/chromium/src/components/url_formatter/elide_url.h?q=FormatURLForS&sq=package:chromium&l=108) function encapsulates this and other behaviors.

* Follow Chrome’s rules for [IDN Display](https://www.chromium.org/developers/design-documents/idn-in-google-chrome) to avoid homograph attacks.

* For the benefit of technical users, *consider* displaying an indicator when a URL contains non-ASCII characters. To date, Chrome has not implemented such an indicator. By way of comparison, Internet Explorer shows an icon in the address bar which opens a bubble that provides more information. An international URL indicator generally *should not* be positioned as a warning, but should allow the user to get more information about the content of the URL. Security-savvy users may find this UI useful in detecting spoofs and escalating reports to more effective security mechanisms (e.g., SafeBrowsing).

#### RTL

* Ensure that URLs are sanitized to prevent abuse by [Right-to-Left override characters](https://unicode.org/reports/tr9/#Explicit_Directional_Embeddings).

    * In Chrome Views code, a view that solely contains a URL should use `DIRECTIONALITY_AS_URL`, which handles everything correctly and will be updated if the spec changes.

    * Follow the advice in [Sneaky Unicode Characters](#sneakyunicode) below to remove all explicit overrides.

    * Wrap the URL in U+202A ... U+202C, so that it is forced to appear in a LTR paragraph. The URL standard (both [RFC 3987](https://www.ietf.org/rfc/rfc3987.txt) and https://url.spec.whatwg.org) mandate that URLs are displayed in LTR paragraphs, no matter what they contain.

<a name="sneakyunicode"></a>
#### Sneaky Unicode Characters

An attacker may abuse whitespace and line-wrapping characters in order to push the display of their true origin out of view. Such characters should be banned or displayed in %-escaped form.

* Non-breaking spaces (U+00A0) and other invisible characters (e.g., U+2000 to U+200A inclusive).

* Unicode line terminators (e.g., U+2028, U+2029, and U+0085).

* Unicode explicit directional formatting commands (e.g., U+200E--U+200F, U+202A--U+202E, see the full [list](https://unicode.org/reports/tr9/#Directional_Formatting_Characters) and [`ShouldUnescapeCodePoint`](https://cs.chromium.org/chromium/src/net/base/escape.cc?l=172&rcl=dc22553340d5c4dda162f17a07d706748be44042)).

* Characters that look like security UI (e.g., U+1F512 🔒).
    - Emoji may be confusing because users are not accustomed to seeing graphics in URL displays and may be misled into believing that they represent claims on the browser’s part (e.g., the Lock emoji). See issue [746350](https://bugs.chromium.org/p/chromium/issues/detail?id=746350).

* Use of [Combining characters](https://blog.emojipedia.org/fun-emoji-hacks/) to create look-alikes.

In Chromium, this is handled by routines in `base/strings/escape.h`. Outside of Chromium, consult `ShouldUnescapeCodePoint` in `net/base/escape.cc` for guidance.

### Literal IP Addresses

A URL may contain an IPv4 or IPv6 literal in the host component instead of a fully-qualified hostname:

![URL with an IP literal hostname](ip_example.png)

#### IPv4

The canonical representation of an IPv4 literal is [dotted-decimal](https://en.wikipedia.org/wiki/Dot-decimal_notation) (aka "dotted quad") notation, but many surfaces will accept a 32-bit decimal integer (or even [octal](https://bugs.chromium.org/p/chromium/issues/detail?id=787361) or hex) IP representation:

![URL with a decimal IP literal hostname](decimal_ip.png)
![URL with a hex IP literal hostname](hex_ip.png)

IPv4 literals should be converted to canonical form for display. If your code makes any sort of formatting or security decision based on the [absence of dots within a hostname](https://blogs.msdn.microsoft.com/ieinternals/2014/03/06/browser-arcana-ip-literals-in-urls/), be sure that it isn’t fooled by undotted IP literals.

##### IPv6

IPv6 literals use colons as a delimiter and the literal is wrapped in square brackets (this allows disambiguating the colons in the literal from the colon which begins the port component of the URL).

![URL with an IPv6 literal hostname](ipv6.png)

If an IPv6 Literal contains a Zone component, the % delimiter must be [escaped to %25](https://en.wikipedia.org/wiki/IPv6_address#Use_of_zone_indices_in_URIs).

https://[fe80::1ff:fe23:4567:890a%25eth0]/

Most surfaces will accept non-canonical forms of IPv6 literals:

![URL with a non-canonical IPv6 literal hostname](noncanonical_ipv6.png)

....but these should be converted to canonical form for display.

### Uncommon Schemes and Virtual URLs

While many users have at least *some* familiarity with HTTP and HTTPS URLs, the browser offers a number of more exotic URL schemes with which users have no experience.

Where possible, limit acceptance of URLs outside of the most common schemes (e.g., HTTP/HTTPS) to reduce risk.

[data:// URIs](https://en.wikipedia.org/wiki/Data_URI_scheme) carry the response data directly within the URL, meaning that they do not need to hit the network. Attackers have tried to spoof users by putting what *looks like* an origin at the front of the URL string, so Chrome no longer allows websites to navigate directly to Data URIs in the top frame and also labels Data URIs as "Not secure" in the event that a user loads one via other means. filesystem URIs (a Chrome-specific scheme) receive the same treatment.

![data:// URI with a Not Secure label](notsecure_data_uri.png)

filesystem URIs and blob URIs embed the origin from which they originated as the first component after the URL scheme.

![blob URI with a Not Secure label](notsecure_blob_uri.png)

![filesystem URI with a Not Secure label](notsecure_filesystem_uri.png)

![filesystem URI](filesystem_uri.png)

In Chrome, file:// schemed URIs do not contain a host component; be sure that your UI accounts for this possibility.

**view-source:** is a special URL scheme which wraps another scheme (e.g., `view-source:https://example.com`) and displays the document in a special Blink "view-source" mode. For security reasons, web content cannot navigate directly to view-source URLs.

In Chrome, a **virtual URL** is roughly the URL displayed in the omnibox even though the real url is something different. An example is `chrome://newtab/` -- under the hood it is either a local version or a remote one but that shouldn’t matter to the user who sees it as the New Tab Page. URL spoofs can easily result if the URL display surface fails to update the URL upon a navigation to a resource which is not intended to be rendered under the virtual URL (e.g., [750298](https://bugs.chromium.org/p/chromium/issues/detail?id=750298)).

### %-Escaping

The [URL Standard](https://url.spec.whatwg.org/#url-rendering) suggests that *the path, query, and fragment components of the URL should have their sequences of percent-encoded bytes replaced with code points resulting from percent decoding those sequences converted to bytes, unless that renders those sequences invisible.*

This is generally a user-experience feature (some sites strive to use human-readable URLs and %-escaped characters are not human readable) but could lead to spoofing attacks if performed incorrectly.

Chrome’s [`FormatUrl`](https://cs.chromium.org/chromium/src/components/url_formatter/url_formatter.h?l=100&rcl=1deab0dd75a1659e44b8159d60de9cf26dc3dbf0) function takes an [`UnescapeRule`](https://cs.chromium.org/chromium/src/base/strings/escape.h?l=64&rcl=ecba19472b9290092745e9846edd0d6fd8dcc48b) parameter that determines what components should be decoded for display. As of Chrome 65, we unescape path, query, and fragment components for display.

Space and other invisible characters should be displayed in encoded form.

Example URL with space and emoji:

     https://example.com/ 🍌/? 🍌# 🍌

Renders as:

![Rendering of Emojis and spaces in Chrome's omnibox](encode_for_display.png)

## Glossary

A **homograph** (or **homoglyph**) **attack** occurs when an attacker uses lookalike characters to make one URL resemble another. This can occur purely in ASCII (e.g., **1** looks like **l**, **vv** looks like **w**), or when using Unicode characters via the **International Domain Names** mechanism.

**International Domain Names** is a mechanism for using Unicode characters for hostnames. Under the covers, the Unicode labels are encoded using **[punycode](https://tools.ietf.org/html/rfc3492)** and prefixed with **xn--**. The browser may display the label in Unicode, or in the underlying punycode form. Users are not expected to be able to decode the punycode form: display in this form is intended to foil spoofing attempts.

<a name="label"></a>
A **[label](https://en.wikipedia.org/wiki/DNS_label#Parts_of_a_domain_name)** is a single component of a [domain name](https://en.wikipedia.org/wiki/Domain_name) string, delimited by periods. For instance, "*www*", “*microsoft*”, and “*com*” are the three labels in the domain name “*www.microsoft.com*”.

A **plain hostname** is an unqualified, single-label hostname like "*payroll*", which typically refers to a server on a local intranet.

A **[Public Suffix](https://publicsuffix.org/)** is the suffix portion of a FQDN under which independent entities may register subdomains. For example, *ltd.co.im* is a Public Suffix. A Public Suffix contains one or more labels. Sometimes the term "[effective TLD](https://wiki.mozilla.org/Gecko:Effective_TLD_List)" is used as a synonym.

<a name="registrabledomain"></a>
The **registrable domain** is the public suffix plus one additional label. Sometimes eTLD+1 is used as a synonym.

## A Caveat on Security Sensitive Surfaces

It should be noted that *many* displays of URLs in the web platform occur on surfaces [not deemed securable](https://docs.google.com/document/d/11-SXwzCGBlk8q1cNtb7peZjb2UjRPrKSFhOfZhTOz24/edit#).

Any URL displayed below the browser’s "[line of death](https://textslashplain.com/2017/01/14/the-line-of-death/)" is usually *inherently* spoofable (insofar as web content can usually fake the entire UI). In particular, this means that UI helpers like the [Status Bubble](https://dev.chromium.org/user-experience/status-bubble#TOC-Lack-of-Security) are inherently untrustworthy.

## Out-of-Scope for this Document

### Dynamic Displays

This document focuses on best practices for "static" display of URLs: that is, the display of a single URL.

URL spoofing vulnerabilities also include "dynamic" spoofing attacks that rely upon the browser failing to update a URL display (or failing to do so in a timely manner) based on an asynchronous operation.

If your UI allows for asynchronous operations (e.g., user can interact with one context while another loads), care must be taken to avoid mismatch between the "current" and the “loading” context.

The most common dynamic attacks rely upon the fact that navigation and script execution are typically asynchronous operations.

* An attacker starts on a page with a trusted URL and then navigates that context to malicious content. The user sees the outdated trusted URL and interacts with the malicious content, believing that it is under the control of the trusted site.

* Alternatively, the attack starts on a malicious page which begins a navigation to a trusted URL. The user sees the pending trusted URL and interacts with the malicious content believing it is under the control of the trusted site.

Dynamic attacks are particularly prevalent and challenging to mitigate in UI surfaces used both for URL input *and* display, such as the omnibox.

An additional complexity arises from the fact that some forms of navigation (those which result in a HTTP/204 response or a file download) are not expected to update the URL shown in the omnibox. As a consequence, the precise timing of updates to the URL shown in the omnibox is a critical factor in the security of the system and a common source of vulnerability. Chrome has been discussing possible UI treatments to more clearly differentiate pending and committed URLs in [crbug.com/719856](https://crbug.com/719856).

However, because dynamic attacks have little in common with static attacks and are generally very tightly tied to the behavior of the URL display surface, they are out of scope for this document.

### Misleading Domain Names

The web security model expects the user to recognize which hosts are backed by legitimate organizations (`google.com`) and which are not (`google-update.com`).

For the purposes of this document, we do not concern ourselves with the ability of a website to obtain a misleading domain name that is visually distinctive from a legitimate domain name.

There exist features, like Safe Browsing and EV certificate display, which may act as a supplement to origin display, but these features are not available in all situations and are often "best effort." Replacing or augmenting human analysis of origins remains an active topic of research and [brainstorming](https://medium.com/@owencm/rethinking-url-bars-as-primary-browser-ui-e2118339d2c0).

### "Proxy" Domain Names

In some cases, a domain owner is willing to supply content from a third-party within their own address space, leading to potential confusion about the ownership and source of the content displayed.

This is, generally, outside of the client threat-model, although in some cases (e.g., AMP), the client platform may attempt to introduce new UI to clarify the situation.

## Testing URL Displays

Chromium's open-source [Trickuri](https://github.com/chromium/trickuri) tool is a Go-based proxy server designed to enable manual testing of URL display behavior.

## Further Reading

* [The Trouble with URLs](https://docs.google.com/presentation/d/1Nr47m1qlLjV8xZfw03jVKU3GQpSwbC0PWLHhXcyMcQM/) (LocoMoco Sec Conference talk)

* [Chris Palmer’s Problems of URLs](https://noncombatant.org/2017/11/07/problems-of-urls/)

* [Rethinking URL bars](https://medium.com/@owencm/rethinking-url-bars-as-primary-browser-ui-e2118339d2c0)

* [Chromium’s URL Formatter Component (C++)](https://cs.chromium.org/chromium/src/components/url_formatter/url_formatter.cc)

* [URL interop issues across specs](https://github.com/bagder/docs/blob/master/URL-interop.md)
