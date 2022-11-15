# Frequently Asked Questions
Last updated: November 11, 2022

- [What is the Chrome Root Store?](#what-is-the-chrome-root-store)
- [Where is the Chrome Root Store source code located?](#where-is-the-chrome-root-store-source-code-located)
- [What is the Chrome Certificate Verifier?](#what-is-the-chrome-certificate-verifier)
- [Where is Chrome Certificate Verifier source code located?](#where-is-chrome-certificate-verifier-source-code)
- [How do these new features impact me?](#how-do-these-new-features-impact-me)
- [When are these changes taking place?](#when-are-these-changes-taking-place)
- [Given the rollout is gradual, how can I tell if these features are in use on my system?](#given-the-rollout-is-gradual_how-can-i-tell-if-these-features-are-in-use-on-my-system)
- [How can I tell which certificates are trusted by the Chrome Root Store?](#how-can-i-tell-which-certificates-are-trusted-by-the-chrome-root-store)
- [Can you help? I’m experiencing problems.](#can-you-help_i_m-experiencing-problems)
- [Can I revert to the platform root store and verifier?](#can-i-revert-to-the-platform-root-store-and-verifier)
- [What criteria does the Chrome Certificate Verifier use to evaluate certificates?](#what-criteria-does-the-chrome-certificate-verifier-use-to-evaluate-certificates)
- [What criteria does the Chrome Certificate Verifier use to build certificate paths?](#what-criteria-does-the-chrome-certificate-verifier-use-to-build-certificate-paths)
- [How is the Chrome Root Store updated?](#how-is-the-chrome-root-store-updated)
- [Why does the certificate viewer look different?](#why-does-the-certificate-viewer-look-different)

## What is the Chrome Root Store?
Chrome uses
[digital certificates](https://en.wikipedia.org/wiki/Public_key_certificate)
(often referred to as “certificates,” “HTTPS certificates,” or “server
authentication certificates”) to ensure the connections it makes on behalf of
its users are secure and private. Certificates are responsible for binding a
domain name to a public key, which Chrome uses to encrypt data sent to and from
the corresponding website.

As part of establishing a secure connection to a website, Chrome verifies that a
recognized system known as a “Certification Authority” (CA) issued its
certificate. Certificates issued by a CA not recognized by Chrome or a user’s
local settings can cause users to see warnings and error pages.

Root stores, sometimes called “trust stores,” tell operating systems and
applications what certificates to trust. The
[Chrome Root Store](https://g.co/chrome/root-store) contains the set of
certificates Chrome trusts by default.

In Chrome 105, we began rolling out the Chrome Root Store to apply a common
certificate verification process on Windows and macOS. The rollout of the
Chrome Root Store on Android, Chrome OS, and Linux  will be announced at a later
date. Apple policies prevent the Chrome Root Store and corresponding Chrome
Certificate Verifier from being used on Chrome for iOS.

## Where is the Chrome Root Store source code located?
Source locations include
[//net/data/ssl/chrome_root_store](/net/data/ssl/chrome_root_store),
[//net/cert](/net/cert), [//services/cert_verifier](/services/cert_verifier),
and [//chrome/browser/component_updater/](/chrome/browser/component_updater/).

## What is the Chrome Certificate Verifier?
Historically, Chrome integrated certificate verification processes with the
platform on which it was running. This resulted in inconsistent user experiences
across platforms, while also making it difficult for developers to understand
Chrome's expected behavior.

In Chrome 105, we began rolling out the Chrome Certificate Verifier to apply a
common certificate verification process on Windows and macOS. The rollout of the
Chrome Certificate Verifier on Android will be announced at a later date. The 
Chrome Certificate Verifier launched on Chrome OS in Chrome 77 and Linux in
Chrome 79. Apple policies prevent the Chrome Certificate Verifier and
corresponding Chrome Root Store from being used on Chrome for iOS.

Once complete, the launch of the Chrome Certificate Verifier will ensure users
have a consistent experience across platforms, that developers have a consistent
understanding of Chrome's behavior, and that Chrome better protects the security
and privacy of users' connections to websites.

## Where is Chrome Certificate Verifier source code?
Source locations include
[//net/cert](/net/cert), [//net/cert/internal](/net/cert/internal), and
[//net/cert/pki](/net/cert/pki).

## How do these new features impact me?

### Chrome Users
We expect the transition to the Chrome Root Store and Chrome Certificate
Verifier to be seamless for most users.

As the transition occurs, a small population of users may notice that a small
number of websites that successfully loaded in earlier versions of Chrome now
present a “Your connection is not private” warning. In instances where the
website’s certificate does not validate to a certificate included in the Chrome
Root Store or a user’s local settings, users will see detailed error language
that includes “ERR_CERT_AUTHORITY_INVALID.”

See troubleshooting steps [here](#can-you-help_i_m-experiencing-problems).

### Website Operators
We expect the transition to the Chrome Root Store and Chrome Certificate
Verifier to be seamless for most website operators.

We encourage website operators to configure HTTPS for their site(s) with
certificates that follow modern best practices, including those found in the
CA/Browser Forum
[Baseline Requirements for the Issuance and Management of Publicly-Trusted Certificates](https://cabforum.org/baseline-requirements-documents/)
and the Chrome Root Program [policy](https://g.co/chrome/root-policy).

If your website’s certificate issuer is not included in the
[Chrome Root Store](https://chromium.googlesource.com/chromium/src/+/main/net/data/ssl/chrome_root_store/root_store.md),
consider transitioning to another service provider to avoid compatibility
issues.

### Enterprise CA Owners
We expect the transition to the Chrome Root Store and Chrome Certificate
Verifier to be seamless for Enterprise CA owners.

Enterprise CAs are intended for use cases exclusively internal to an
organization (e.g., a TLS certificate issued to a corporate intranet site).

The Chrome Certificate Verifier considers locally-managed certificates during
the certificate verification process. Consequently, if an enterprise distributes
a root CA certificate as trusted to its users (for example, by a Windows Group
Policy Object), it will be considered trusted in Chrome.

### Enterprise System Administrators
The Chrome Certificate Verifier considers locally-managed certificates during
the certificate verification process. Consequently, if an enterprise distributes
a root CA certificate as trusted to its users (for example, by a Windows Group
Policy Object), it will be considered trusted in Chrome.

The Chrome Certificate Verifier evaluates certificate profile conformance
against [RFC 5280](https://datatracker.ietf.org/doc/html/rfc5280), and in some
cases, is more strict than platform verifiers. As a result, an enterprise policy
will *temporarily* be available to re-enable the platform root store and
certificate verifier to provide enterprises time to remediate certificate
profile conformance errors. See more
[below](#can-i-revert-to-the-platform-root-store-and-verifier).

### “Publicly-Trusted” CA Owners
CA Owners who meet the Chrome Root Program
[policy](https://g.co/chrome/root-policy) requirements may apply for a CA
certificate’s inclusion in the Chrome Root Store. CAs included in the Chrome
Root Store are expected to adhere to the Chrome Root Program policy and continue
to operate in a consistent and trustworthy manner. A CA owner’s failure to
follow the requirements defined in the Chrome Root Program policy may result in
a CA certificate’s removal from the Chrome Root Store, limitations on Chrome's
acceptance of the certificates they issue, or other technical or policy
restrictions.

## When are these changes taking place?
A “rollout” is a gradual launch of a new feature. Sometimes, to ensure it goes
smoothly, we don’t enable a new feature for all of our users at once. Instead,
we start with a small percentage of users and increase that percentage over time
to ensure we minimize unanticipated compatibility issues. The Chrome Root Store
and Certificate Verifier began rolling out on Windows and macOS in Chrome
105, with other platforms to follow.

## Given the rollout is gradual, how can I tell if these features are in use on my system?

**If on Windows:** Navigate to https://rootcertificateprograms.edicom.es/ …
- **Expected outcome with Chrome Root Store enabled:** Page does not load
(NET::ERR_CERT_AUTHORITY_INVALID)
- **Expected outcome with Chrome Root Store disabled:** Page loads

**If on macOS:** Navigate to https://valid-ctrca.certificates.certum.pl/ …
- **Expected outcome with Chrome Root Store enabled:** Page loads
- **Expected outcome with Chrome Root Store disabled:** Page does not load
(NET::ERR_CERT_AUTHORITY_INVALID)

## How can I tell which certificates are trusted by the Chrome Root Store?
The current contents of the Chrome Root Store is available
[here](https://chromium.googlesource.com/chromium/src/+/main/net/data/ssl/chrome_root_store/root_store.md).

The Chrome Root Store is updated by Component Updater. To observe the contents
of the Chrome Root Store in use by your version of Chrome M105.0.5122.0 or
higher:

1. Navigate to `chrome://system`
2. Click the `Expand`... button next to `chrome_root_store`
3. *The contents of the Chrome Root Store will display*

## Can you help? I’m experiencing problems.
As the transition to the Chrome Root Store and Certificate Verifier occurs, a
small population of users may notice that a small number of websites that
successfully loaded in earlier versions of Chrome now present a “Your connection
is not private” warning that includes a message that reads
“NET::ERR_CERT_AUTHORITY_INVALID”.

**Troubleshooting (for developers, system administrators, or "power users"):**
1. [Verify](#given-the-rollout-is-gradual_how-can-i-tell-if-these-features-are-in-use-on-my-system)
the Chrome Root Store and Certificate Verifier are in use.
     - If the Chrome Root Store and Certificate Verifier are not enabled, read
     more about common connection errors
     [here](https://support.google.com/chrome/answer/6098869?hl=en).
2. Choose to *either* add the website’s corresponding root CA certificate to
your platform root store *or* temporarily use a Chrome Enterprise Policy
to disable the use of the Chrome Root Store and Certificate Verifier.

    * **Add a CA certificate to the platform root store:** Refer to your
    operating system instructions for managing certificates. <br><br>*Warning*:
    You should **never** install a root certificate without careful
    consideration to the impact this might have on your privacy and security.
    *Only* install a root certificate after obtaining it from a trusted source
    and verifying its authenticity (e.g., verifying its SHA-256 thumbprint).

    * **Use the Chrome Enterprise Policy:** See
    [below](#can-you-help_i_m-experiencing-problems).

If you believe the Chrome Certificate Verifier is not working as intended,
submit a [bug](https://bugs.chromium.org/p/chromium/issues/entry).

## Can I revert to the platform root store and verifier?
The Chrome Certificate Verifier evaluates certificate profile conformance
against [RFC 5280](https://datatracker.ietf.org/doc/html/rfc5280), and in some
cases, is more strict than platform verifiers. The
[ChromeRootStoreEnabled](https://chromeenterprise.google/policies/?policy=ChromeRootStoreEnabled)
enterprise policy will be temporarily available to revert to the platform root
store and verifier.

This enterprise policy is planned to be removed from Windows and macOS beginning
in Chrome 113, and should only be used as a temporary solution while
troubleshooting and remediating instances of certificate profile conformance
issues.

## What criteria does the Chrome Certificate Verifier use to evaluate certificates?
The Chrome Certificate Verifier will apply standard processing to include
checking:
- the certificate's key usage and extended key usage are consistent with TLS
use-cases.
- the certificate validity period is not in the past or future.
- key sizes and algorithms are of known and acceptable quality.
- whether mismatched or unknown signature algorithms are included.
- that the certificate does not chain to or through a blocked CA.
- conformance with [RFC 5280](https://datatracker.ietf.org/doc/html/rfc5280).

Chrome applies additional processing rules for certificates chaining to roots
included in the Chrome Root Store, such as:
- Certificate Transparency enforcement, and
- maximum certificate validity enforcement as required by the CA/B Forum
Baseline Requirements (i.e., 398 days or less).

## What criteria does the Chrome Certificate Verifier use to build certificate paths?
The Chrome Certificate Verifier was designed to follow path-building guidance
established in [RFC 4158](https://datatracker.ietf.org/doc/html/rfc4158).

## How is the Chrome Root Store updated?
Chrome uses a "[component updater](https://chromium.googlesource.com/chromium/src/+/lkgr/components/component_updater/README.md)"
tool to push specific updates to browser components without requiring an update
to the Chrome browser application itself. As root CA certificates are added or
removed from the Chrome Root Store, the component updater will be responsible
for automatically propagating these changes to user end-points with no need for
user action.

If your enterprise has [disabled](https://chromeenterprise.google/policies/?policy=ComponentUpdatesEnabled)
component updates, end-points will only receive updated versions of the Chrome
Root Store during Chrome browser application updates.

## Why does the certificate viewer look different?
In Chrome 105, Chrome on Windows and macOS transitioned from using the
native platform certificate viewer to the Chrome Certificate Viewer. This
transition promotes a consistent experience across platforms as we begin the
[rollout](#when-are-these-changes-taking-place) of the Chrome Root Store.