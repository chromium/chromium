# Certificate Lifetimes

As part of our ongoing commitment to ensuring users’ security, Google is
reducing the maximum allowed lifetimes of TLS certificates.

## Upcoming Changes

Beginning with Chrome 85, TLS server certificates issued on or after
2020-09-01 00:00:00 UTC will be required to have a validity period of 398 days
or less. This will only apply to TLS server certificates from CAs that are
trusted in a default installation of Google Chrome, commonly known as
"publicly trusted CAs", and will not apply to locally-operated CAs that have
been manually configured.

Certificates that do not comply with this requirement will not work, and may
cause webpages to fail to load or to render incorrectly.

If a certificate that does not comply with this requirement is issued by a CA
trusted in a default installation of Google Chrome, this will be treated as a
failure to comply with the security policies necessary to being a trusted CA,
and may result in the removal of trust of that CA’s certificates.

## Technical Details

* A certificate will be impacted by this restriction if either the notBefore
  of the certificate is on or after 2020-09-01 00:00:00 UTC, or if the first
  precertificate logged by the CA to a Certificate Transparency Log that is
  qualified at time of issuance is on or after this date.
* The validity period of a certificate is defined within RFC 5280, Section
  4.1.2.5, as "the period of time from notBefore through notAfter, inclusive."
* 398 days is measured with a day being equal to 86,400 seconds. Any time
  greater than this indicates an additional day of validity.
* To avoid the risk of misissuance, such as due to leap seconds or
  CA-configured randomization, CAs SHOULD issue such server certificates with
  validity periods of 397 days or less.

## Frequently Asked Questions

* Why is Chrome making this change?
  * Shortening certificate lifetimes protects users by reducing the impact
    of compromised keys, and by speeding up the replacement of insecure
    technologies and practices across the web. Key compromises and the
    discovery of internet security weaknesses are common events that can lead
    to real-world harm, and the web’s users should be better protected against
    them.
* Does this apply to locally-operated CAs, such as those used within
  enterprises that use enterprise-configured configured CAs?
  * No. This only applies to the set of CAs that are trusted by default by
    Google Chrome, and not CAs that are operated by an enterprise and that
    have no certification paths to CAs that are trusted by default.
* Is there an enterprise policy to disable this enforcement?
  * No. These changes are transparent and do not offer an enterprise control
    to override, as they only apply to so-called "publicly trusted" CAs.
    Enterprises that wish to have certificates with validity periods longer
    than 398 days may do so by using a locally-operated CA that does not have
    any certification paths up to a publicly trusted CA.
* Does this mean I have to replace my existing certificates?
  * No. This requirement only applies to new certificate issuance on or after
    2020-09-01 00:00:00 UTC. Existing certificates whose validity period
    exceeds 398 days will continue to work, while new certificates must comply
    with these new requirements, such as when they are renewed or replaced.
* Will this make certificates more expensive?
  * As with past changes to the maximum certificate lifetimes, many CAs have
    committed to providing additional certificates, as needed by the shortened
    maximum lifetime, at no additional cost.
* What will happen if a certificate is issued that does not meet these
  requirements?
  * Google Chrome will reject such certificates as having too long a validity
    period, consistent with existing validity-period based enforcement.
    Additionally, such certificates will be treated as a critical security
    failure by the CA, and may result in further action taken on the CA that
    may affect how current or future certificates from that CA function.
    Chromium-based browsers will have this enforcement enabled by default, and
    will need to modify the source to disable this.
* What are other browsers doing?
  * Apple previously announced this change for versions of iOS, iPadOS, macOS,
    tvOS, and watchOS, as documented at
    https://support.apple.com/en-us/HT211025, which will apply to all
    applications, and not just those of Safari. This certificate lifetime
    requirement is fully interoperable with Apple’s requirements.

    Microsoft, Mozilla, Opera, and 360 have previously indicated their support
    for these requirements, although have not yet made announcements at the
    time of this post (2020-06-22). Other browsers, including those browsers
    based on Chromium, may provide additional guidance or clarification.
