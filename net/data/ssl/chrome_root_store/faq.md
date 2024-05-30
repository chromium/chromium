# Frequently Asked Questions
Last updated: June 1, 2024

[TOC]

## General Questions

### What is the Chrome Root Store?
Chrome uses
[digital certificates](https://en.wikipedia.org/wiki/Public_key_certificate)
(often referred to as “certificates,” “HTTPS certificates,” or “server
authentication certificates”) to ensure the connections it makes on behalf
of its users are secure and private. Certificates bind a domain name to a
public key, which Chrome uses to encrypt data sent to and from the
corresponding website.

As part of establishing a secure connection to a website, Chrome verifies
that a recognized system known as a “Certification Authority” (CA) issued
its certificate. Certificates issued by a CA not recognized by Chrome or a
user’s local settings can cause users to see warnings and error pages.

Root stores, sometimes called “trust stores,” tell operating systems and
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
The Chrome Certificate Verifier
[considers](#does-the-chrome-certificate-verifier-consider-local-trust-decisions)
locally-managed certificates during the certificate verification process.
Consequently, if an enterprise distributes a root CA certificate as
trusted to its users (for example, by a Windows Group Policy Object), it
will be considered trusted in Chrome.

### How can I apply for my CA's inclusion in the Chrome Root Store?
CA Owners who meet the Chrome Root Program
[policy](https://g.co/chrome/root-policy) requirements may apply for a CA
certificate’s inclusion in the Chrome Root Store. CAs included in the
Chrome Root Store are expected to adhere to the Chrome Root Program policy
and continue to operate in a consistent and trustworthy manner. A CA
owner’s failure to follow the requirements defined in the Chrome Root
Program policy may result in a CA certificate’s removal from the Chrome
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

### Can I revert to the platform root store and verifier?
An
[enterprise policy](https://chromeenterprise.google/policies/?policy=ChromeRootStoreEnabled)
was available for a limited time in support of troubleshooting during the
transition to the Chrome Root Store and Chrome Certificate Verifier.

This enterprise policy is now deprecated.

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

### Does the Chrome Certificate Verifier consider local trust decisions?

Yes.

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

### What about client authentication certificates?
Historically, Chrome has integrated with platform certificate stores to
support the use of client authentication certificates. This behavior is
unchanged by the rollout of the Chrome Root Store and the Chrome
Certificate Verifier.

### How can I tell which certificates are trusted by the Chrome Root Store?
The most recent version of the Chrome Root Store is available
[here](https://chromium.googlesource.com/chromium/src/+/main/net/data/ssl/chrome_root_store/root_store.md).

The Chrome Root Store is updated by Component Updater. To observe the
contents of the Chrome Root Store in use by a version of Chrome:

1. Navigate to ```chrome://system```
2. Click the ```Expand...``` button next to `chrome_root_store`
3. *The contents of the Chrome Root Store will display*

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

### Where is the Chrome Root Store source code located?
Source locations include
[//net/data/ssl/chrome_root_store](/net/data/ssl/chrome_root_store),
[//net/cert](/net/cert), [//services/cert_verifier](/services/cert_verifier),
and [//chrome/browser/component_updater/](/chrome/browser/component_updater/).

### Where is the Chrome Certificate Verifier source code located?
Source locations include
[//net/cert](/net/cert), [//net/cert/internal](/net/cert/internal), and
[//net/cert/pki](/net/cert/pki).