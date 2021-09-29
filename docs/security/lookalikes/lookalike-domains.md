# "Lookalike" Warnings in Google Chrome

[TOC]

## What are lookalike warnings?

"Lookalike" domains are domains that are crafted to impersonate the URLs of
other sites in order to trick users into believing they're on a different site.
These domains are used in social engineering attacks, from phishing to retail
fraud.

In addition to [Google Safe Browsing](https://safebrowsing.google.com/)
protections, Chrome attempts to detect these lookalike domains using a number of
on-device heuristics. These heuristics compare the visited URL against domains
that the user has visited previously and other popular domains.

When Chrome detects a potential lookalike domain, it may block the page and show
a full-page warning, or it may show a warning overlay, depending on how certain
Chrome is that the site is a spoof. These warnings typically have a "Did you
mean ...?" message.

| High-confidence warnings               | Low-confidence warning        |
|:--------------------------------------:|:-----------------------------:|
| ![Interstitial page](interstitial.png) | ![Safety Tip bubble](tip.png) |

These warnings do not indicate that the site the user has visited is malicious.
They only indicate that the site looks like another site, and the user should
make sure that they're on the site that they expected.

## Examples of lookalike domains

Chrome's heuristics are designed to detect spoofing techniques in the wild. Some
example "lookalike" patterns include:

 * Domains that are a small edit-distance away from other domains, such as
   `goog0le.com`.
 * Domains that embed other domain names within their own hostname, such as
   `google.com.example.com`.
 * Domains that use IDN
   [homographs](https://chromium.googlesource.com/chromium/src/+/main/docs/idn.md),
   such as `goögle.com`.

This list is not exhaustive, and developers are encouraged to avoid using
domains that users without technical backgrounds may confuse for another site.


## Heuristics are imperfect

Like all heuristics, Chrome's heuristics are not always right. For instance,
attackers can choose lookalike domains that Chrome is unable to detect. Our
intent with Chrome's lookalike heuristics is not to make spoofing impossible,
but to force attackers to use less convincing lookalikes, allowing users to
notice spoofs more easily.

In addition to not catching all spoofs, Chrome's heuristics also label some
benign pages as lookalikes. We have several approaches to minimize these
mistakes:

 * Heuristics are tuned to minimize warnings on legitimate pages.
 * Users are never prohibited from visiting the site requested, and the warnings
   shown are designed to be helpful and informative, rather than scary.
 * We monitor what sites trigger the most warnings on a regular basis, and
   disable warnings on identified false positives.
 * For domains used internally, we provide an [Enterprise
   Policy](https://cloud.google.com/docs/chrome-enterprise/policies/?policy=LookalikeWarningAllowlistDomains)
   allowing businesses to selectively disable these warnings as needed for their
   users.
 * For several months following the roll-out of new heuristics, we accept
   appeals from site operators whose sites have been incorrectly flagged.
 * Heuristics launching in Chrome 88 or later will trigger a console message
   informing site owners of the issue for at least one release prior to
   triggering user-visible warnings.


## Not all users see all warnings

Chrome shows warnings in part based on a users' browsing history. This allows
Chrome to be both more helpful (by providing better recommendations) and make
fewer mistakes (by not flagging lookalikes for irrelevant sites).

Chrome only shows warnings on sites that the user has not used frequently.
Further, Chrome will only recommend sites that are either well-known (i.e. top)
sites, or the user has an established relationship.

Sites that show a warning to you may not show for another user, unless that user
has visited the same sites that you have.

## Removing Lookalike Warnings from a site

It is possible to remove warnings on sites where Chrome is incorrectly showing
a warning.
 * If you are the owner of both the site showing the warning and the site that
   Chrome thinks users should visit, you can
   [**verify ownership of both sites**](#automated-warning-removal).
 * If you are not the owner of both sites, or you can't follow the automated
   process, you can [**request a manual review**](#requesting-a-manual-review).
 
 
### Automated warning removal

If you own both the site where Chrome is showing a warning, as well as the site
that Chrome is recommending, you can suppress these warnings by proving that you
control both sites. To do this, Chrome uses a special form of 
[Digital Asset Links](https://developers.google.com/digital-asset-links).

#### Instructions
1.  Assuming you own both `example.com` and `example.net`, create a file
named `assetlinks.json` and put the following contents in it:
```
[{
  "relation": ["lookalikes/allowlist"],
  "target" : { "namespace": "web", "site": "https://example.com"}
},{
  "relation": ["lookalikes/allowlist"],
  "target" : { "namespace": "web", "site": "https://example.net"}
}]
```

2. Upload this file to the following URLs:
  - `https://example.com/.well-known/assetlinks.json`
  - `https://example.net/.well-known/assetlinks.json`
3. Fill out a self-verification [request](https://forms.gle/DsoM64EmSZ5H4bNd8).

Once you submit the request, please allow a few days for all warnings to stop.
If verification fails, you should be notified via email within a few hours. If
you don't get an email indicating verification failure and your sites still show
a warning after a week, please submit a manual review
[request](https://forms.gle/BxV3JGbCbRjucDxq6).

Important notes:
 * You must keep the `assetlinks.json` file in place so long as you wish to
   suppress the warnings. If you remove either file, Chrome may resume showing
   warnings.
 * You can extend the example `assetlinks.json` to support more than two
   domains, or to support additional Digital Asset Links entries, if needed.
   Please note that Chrome does not support `include` statements in
   `assetlinks.json` files.

### Requesting a manual review

If a site triggers erroneous lookalike warnings in Chrome,
you can ask for a manual appeal. These appeals are evaluated manually, and we
can suppress the warning for all Chrome users when necessary.

In the case of compelling spoofs, we may ask you to demonstrate that you not
only own the site on which the warning is shown, but the site that Chrome
believes that your site is spoofing. We may also opt to suppress warnings, but
only for a limited period of time (generally 6 months). This is generally used
for cases where a site is changing its domain name.

Appeals for domains triggered by a given heuristic are generally considered for
the 6 months following the release of that heuristic. These six months are
designed to allow Chrome to detect most existing sites that trigger the
heuristic erroneously. After that time, we encourage developers to test their
new sites in Chrome to ensure that their new domain does not trigger warnings.

If you are a site owner and would like to request an appeal, please fill out
a [request](https://forms.gle/BxV3JGbCbRjucDxq6).


#### Reasons an appeal might be denied

There are several reasons that may lead us to deny your appeal. Most commonly,
appeals are denied for domains that are only used internally (i.e. for testing
or in an enterprise setting) or where we think few users will see the warning.

For sites that are used for testing or in an enterprise setting, we recommend
using the [Enterprise
Policy](https://cloud.google.com/docs/chrome-enterprise/policies/?policy=LookalikeWarningAllowlistDomains)
to suppress the warning for impacted users.

For newly created sites, we encourage domain owners to choose domains that do
not look like domains used by other sites commonly visited by your users.

Many warnings are also only encountered by a small fraction of users who happen
to intersect with both sites. See a further description of this above.
