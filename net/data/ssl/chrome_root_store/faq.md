# Frequently Asked Questions
Last updated: November 14, 2025

[TOC]

## General Questions

### What is the Chrome Root Store?
Chrome uses
[digital certificates](https://en.wikipedia.org/wiki/Public_key_certificate)
(often referred to as "certificates," "HTTPS certificates," or "server
authentication certificates") to ensure the connections it makes on behalf
of its users are secure and private. Certificates bind a domain name to a
public key, which Chrome uses to encrypt data sent to and from the
corresponding website.

As part of establishing a secure connection to a website, Chrome verifies
that a recognized system known as a "Certification Authority" (CA) issued
its certificate. Certificates issued by a CA not recognized by Chrome or a
user's local settings can cause users to see warnings and error pages.

Root stores, sometimes called "trust stores," tell operating systems and
applications what certificates to trust.

The
[Chrome Root Store](https://g.co/chrome/root-store) contains the set of
certificates Chrome trusts by default.

### What is the Chrome Certificate Verifier?
Historically, Chrome integrated certificate verification processes with
the platform it ran on. This resulted in inconsistent user experiences
across platforms, making it difficult for developers to understand
Chrome's expected behavior.

The launch of the Chrome Certificate Verifier addressed these concerns by
applying a common certificate verification process across Windows, macOS,
Chrome OS, Linux, and Android.

**Note:** Apple policies prevent the Chrome Certificate Verifier and
corresponding Chrome Root Store from being used on Chrome for iOS.

### When did these features land?
The Chrome Root Store and Chrome Certificate Verifier were rolled out to
Chrome users as described below.

| Chrome on...\*  | Rollout Began\*\*                    | Enabled by Default                   |
| --------------- | ------------------------------------ | ------------------------------------ |
| Android         | Chrome 114                           | Chrome 115                           |
| Chrome OS       | Chrome 114                           | Chrome 114                           |
| iOS\*\*\*       | N/A                                  | N/A                                  |
| Linux           | Chrome 114                           | Chrome 114                           |
| macOS           | Chrome 105                           | Chrome 108                           |
| Windows         | Chrome 105                           | Chrome 108                           |

**Notes:**<br>
(\*) Find Chrome browser system requirements [here.](https://support.google.com/chrome/a/answer/7100626)

(\*\*) During this release, users also had the opportunity to "opt-in" to
these features, regardless of whether they were automatically enrolled in
the rollout population.

(\*\*\*) Apple policies prevent the Chrome Root Store and Chrome
Certificate Verifier from being used on Chrome for iOS.

### How do these features impact "enterprise", "private", or "only-locally trusted" certificates?
Depending on the underlying [connection protocol](#how-does-the-chrome-certificate-verifier-consider-local-trust-decisions)
used, the Chrome Certificate Verifier considers locally-managed certificates
during the certificate verification process.

### How can I apply for my CA's inclusion in the Chrome Root Store?
CA Owners who meet the Chrome Root Program
[policy](https://g.co/chrome/root-policy) requirements may apply for a CA
certificate's inclusion in the Chrome Root Store. CAs included in the
Chrome Root Store are expected to adhere to the Chrome Root Program policy
and continue to operate in a consistent and trustworthy manner. A CA
owner's failure to follow the requirements defined in the Chrome Root
Program policy may result in a CA certificate's removal from the Chrome
Root Store, limitations on Chrome's acceptance of the certificates they
issue, or other technical or policy restrictions.

## Support and Troubleshooting

### Where can I report an issue?
If you believe the Chrome Certificate Verifier is not working as intended,
submit a [bug](https://bugs.chromium.org/p/chromium/issues/entry) and
attach a
[NetLog dump](https://www.chromium.org/for-testers/providing-network-details/)
repeating the steps leading to the issue from a new Incognito window. Add
a comment to route the bug to the Internals>Network>Certificate component
for the fastest delivery to the appropriate triage team.

If you believe you've identified a Security Bug, follow
[these](https://www.chromium.org/Home/chromium-security/reporting-security-bugs/)
instructions.

## Additional Information for Administrators, Engineers, and Power Users

### How is the Chrome Root Store updated?
Chrome uses a "[component updater](https://chromium.googlesource.com/chromium/src/+/lkgr/components/component_updater/README.md)"
tool to push specific updates to browser components without updating the
Chrome browser application. As root CA certificates are added or removed
from the Chrome Root Store, or otherwise modified by the Chrome Root
Store, the component updater will automatically propagate these changes to
user endpoints without user action.

If your enterprise has [disabled](https://chromeenterprise.google/policies/?policy=ComponentUpdatesEnabled)
component updates, endpoints will only receive updated versions of the
Chrome Root Store during Chrome browser application updates.

During routine operating conditions, the Chrome Root Store is updated
approximately quarterly. However, aperiodic updates may take place to
promote the safety and privacy of Chrome's users.

### What happens when a certificate is added to or removed from the Chrome Root Store?

Google Chrome includes or removes self-signed root CA certificates in the Chrome
Root Store as it deems appropriate at its sole discretion.

New binary releases of Chrome include the current version of the Chrome Root
Store. If there is a change to the Chrome Root Store, future binary releases of
Chrome will automatically include that change.

Existing versions of Chrome [relying](#when-did-these-features-land) on the
Chrome Root Store and capable of [receiving component updates](#how-is-the-chrome-root-store-updated)
typically receive updated versions of the Chrome Root Store within 24-hours of
the component's release. Upon receiving the updated component, Chrome will rely
on the contents of the updated root store.

Existing versions of Chrome *not* relying on the Chrome Root Store and/or that
have disabled component updates will *not* become aware of the change(s) until
installing a binary update following the publication of the updated root store.

### How does the Chrome Certificate Verifier consider local trust decisions?

For TLS connections relying on the Transmission Control Protocol (TCP), the
Chrome Certificate Verifier considers local trust decisions for both adding and
removing trust. Learn more [here](#how-does-the-chrome-certificate-verifier-integrate-with-platform-trust-stores-for-local-trust-decisions).

For TLS connections relying on the Quick UDP Internet Connections (QUIC)
protocol, the Chrome Certificate Verifier *only* considers local trust decisions
for removing trust.

### How does the Chrome Certificate Verifier integrate with platform trust stores for local trust decisions?

On **Windows**, the Chrome Certificate Verifier automatically consumes
certificates **added** to the following certificate stores:

- Local Machine (*accessed via certlm.msc*)
     - Trust:
          - Trusted Root Certification Authorities
          - Trusted People
          - Enterprise Trust -> Enterprise -> Trusted Root Certification Authorities
          - Enterprise Trust -> Enterprise -> Trusted People
          - Enterprise Trust -> Group Policy -> Trusted Root Certification Authorities
          - Enterprise Trust -> Group Policy -> Trusted People
     - Distrust:
          - Untrusted Certificates
          - Enterprise Trust -> Enterprise -> Untrusted Certificates
          - Enterprise Trust -> Group Policy -> Untrusted Certificates

- Current User (*accessed via certmgr.msc*)
     - Trust:
          - Trusted Root Certification Authorities
          - Enterprise Trust -> Group Policy -> Trusted Root Certification Authorities
     - Distrust:
          - Untrusted Certificates
          - Enterprise Trust -> Group Policy -> Untrusted Certificates

On **macOS**, the Chrome Certificate Verifier automatically consumes
certificates **added** to the following certificate stores:

- Default and System Keychains
    - Trust:
         - Any certificate where the "When using this certificate" flag is
         set to "Always Trust" or
         - Any certificate where the "Secure Sockets Layer (SSL)" flag is
         set to "Always Trust."
    - Distrust:
         - Any certificate where the "When using this certificate" flag is
         set to "Never Trust" or
         - Any certificate where the "Secure Sockets Layer (SSL)" flag is
         set to "Never Trust."

**Note:** The Chrome Certificate Verifier **does not** rely on the
contents of the default trust store shipped by the platform provider.
When viewing the contents of a platform trust store, it's important to
remember there's a difference between an enterprise or user explicitly
distributing trust for a certificate and inheriting that trust from the
default platform root store. For example, on Windows, viewing the
```Trusted Root Certification Authorities``` trust store may present a
specific CA certificate as trusted, but that certificate's trust is
inherited from the Windows Certificate Trust List, observed by viewing
the ```Trusted Root Certification Authorities\Third-Party``` trust store,
rather than explicitly being distributed as trusted by an enterprise or
user (observed in either of the ```Trusted Root Certification
Authorities\Registry```, ```Trusted Root Certification Authorities\Group
Policy```, or ```Trusted Root Certification Authorities\Enterprise```
trust stores).

### Can I use the Chrome Root Store for my application's trust needs?

The Chrome Root Store is optimized specifically for public TLS server
authentication in scenarios that align with the security and
[policy](https://googlechrome.github.io/chromerootprogram/) requirements of the
Chrome web browser when establishing secure connections to websites. Its design
and the Certification Authorities (CAs) included are curated to meet these
particular user needs, risk profiles, and security objectives.

While the contents of the Chrome Root Store are publicly available, applications
with different use cases, risk profiles, or security objectives, such as
authenticating client certificates, verifying email signatures, or validating
the authenticity of signed code, may find that the Chrome Root Store isn't an
ideal fit. Relying parties should carefully consider if their specific needs and
risk tolerance mirror those of Chrome users browsing the web. For use cases
beyond securing HTTPS connections, or where a different risk assessment is
appropriate, exploring or establishing a more tailored trust solution might be
more beneficial.

### What's the Chrome Certificate Manager?
The Chrome Certificate Manager unifies certificate management across the Chrome
platforms [relying](#when-did-these-features-land) on the Chrome Root Store,
offering a simple and common interface for modifying trust (e.g., adding
certificates, constraining certificates, or removing certificates).

It can be accessed by navigating to ```chrome://certificate-manager``` on Chrome
134 and up.

### Are there enterprise policies available to modify default certificate trust?
Yes.

Chrome's certificate management policies are described [here](https://chromeenterprise.google/policies/#CertificateManagement).

### How does Chrome support client authentication use cases?
Chrome integrates with platform certificate stores to support the use of client
authentication certificates.

Besides ensuring they are well-formed, Chrome passes client authentication
certificates to relying servers, which then evaluate and enforce their chosen
policy.

### Does Chrome rely on the client authentication EKU when establishing secure connections to websites?
No, Chrome does **not** depend on the id-kp-clientAuth Extended Key Usage (EKU)
contained in server certificates when establishing secure connections to
websites.

Beginning [June 15, 2026](https://googlechrome.github.io/chromerootprogram/#322-pki-hierarchies-included-in-the-chrome-root-store),
PKI hierarchies represented by a root CA certificate included in the Chrome Root
Store are expected to only issue TLS server authentication certificates that
contain *only* the id-kp-serverAuth EKU. This expectation does not apply to
hierarchies whose corresponding root is not included in the Chrome Root Store
(i.e., "enterprise", "private", or "only-locally trusted" PKI hierarchies).

### How can I tell which certificates are trusted by the Chrome Root Store?
The most recent version of the Chrome Root Store is available
[here](https://chromium.googlesource.com/chromium/src/+/main/net/data/ssl/chrome_root_store/root_store.md).

The Chrome Root Store is updated by Component Updater. To observe the
contents of the Chrome Root Store in use by a version of Chrome:

1. Navigate to ```chrome://system```
2. Click the ```Expand...``` button next to `chrome_root_store`
3. *The contents of the Chrome Root Store will display*

### What does it mean for a certificate in the Chrome Root Store to be constrained?
Chrome maintains a variety of mechanisms to protect its users from certificates
that put their safety and privacy at risk. One way this is accomplished is
through the use of metadata-based constraints added to CA certificates included
in the Chrome Root Store.

The set of constraints that may be applied to a CA certificate included in the
Chrome Root Store is described [here](/net/cert/root_store.proto).

To understand a constraint applied to a root included in the Chrome Root Store,
view the certificate's entry in [root_store.textproto](/net/data/ssl/chrome_root_store/root_store.textproto).

### Can I simulate a constraint before it takes effect?

Yes.

A command-line flag was added beginning in Chrome 128 that allows administrators
and power users to simulate an SCTNotAfter distrust constraint before it takes
effect.

**How to: Simulate an SCTNotAfter distrust**

1.  Close all open versions of Chrome

2.  Start Chrome using the following command-line flag, substituting the
    variables described below with actual values.

     ```--test-crs-constraints=$[comma separated list of trust anchor certificate SHA256 hashes]:sctnotafter=$[epoch timestamp in seconds]```

3.  Evaluate the effects of the flag with test websites

**Example:** The following command will simulate an SCTNotAfter distrust with
an effective date of April 30, 2024 11:59:59 PM GMT for a root whose
SHA256 hash is
```A441B15FE9A3CF56661190A0B93B9DEC7D04127288CC87250967CF3B52894D11```.
The expected behavior is that any website whose certificate is issued
before the enforcement date/timestamp will function in Chrome, and all
issued after will display an interstitial.

```--test-crs-constraints=A441B15FE9A3CF56661190A0B93B9DEC7D04127288CC87250967CF3B52894D11:sctnotafter=1714521599```

**Illustrative Command (on Windows):**

```"C:\Users\User123\AppData\Local\Google\Chrome SxS\Application\chrome.exe" --test-crs-constraints=A441B15FE9A3CF56661190A0B93B9DEC7D04127288CC87250967CF3B52894D11:sctnotafter=1714521599```

**Illustrative Command (on macOS):**

```"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --test-crs-constraints=A441B15FE9A3CF56661190A0B93B9DEC7D04127288CC87250967CF3B52894D11:sctnotafter=1714521599```

**Notes:**
- If copy and pasting the above commands, ensure no line-breaks are
introduced.
- SHA256 hashes of the CA certificates included in the Chrome Root Store
are available [here](https://chromium.googlesource.com/chromium/src/+/main/net/data/ssl/chrome_root_store/root_store.md).

Learn more about command-line flags
[here](https://developer.chrome.com/docs/web-platform/chrome-flags#command-line_flags).

### What criteria does the Chrome Certificate Verifier use to evaluate certificates?
The Chrome Certificate Verifier applies standard processing to include
checking:
- the certificate's key usage and extended key usage are consistent with
TLS use cases.
- the certificate validity period is not in the past or future.
- key sizes and algorithms are of known and acceptable quality.
- whether mismatched or unknown signature algorithms are included.
- that the certificate does not chain to or through a blocked CA.
- conformance with [RFC 5280](https://datatracker.ietf.org/doc/html/rfc5280).

Chrome applies additional processing rules for certificates chaining to
roots included in the Chrome Root Store, such as:
- Certificate Transparency enforcement, and
- maximum certificate validity enforcement as required by the CA/B Forum
Baseline Requirements (i.e., 398 days or less).

### What criteria does the Chrome Certificate Verifier use to build certificate paths?
The Chrome Certificate Verifier was designed to follow path-building
guidance established in [RFC 4158](https://datatracker.ietf.org/doc/html/rfc4158).

### What's the story with certificate revocation?
Certificate revocation is the process by which a CA invalidates the use of
a specific certificate. This is a security function that contains risk.

There are many reasons why a certificate might be revoked, including, but
not limited to:
- Its private key was compromised or unintentionally leaked,
allowing impersonation,
- An error was made during the domain control validation
process, meaning the certificate cannot be relied upon, and
- The certificate was issued in a way that violates the CA Owner’s stated
practices which ecosystem stakeholders may use to make risk-based
decisions.

Common approaches for generating or consuming revocation information
include [Certificate Revocation Lists](https://datatracker.ietf.org/doc/html/rfc5280#section-3.3),
[Online Certificate Status Protocol](https://datatracker.ietf.org/doc/html/rfc6960),
and out-of-band solutions, like
[CRLSet](https://www.chromium.org/Home/chromium-security/crlsets/).

For over a [decade](https://www.imperialviolet.org/2011/03/18/revocation.html),
[challenges](https://www.imperialviolet.org/2014/04/29/revocationagain.html)
[related](https://www.fastly.com/blog/addressing-challenges-tls-revocation-and-ocsp)
to [revocation](https://scotthelme.co.uk/revocation-is-broken/) at the
[scale](https://letsencrypt.org/2022/09/07/new-life-for-crls) of the
Internet have been discussed. The reality is that all existing revocation
solutions come with inherent problems and gaps in intended protection.

For example:
- Timeouts (e.g., an OCSP responder is online but does not respond
within an acceptable time limit).
- Local-network attackers can block "online" lookups.
- OCSP leaks browsing history to third-parties.
- CRLs can be quite large (some "in the wild" have been observed being
close to 100MB) leading to performance costs and usability issues.

All of these problems are improved, if not outright solved, by a world
with exclusively short-lived certificates. Chrome has long-advocated the
use of short-lived certificates, including by leading ballots within the
CA/Browser Forum (e.g., [SC-063](https://cabforum.org/2023/07/14/ballot-sc063v4-make-ocsp-optional-require-crls-and-incentivize-automation/)),
and has used the [Chrome Root Program Policy](https://googlechrome.github.io/chromerootprogram/)
to further promote agility across the ecosystem.

Chrome has also focused on initiatives that reduce the likelihood of
events that necessitate revocation (e.g.,
[Multi-Perspective Issuance Corroboration](https://cabforum.org/2024/08/05/ballot-sc067v3-require-domain-validation-and-caa-checks-to-be-performed-from-multiple-network-perspectives-corroboration/))
and the removal of insecure domain control validation methods (e.g.,
[SC-080](https://cabforum.org/2024/11/14/ballot-sc080v3-sunset-the-use-of-whois-to-identify-domain-contacts-and-relying-dcv-methods/)).
These changes systemically reduce the chance of certificate misissuance and
shrink the window of risk, which are the very problems that make robust
revocation so critical in the first place.

Chrome will continue to champion these systemic improvements to the
ecosystem to reduce the likelihood of a secure connection relying upon a
certificate that’s been improperly issued or invalidated.

### What is Chrome's current revocation checking behavior?
Today, by default, CRLSet blocks the use of all...
 - revoked CA certificates trusted in Chrome
 - server authentication certificates issued by CAs trusted in Chrome
whose revocation reason code is "keyCompromise" or "privilegeWithdrawn."
Additional reason codes may be added in the future.

The following enterprise policies can be used to change the default
revocation checking behavior in Chrome:
- [EnableOnlineRevocationChecks](https://chromeenterprise.google/policies/#EnableOnlineRevocationChecks)
- [RequireOnlineRevocationChecksForLocalAnchors](https://chromeenterprise.google/policies/#RequireOnlineRevocationChecksForLocalAnchors)

### Where is the Chrome Root Store source code located?
Source locations include
[//net/data/ssl/chrome_root_store](/net/data/ssl/chrome_root_store),
[//net/cert](/net/cert), [//services/cert_verifier](/services/cert_verifier),
and [//chrome/browser/component_updater/](/chrome/browser/component_updater/).

### Where is the Chrome Certificate Verifier source code located?
Source locations include [//net/cert](/net/cert),
[//net/cert/internal](/net/cert/internal), and [//third_party/boringssl/src/pki/](https://boringssl.googlesource.com/boringssl/+/refs/heads/main/pki/).