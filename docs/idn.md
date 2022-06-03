# Internationalized Domain Names (IDN) in Google Chrome

## Background

Many years ago, domains could only consist of the Latin letters A to Z, digits,
and a few other characters. [Internationalized Domain Names
(IDNs)](https://en.wikipedia.org/wiki/Internationalized_domain_name) were
created to better support non-Latin alphabets for web users around the globe.

Different characters from different (or even the same!) languages can look very
similar. We’ve seen
[reports](https://bugs.chromium.org/p/chromium/issues/detail?id=683314) of
proof-of-concept attacks. These are called [homograph
attacks](https://en.wikipedia.org/wiki/IDN_homograph_attack). For example, the
Latin "a" looks a lot like the Cyrillic "а", so someone could register
`http://ebаy.com` (using Cyrillic "`а`"), which could be confused for
`http://ebay.com`. This is a limitation of how URLs are displayed in browsers in
general, not a specific bug in Chrome.

In a perfect world, domain registrars would not allow these confusable domain
names to be registered. Some domain registrars do exactly that, mostly by
restricting the characters allowed, but many do not. To better protect against
these attacks, browsers display some domains in
[punycode](https://en.wikipedia.org/wiki/Punycode) (looks like `xn--...`)
instead of the original IDN, according to their own IDN policies.

This is a challenging problem space. Chrome has a global user base of billions
of people around the world, many of whom are not viewing URLs with Latin
letters. We want to prevent confusion, while ensuring that users across
languages have a great experience in Chrome. Displaying either punycode or a
visible security warning on too wide of a set of URLs would hurt web usability
for people around the world.

Chrome and other browsers try to balance these needs by implementing IDN
policies in a way that allows IDN to be shown for valid domains, but protects
against confusable homograph attacks.

Chrome's IDN policy is one of several tools that aim to protect users.
[Google Safe Browsing](https://safebrowsing.google.com/) continues to help
protect over two billion devices every day by showing warnings to users when
they attempt to navigate to dangerous or deceptive sites or download dangerous
files. Password managers continue to remember which domain password logins are
for, and won’t automatically fill a password into a domain that is not the
exactly correct one.

## How IDN works

IDNs were devised to support arbitrary Unicode characters in hostnames in a
backward-compatible way. This works by having user agents transform hostnames
containing non-ASCII Unicode characters into an ASCII-only hostname, which can
then be sent on to DNS servers. This is done by encoding each domain label into
its punycode representation. This representation includes a four-character
prefix (`xn--`) and then the unicode translated to ASCII Compatible Encoding
(ACE). For example, `http://öbb.at` is transformed to `http://xn--bb-eka.at`.

## Google Chrome's IDN policy

Since Chrome 51, Chrome uses an IDN display policy that does not take into
account the language settings (the Accept-Language list) of the browser. A
[similar strategy](https://wiki.mozilla.org/IDN_Display_Algorithm#Algorithm) is
used by Firefox.

Google Chrome decides if it should show Unicode or punycode for each domain
label (component) of a hostname separately. To decide if a component should be
shown in Unicode, Google Chrome uses the following algorithm:
1. Convert each component stored in the ACE to Unicode per [UTS 46 transitional
   processing](http://unicode.org/reports/tr46/#Processing) (`ToUnicode`).

2. If there is an error in `ToUnicode` conversion (e.g. contains [disallowed
   characters](http://unicode.org/cldr/utility/list-unicodeset.jsp?a=%5B%3Auts46%3Ddisallowed%3A%5D&abb=on&g=&i=),
   [starts with a combining
   mark](https://unicode-org.github.io/icu-docs/apidoc/released/icu4c/uidna_8h.html#a0411cd49bb5b71852cecd93bcbf0ca2da390a6b3d9844a1dcc1f99fb1ae478ecf),
   or [violates BiDi
   rules](https://unicode-org.github.io/icu-docs/apidoc/released/icu4c/uidna_8h.html#a0411cd49bb5b71852cecd93bcbf0ca2da8a9311811fb0f3db1644ac1a88056370)),
   show punycode.

3. If there is a character in a label not belonging to [Characters allowed in
   identifiers](http://unicode.org/cldr/utility/list-unicodeset.jsp?a=%5B%3AIdentifierStatus%3DAllowed%3A&abb=on&g=&i=)
   per [Unicode Technical Standard 39 (UTS
   39)](http://www.unicode.org/reports/tr39/#Identifier_Status_and_Type), show
   punycode.

4. If any character in a label belongs to [the disallowed
   list](https://unicode.org/cldr/utility/list-unicodeset.jsp?a=%5B%5Cu01CD-%5Cu01DC%5D+%5B%5Cu1c80-%5Cu1c8f%5D++%5B%5Cu1e90-%5Cu1e9b%5D++%5B%5Cu1f00-%5Cu1fff%5D++%5B%5Cua640-%5Cua69f%5D-%5B%5Cua720-%5Cua72f%5D+%5B%5Cu0338+%5Cu058a+%5Cu2010+%5Cu2019+%5Cu2027+%5Cu30a0+%5Cu02bb+%5Cu02bc+%5D&abb=on&g=&i=),
   show punycode.

5. If the component uses characters drawn from multiple scripts, it is subject
to a script mixing check based on ["Highly Restrictive" profile of UTS
39](http://www.unicode.org/reports/tr39/#Restriction_Level_Detection) with an
additional restriction on Latin. If the component fails the check, show the
component in punycode.
  - Latin, Cyrillic or Greek characters cannot be mixed with each other
  - Latin characters in the ASCII range can be mixed ONLY with Chinese (Han,
    Bopomofo), Japanese (Kanji, Katakana, Hiragana), or Korean (Hangul, Hanja)
  - Han (CJK Ideographs) can be mixed with Bopomofo
  - Han can be mixed with Hiragana and Katakana
  - Han can be mixed with Korean Hangul

6. If two or more numbering systems (e.g. European digits + Bengali digits) are
mixed, show punycode.

7. If there are any invisible characters (e.g. a sequence of the same combining
mark or a sequence of Kana combining marks), show punycode.

8. If there are any characters used in an unusual way, show punycode. E.g.
[`LATIN MIDDLE DOT (·)`](https://unicode.org/cldr/utility/character.jsp?a=00B7)
used outside [ela geminada](https://en.wiktionary.org/wiki/ela_geminada).

9. Test the label for [mixed script confusable per UTS
39](http://unicode.org/reports/tr39/#Mixed_Script_Confusables). If mixed script
confusable is detected, show punycode.

10. Test the label for [whole script
confusables](http://unicode.org/reports/tr39/#Whole_Script_Confusables): If all
the letters in a given label belong to a set of whole-script-confusable letters
in one of the [whole-script-confusable
scripts](https://cs.chromium.org/chromium/src/components/url_formatter/spoof_checks/idn_spoof_checker.cc?type=cs&q=kWholeScriptConfusables&sq=package:chromium)
and if the hostname doesn't have a corresponding
[allowed top-level-domain](https://cs.chromium.org/chromium/src/components/url_formatter/spoof_checks/idn_spoof_checker.h?type=cs&q=allowed_tlds)
for that script, show punycode.
**Example for Cyrillic:**
The first label in hostname `аррӏе.com` (`xn--80ak6aa92e.com`) is all [Cyrillic
letters that look like Latin letters](http://unicode.org/cldr/utility/list-unicodeset.jsp?a=%5B%D0%B0%D1%81%D4%81%D0%B5%D2%BB%D1%96%D1%98%D3%8F%D0%BE%D1%80%D4%9B%D1%95%D4%9D%D1%85%D1%83%D1%8A%D0%AC%D2%BD%D0%BF%D0%B3%D1%B5%D1%A1%5D&g=gc&i=)
**AND** the TLD (`com`) is not Cyrillic **AND** the TLD is not one of the TLDs
known to host a large number of Cyrillic domains (e.g. `ru`, `su`, `pyc`, `ua`).
Show it in punycode.

11. If the label contains only [digits and digit
spoofs](https://cs.chromium.org/chromium/src/components/url_formatter/spoof_checks/idn_spoof_checker.cc?type=cs&q=IsDigitLookalike),
show punycode.

12. If the label matches a [dangerous
pattern](https://cs.chromium.org/chromium/src/components/url_formatter/spoof_checks/idn_spoof_checker.cc?type=cs&g=0&l=422),
show punycode.

13. If the [skeleton](http://unicode.org/reports/tr39/#def-skeleton) of the
registrable part of a hostname is identical to one of the top domains after
removing diacritic marks and mapping each character to its spoofing skeleton
(e.g. `www.googlé.com` with `é` in place of `e`), show punycode.

Otherwise, show Unicode.

This is implemented by `IDNToUnicodeOneComponent()` and `IsIDNComponentSafe()`
in
[`components/url_formatter/url_formatter.cc`](https://cs.chromium.org/search/?q=components/url_formatter/url_formatter.cc)
and `IDNSpoofChecker` class in
[`components/url_formatter/spoof_checks/idn_spoof_checker.cc`](https://cs.chromium.org/chromium/src/components/url_formatter/spoof_checks/idn_spoof_checker.cc).

## Additional Protections

In addition to the spoof checks above, Chrome also implements a full page
security warning to protect against lookalike URLs. You can find an example of 
this warning at `chrome://interstitials/lookalike`. This warning blocks main
frame navigations that involve lookalike URLs, either as a direct navigation or
as part of a redirect.

The algorithm to show this warning is as follows:

1. If the scheme of the navigation is not `http` or `https`, allow
the navigation.

2. If the navigation is a redirect, check the redirect chain. If the redirect
chain is safe, allow the navigation. (See Defensive Registrations section for
details).

3. If the hostname of the navigation has at least a medium site engagement
score, allow the navigation. Site engagement score is assigned to sites by the
[Site Engagement
Service](https://www.chromium.org/developers/design-documents/site-engagement).

4. If the hostname of the navigation is in
[`domains.list`](https://cs.chromium.org/chromium/src/components/url_formatter/spoof_checks/top_domains/domains.list),
allow the navigation.

5. If the user previously allowed the hostname of the navigation by clicking
"Ignore" in the warning, allow the navigation. Currently, user decisions are
stored per tab, so navigating to the same site in a new tab may show the
warning.

6. If the hostname has the same skeleton as a recently engaged site or a top 500
domain, block the navigation and show the warning.

All of these checks are done locally on the client side.

### Defensive Registrations

Domain owners can sometimes register multiple versions of their domains, such
as the ASCII and IDN versions, to improve user experience and prevent potential
spoofs. We call these supplementary domains defensive registrations.

In some cases, Chrome's lookalike warning may flag and block navigations to
these domains:
 - If one of the sites is in `domains.list` but the other isn't, the latter will
be blocked. 
 - If the user engaged with one of the sites but not the other, the latter will
be blocked.

### Avoiding a lookalike warning on your site

**Domain owners can avoid the "Did you mean" warning by redirecting their
defensive registrations to their canonical domain.**

**Example**: If you own both `example.com` and `éxample.com` and the majority of
your traffic is to `example.com`, you can fix the warning by redirecting
`éxample.com` to `example.com`. The lookalike warning logic considers this a
safe redirect and allows the navigation. If you must also redirect `http`
navigations to `https`, do this in a single redirect such as
`http://éxample.com -> https://example.com`. Use HTTP 301 or HTTP 302
redirects, the lookalike warning ignores meta redirects.

## Reporting Security Bugs

We reward certain cases of IDN spoofs according to [Chrome's Vulnerability
Reward Program](https://www.google.com/about/appsecurity/chrome-rewards/index.html)
policies. Please see [this
document]( https://docs.google.com/document/d/1_xJz3J9kkAPwk3pma6K3X12SyPTyyaJDSCxTfF8Y5sU/edit?usp=sharing)
before reporting a security bug.
