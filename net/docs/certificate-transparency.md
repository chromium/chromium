# Certificate Transparency

[TOC]

## Overview

Certificate Transparency (CT) is a protocol designed to fix several structural
flaws in the SSL/TLS certificate ecosystem. Originally described in
[RFC 6962](https://tools.ietf.org/html/rfc6962), it provides a public,
append-only data structure that can log certificates that are issued by
[certificate authorities](https://en.wikipedia.org/wiki/Certificate_authority) (CAs).
By logging certificates, it becomes possible for the public to see what
certificates have been issued by a given CA. This allows site operators to
detect when a certificate has been issued for their domains, allowing them to
check for unauthorized issuance. It also allows browsers and root stores, and
the broader community, to examine the certificates a CA has issued and ensure
that the CA is complying with their expected or disclosed practices.

For more information about how Certificate Transparency works, see:
  * https://www.certificate-transparency.org
  * [Introducing Certificate Transparency and Nimbus](https://blog.cloudflare.com/introducing-certificate-transparency-and-nimbus/)

## Certificate Transparency for Site Operators

### Basics

We say that a certificate supports Certificate Transparency if it comes with
CT information that demonstrates it has been logged in several CT logs. This
CT information must comply with the
[Certificate Transparency in Chrome](https://github.com/chromium/ct-policy/blob/main/ct_policy.md)
policy. We sometimes refer to a site that "supports" CT as using a certificate
that is "CT qualified" or "disclosed via CT."

In general, a site operator does not need to take special action to
support Certificate Transparency. As Certificate Transparency has been used in
several browsers for years, nearly every CA supports CT by embedding CT
information inside the certificate. This means that when you get a certificate,
it will already support CT and require no further configuration. This is the
preferred and recommended way to enable CT support -- if you obtain a
certificate from your CA and it does not support CT, then that generally
indicates that your CA is not following industry best practice, and you should
probably look for another CA to provide certificates for your sites.

Chrome's policies requiring CT only apply to certificates from publicly-trusted
CAs - that is, CAs that your browser or device trust without any additional
configuration. For organizations using their own CAs, or for locally installed
CAs, see
[Certificate Transparency for Enterprises](#Certificate-Transparency-For-Enterprises).

### Chrome Policies

Chrome gradually phased in requirements that all publicly-trusted certificates
are logged to Certificate Transparency in order to be used in Chrome.
If a connection is attempted in Chrome and no CT information is provided,
then the certificate will be rejected as untrusted, and the connection will be
blocked. In the case of a main page load, the user will see a full page
certificate warning page, with the error code
`net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED`. If you receive this error, this
indicates that your CA has not taken steps to make sure your certificate
supports CT, and you should contact your CA's sales or support team to ensure
you can get a replacement certificate that works.

### Domain Privacy

Supporting CT by disclosing the certificate to a CT Log means that the full
contents of the certificate will be publicly accessible and viewable. In
particular, this means that the domains a certificate are for will be included
in the Certificate Transparency log, as well as possibly information regarding
the organization that that those domains are affiliated with.

For most certificates, this is no different than what was already available.
Publicly-trusted certificates have been subject to aggregation for public
analysis for some time, through products and tools such as
[Censys](https://censys.io/) or [scans.io](https://scans.io/). While
Certificate Transparency provides an interoperable protocol for exchanging
these datasets, in many cases, the certificate details and domains were already
publicly detectable.

Requiring that the full certificate be disclosed if it was issued by a
publicly-trusted CA is an important part of the security goals of Certificate
Transparency. Permitting some of the information to be hidden from
certificates allows for both attackers and untrustworthy CAs to hide
certificates that could be used to compromise users. Certificate Transparency
has detected issues at a large
[number of CAs](https://wiki.mozilla.org/CA/Incident_Dashboard), many that the
CAs themselves were not even aware of, and so public disclosure is critical
to keeping all users safe.

While proposals for hiding domain names were presented during the development
of Certificate Transparency, none of them were able to balance the needs of
site operators that did not need to hide their domains, those that did, and the
security risks that users would face.

Because of this, Chrome does not support any method for hiding domain names or
other information within publicly-trusted certificates, nor are there any plans
to support such mechanisms. Domain operators that wish to hide their
certificates, enabling security risks and attacks, have two options:

1. **Wildcard Certificates** - Wildcard certificates allow a single certificate
   to be used for multiple hostnames, by putting a `*` as the most specific
   DNS label (for example, `*.internal.example.com` is valid for
   `mail.internal.example.com` and `wiki.internal.example.com`, but not for
   `www.example.com` or `two.levels.internal.example.com`). Wildcard
   certificates require greater care by the site operator to protect their
   private key, but also can have their issuance controlled via technologies
   such as [CAA (RFC 6844)](https://tools.ietf.org/html/rfc6844). This still
   requires the certificate be disclosed, but can limit how much of the domain
   is disclosed.
2. **Enterprise-specific configuration** - If the domains being accessed are
   not intended to be used on the public internet, or not on machines or by
   users that are not part of a single enterprise, then that enterprise can
   use the options in the
   [Certificate Transparency for Enterprises](#Certificate-Transparency-For-Enterprises).
   This allows the enterprise to not reveal any information about the
   certificate, but these certificates will **only** be trusted by their
   members.

### What to do if your certificate does not work

If you're receiving a `net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED` error
message, the best thing to do is to contact your CA's support or sales team
to diagnose the error with them. They will most likely need to replace your
certificate with a new one that properly supports CT.

## Certificate Transparency for Enterprises

### Locally-trusted CAs

Certificate Transparency only applies to CAs that are publicly-trusted - that
is, CAs that are supported by your browser or device out of the box, without
any additional configuration steps.

For CAs that have been manually installed, provided those certificates are not
or have not been publicly-trusted, it's not necessary to enable support for
Certificate Transparency. Further, Certificate Transparency Logs will not
accept certificates from those CAs, thus it's not possible to support CT.

In some cases, an Enterprise may have a locally-trusted CA that has been
manually installed, but it was previously publicly-trusted. For example, this
CA may have been removed by a browser or an OS for not complying with the
root store policies, but the Enterprise may still have a dependency on
trusting this CA. In these cases, the Enterprise can use
[Enterprise Policies](#Enterprise-Policies) to configure how Certificate
Transparency will be enforced for those CAs.

### Private Domain Names

For Enterprises that have domain names that are internal to their organization,
and do not need to be publicly-trusted by default, several options exist to
enable these domains to be kept private, while allowing the certificates to
still be used, without error, for users in their organization.

The recommended option is to no longer rely on publicly-trusted certificates
to serve these domains, as they are organization specific. For example, such
organizations can use a private CA, which [several](https://aws.amazon.com/certificate-manager/private-certificate-authority/)
[CAs](https://www.digicert.com/private-pki/) [offer](https://www.sectigo.com/enterprise-solutions/certificate-manager/private-pki).
Using a hosted, managed PKI may help organizations more rapidly respond to
change in the TLS ecosystem, such as changes to certificate algorithms or
support for new protocols.

Another option is to request that the publicly-trusted CA not log the
certificate. This will prevent this certificate from being trusted by default,
but organizations that manage their devices or users can override this through
[Enterprise Policies](#Enterprise-Policies) to enable these certificates to be
trusted for users in their Enterprise.

Finally, organizations may manage their own PKI in-house, using CA
software such as [CFSSL](https://github.com/cloudflare/cfssl), [Boulder](https://github.com/letsencrypt/boulder),
[EJBCA](https://www.ejbca.org/) or
[Active Directory Certificate Services](https://learn.microsoft.com/en-us/windows-server/identity/ad-cs/active-directory-certificate-services-overview).
Managing certificates in-house may be more complex and security risky, but
offers an alternative solution to partnering with a certificate provider.

### Enterprise Policies

Several Chrome-specific policies exist that allow Enterprises to configure
their machines or users to disable Certificate Transparency for certain cases.
These policies are documented in the
[Chrome Enterprise policy list](https://chromeenterprise.google/policies/),
but detailed further below.

#### CertificateTransparencyEnforcementDisabledForUrls

This [policy](https://chromeenterprise.google/policies/#CertificateTransparencyEnforcementDisabledForUrls)
has been available since Chrome 53, and allows for disabling Certificate
Transparency enforcement for a certain set of domains or subdomains, without
disabling Certificate Transparency altogether.

If you wish to disable CT for a given hostname, and all of its subdomains, then
the domain is simply entered into the list. For example, `example.com` will
disable CT for `example.com` and all subdomains.

If you wish to disable CT only for a given hostname, but wish to ensure that
subdomains will still have CT enabled, then prefix the domain with a leading
dot. For example, `.example.com` will disable CT for `example.com` exactly,
while leaving it enabled for subdomains.

#### CertificateTransparencyEnforcementDisabledForCas

This [policy](https://chromeenterprise.google/policies/#CertificateTransparencyEnforcementDisabledForCas),
available since Chrome 57, allows for disabling Certificate Transparency
enforcement if certain conditions are met in the trusted certificate chain.
This allows disabling CT without having to list all of the domain names, but
only for certificates issued to a specific organization.

Certificates are specified in this policy by applying Base64 to a hash of their
subjectPublicKeyInformation, as well as specifying the hash algorithm used.
This format is very similar to that used by
[HTTP Public Key Pinning](https://tools.ietf.org/html/rfc7469) (HPKP), so that
sites can use the same [examples](https://tools.ietf.org/html/rfc7469#appendix-A)
or [tools](https://report-uri.com/home/pubkey_hash) used to generate HPKP
hashes to determine how to configure the policy. Note that while both use
Base64, an HPKP hash will be in the form `pin-sha256="hash"`, while the policy
will be in the form `sha256/hash`.

To disable Certificate Transparency for these certificates, the certificate
must match one of the following conditions:

1. The hash specified is of the server certificate's subjectPublicKeyInfo.
2. The hash specified is of an intermediate CA, and that intermediate CA has
   a nameConstraints extension with one or more directoryNames in the
   permittedSubtrees of that extension.
3. The hash specified is of an intermediate CA, that intermediate CA contains
   one or more organizationName (O) attribute in the subject, and the server
   certificate's has the same number of organizationName attributes, with
   byte-for-byte identical values, in the same exact order.

## Certificate Transparency for Chrome/Chromium developers

### //net Interfaces

Support for Certificate Transparency in //net is made up of two core
interfaces:

* [`CTVerifier`](/net/cert/ct_verifier.h): Responsible for extracting the
  CT information (SCTs) from the certificate, the OCSP response, and the
  TLS handshake, validating the signatures against a set of known/configured
  CT logs, and validating that the SCTs match the certificate provided.
* [`CTPolicyEnforcer`](/net/cert/ct_policy_enforcer.h): Responsible for
  taking the extracted, verified SCTs and applying
  application/embedder-specific policies to determine whether the SCTs are
  "good enough" (meet application requirements).

In addition to these two core classes, configuration and support for CT-related
behaviours is expressed via the
[`TransportSecurityState`](/net/http/transport_security_state.h).

### Supporting Certificate Transparency for Embedders

The risks to the CA and CT ecosystem significantly increase if embedders
implement CT without the ability for reliable, rapid updates, keeping track with
ongoing development in the main tree and reliably delivering security updates on
the same cadence as Chromium branches and Google Chrome releases.

As a result, Chromium embedders do **NOT** have CT enforcement enabled by
default, and are **VERY STRONGLY** encouraged to contact
chrome-certificate-transparency@google.com before enabling CT enforcement in
their browsers.

Distributors of products that embed Chromium sources are encouraged to
participate in the
[ct-policy@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/ct-policy)
discussion group, which involves a variety of stakeholders in the CT ecosystem
for discussing matters of policy and implementation, in order to understand
the risks and participate in solutions. Embedders are also welcome to attent
face-to-face summits, announced ahead of time on the discussion group, wherein
key stakeholders gather to work through these issues, helping root programs,
CAs, log operators, and the overall PKI community develop consistent,
interoperable, secure, and reliable policies and implementations.
