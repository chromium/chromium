# Frequently Asked Questions
Last updated: July 7, 2023

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
applications what certificates to trust. The
[Chrome Root Store](https://g.co/chrome/root-store) contains the set of
certificates Chrome trusts by default.

### What is the Chrome Certificate Verifier?
Historically, Chrome integrated certificate verification processes with
the platform it ran on. This resulted in inconsistent user experiences
across platforms, making it difficult for developers to understand
Chrome's expected behavior.

The Chrome Certificate Verifier addresses these concerns by applying a
common certificate verification process across Windows, macOS, Chrome OS,
Linux, and Android. Apple policies prevent the Chrome Certificate Verifier
and corresponding Chrome Root Store from being used on Chrome for iOS.

### How do these features impact me?

#### Chrome Users
We expect the transition to the Chrome Root Store and Chrome Certificate
Verifier to be seamless for most users.

As the transition occurs, a small population of users may notice that a
small number of websites that successfully loaded in earlier versions of
Chrome now present a “Your connection is not private” warning. When a
website’s certificate does not validate to a certificate included in the
Chrome Root Store or a user’s local settings, users will see detailed
error language that includes “ERR_CERT_AUTHORITY_INVALID.”

See the troubleshooting steps [here](#can-you-help_i_m-experiencing-problems).

#### Website Operators
We expect the transition to the Chrome Root Store and Chrome Certificate
Verifier to be seamless for most website operators.

We encourage website operators to configure HTTPS for their site(s) with
certificates that follow modern best practices, including those found in
the CA/Browser Forum [Baseline Requirements for the Issuance and Management of Publicly-Trusted Certificates](https://cabforum.org/baseline-requirements-documents/)
and the Chrome Root Program [policy](https://g.co/chrome/root-policy).

If your website’s certificate issuer is not included in the
[Chrome Root Store](https://chromium.googlesource.com/chromium/src/+/main/net/data/ssl/chrome_root_store/root_store.md),
consider transitioning to another service provider to avoid compatibility
issues.

#### Enterprise CA Owners
We expect the transition to the Chrome Root Store and Chrome Certificate
Verifier to be seamless for Enterprise CA owners.

Enterprise CAs are intended for use cases exclusively internal to an
organization (e.g., a TLS certificate issued to a corporate intranet site).

The Chrome Certificate Verifier [considers](#will-the-chrome-certificate-verifier-consider-local-trust-decisions)
locally-managed certificates during the certificate verification process.
Consequently, if an enterprise distributes a root CA certificate as
trusted to its users (for example, by a Windows Group Policy Object),
it will be considered trusted in Chrome.

#### Enterprise System Administrators
The Chrome Certificate Verifier [considers](#will-the-chrome-certificate-verifier-consider-local-trust-decisions)
locally-managed certificates during the certificate verification process.
Consequently, if an enterprise distributes a root CA certificate as
trusted to its users (for example, by a Windows Group Policy Object),
it will be considered trusted in Chrome.

The Chrome Certificate Verifier evaluates certificate profile conformance
against [RFC 5280](https://datatracker.ietf.org/doc/html/rfc5280), and in
some cases, is more strict than platform verifiers. As a result, an
enterprise policy will *temporarily* be available to re-enable the
platform root store and certificate verifier to provide enterprises time
to remediate certificate profile conformance errors. See more
[below](#can-i-revert-to-the-platform-root-store-and-verifier).

#### “Publicly-Trusted” CA Owners
CA Owners who meet the Chrome Root Program
[policy](https://g.co/chrome/root-policy) requirements may apply for a CA
certificate’s inclusion in the Chrome Root Store. CAs included in the
Chrome Root Store are expected to adhere to the Chrome Root Program policy
and continue to operate in a consistent and trustworthy manner. A CA
owner’s failure to follow the requirements defined in the Chrome Root
Program policy may result in a CA certificate’s removal from the Chrome
Root Store, limitations on Chrome's acceptance of the certificates they
issue, or other technical or policy restrictions.

### When are these changes taking place?
A “rollout” is a gradual launch of a new feature. Sometimes, to ensure it
goes smoothly, we don’t enable a new feature for all of our users at once.
Instead, we start with a small percentage of users and increase that
percentage over time to ensure we minimize unanticipated compatibility
issues.

The table below shows the rollout of these new features across platforms.

| Chrome on...\*  | Chrome Root Store Rollout Begins\*\* | Chrome Root Store Enabled by Default | Sunset of Enterprise Policy\*\*\* |
| --------------- | ------------------------------------ | ------------------------------------ | -------------------------------------  |
| Android         | Chrome 114                           | [Rollout in progress]                | TBD                                    |
| Chrome OS       | Chrome 114                           | Chrome 114                           | Chrome 119                             |                                                                                                                                                                                                                                                         |
| iOS\*\*\*\*     | N/A                                  | N/A                                  | N/A                                    |
| Linux           | Chrome 114                           | Chrome 114                           | Chrome 119                             |
| macOS           | Chrome 105                           | Chrome 108                           | Chrome 112                             |
| Windows         | Chrome 105                           | Chrome 108                           | Chrome 112                             |

**Notes:**<br>
(\*) Find Chrome browser system requirements [here.](https://support.google.com/chrome/a/answer/7100626)

(\*\*) During this release, users also had the opportunity to "opt-in" to
these features, regardless of whether they were automatically enrolled in
the rollout population.

(\*\*\*) The [ChromeRootStoreEnabled](https://chromeenterprise.google/policies/?policy=ChromeRootStoreEnabled)
enterprise policy will be temporarily available to revert to the platform
root store and verifier. The version represented in this column is the
last version of Chrome where the corresponding platform will support the
enterprise policy.

(\*\*\*\*) Apple policies prevent the Chrome Root Store and Chrome Certificate
Verifier from being used on Chrome for iOS.


## Support and Troubleshooting

### Can you help? I’m experiencing problems.
As the transition to the Chrome Root Store and Certificate Verifier occurs,
a small population of users may notice that a small number of websites
that successfully loaded in earlier versions of Chrome now present a “Your
connection is not private” warning that includes a message that reads
“NET::ERR_CERT_AUTHORITY_INVALID.”

**Troubleshooting (for developers, system administrators, or "power users"):**
1. [Verify](#given-the-rollout-is-gradual_how-can-i-tell-if-these-features-are-in-use-on-my-system)
the Chrome Root Store and Certificate Verifier are in use.
     - If the Chrome Root Store and Certificate Verifier are not enabled,
     read more about common connection errors
     [here](https://support.google.com/chrome/answer/6098869?hl=en).
2. Choose to *either* add the website’s corresponding root CA certificate
to your platform root store *or* temporarily use a Chrome Enterprise
Policy to disable the Chrome Root Store and Certificate Verifier.

    * **Add a CA certificate to the platform root store:** Refer to your
    operating system instructions for managing certificates. <br><br>
    *Warning*: You should **never** install a root certificate without
    carefully considering the impact this might have on your privacy and
    security. *Only* install a root certificate after obtaining it from
    a trusted source and verifying its authenticity (e.g., verifying its
    SHA-256 thumbprint).

    * **Use the Chrome Enterprise Policy:** See
    [below](#can-you-help_i_m-experiencing-problems).

If you believe the Chrome Certificate Verifier is not working as intended,
submit a [bug](https://bugs.chromium.org/p/chromium/issues/entry) and
attach a [NetLog dump](https://www.chromium.org/for-testers/providing-network-details/)
repeating the steps leading to the issue from a new Incognito window. Add
a comment to route the bug to the Internals>Network>Certificate component
for the fastest routing to the appropriate triage team.

If you believe you've identified a Security Bug, follow [these](https://www.chromium.org/Home/chromium-security/reporting-security-bugs/)
instructions.

### Can I revert to the platform root store and verifier?
The Chrome Certificate Verifier evaluates certificate profile conformance
against [RFC 5280](https://datatracker.ietf.org/doc/html/rfc5280), and in
some cases, is more strict than platform verifiers. The
[ChromeRootStoreEnabled](https://chromeenterprise.google/policies/?policy=ChromeRootStoreEnabled)
enterprise policy will be temporarily available to revert to the platform
root store and verifier.

This enterprise policy will be available for a limited time only and
should only be used as a temporary solution while troubleshooting and
remediating instances of certificate profile conformance issues.


## Additional Information for Administrators, Engineers, and Power Users

### How is the Chrome Root Store updated?
Chrome uses a "[component updater](https://chromium.googlesource.com/chromium/src/+/lkgr/components/component_updater/README.md)"
tool to push specific updates to browser components without updating the
Chrome browser application. As root CA certificates are added or removed
from the Chrome Root Store, the component updater will automatically
propagate these changes to user endpoints without user action.

If your enterprise has [disabled](https://chromeenterprise.google/policies/?policy=ComponentUpdatesEnabled)
component updates, endpoints will only receive updated versions of the
Chrome Root Store during Chrome browser application updates.

During routine operating conditions, the Chrome Root Store is updated
approximately quarterly. However, aperiodic updates may take place to
promote the safety and privacy of Chrome's users.

### Will the Chrome Certificate Verifier consider local trust decisions?

On **Windows**, the Chrome Certificate Verifier will automatically consume
certificates added to the following certificate stores:

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

On **macOS**, the Chrome Certificate Verifier will automatically consume
certificates added to the following certificate stores:

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

### What about client authentication certificates?
Historically, Chrome has integrated with platform certificate stores to
support the use of client authentication certificates. This behavior is
unchanged by the rollout of the Chrome Root Store and the Chrome
Certificate Verifier.

### Given the gradual rollout, how can I tell if these features are in use on my system?

See these [testing instructions](testing.md).

### How can I tell which certificates are trusted by the Chrome Root Store?
The most recent version of the Chrome Root Store is available
[here](https://chromium.googlesource.com/chromium/src/+/main/net/data/ssl/chrome_root_store/root_store.md).

The Chrome Root Store is updated by Component Updater. To observe the
contents of the Chrome Root Store in use by a version of Chrome where it
has been [launched](#when-are-these-changes-taking-place):

1. Navigate to `chrome://system`
2. Click the `Expand`... button next to `chrome_root_store`
3. *The contents of the Chrome Root Store will display*

### What criteria does the Chrome Certificate Verifier use to evaluate certificates?
The Chrome Certificate Verifier will apply standard processing to include
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