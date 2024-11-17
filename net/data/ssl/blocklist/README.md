# Certificate Blocklist

This directory contains a number of certificates and public keys which are
considered blocked within Chromium-based products.

When applicable, additional information and the full certificate or key
are included.

## Adding a New Entry

Entries are recorded in [cert_verify_proc_blocklist.inc](../../../cert/cert_verify_proc_blocklist.inc).
The filename is the SHA-256 hash of the DER-encoded certificate, which can be
obtained via:

    openssl x509 -in path/to/cert.pem -outform DER | openssl dgst -sha256

The entries in the `cert_verify_proc_blocklist.inc` file can be generated via:

    openssl x509 -in path/to/cert.pem -noout -pubkey | openssl pkey -pubin -outform DER | openssl dgst -sha256 -c | awk '{print "0x" $2}' | sed 's/:/, 0x/g'

## Compromises & Misissuances

### .bd

google.com.bd certificates from Comodo.

  * [487afc8d0d411b2a05561a2a6f35918f4040e5570c4c73ee323cc50583bcfbb7.pem](487afc8d0d411b2a05561a2a6f35918f4040e5570c4c73ee323cc50583bcfbb7.pem)

### Camerfirma

For details, see <https://groups.google.com/g/mozilla.dev.security.policy/c/dSeD3dgnpzk/m/iAUwcFioAQAJ>

As a result of a long-standing pattern of misissuances and incomplete or
insufficient remediations, trust in TLS server certificates from Camerfirma
was fully removed.

  * [04f1bec36951bc1454a904ce32890c5da3cde1356b7900f6e62dfa2041ebad51.pem](04f1bec36951bc1454a904ce32890c5da3cde1356b7900f6e62dfa2041ebad51.pem)
  * [063e4afac491dfd332f3089b8542e94617d893d7fe944e10a7937ee29d9693c0.pem](063e4afac491dfd332f3089b8542e94617d893d7fe944e10a7937ee29d9693c0.pem)
  * [0c258a12a5674aef25f28ba7dcfaeceea348e541e6f5cc4ee63b71b361606ac3.pem](0c258a12a5674aef25f28ba7dcfaeceea348e541e6f5cc4ee63b71b361606ac3.pem)
  * [136335439334a7698016a0d324de72284e079d7b5220bb8fbd747816eebebaca.pem](136335439334a7698016a0d324de72284e079d7b5220bb8fbd747816eebebaca.pem)
  * [c1d80ce474a51128b77e794a98aa2d62a0225da3f419e5c7ed73dfbf660e7109.pem](c1d80ce474a51128b77e794a98aa2d62a0225da3f419e5c7ed73dfbf660e7109.pem)
  * [ef3cb417fc8ebf6f97876c9e4ece39de1ea5fe649141d1028b7d11c0b2298ced.pem](ef3cb417fc8ebf6f97876c9e4ece39de1ea5fe649141d1028b7d11c0b2298ced.pem)

### China Internet Network Information Center (CNNIC)

For details, see <https://security.googleblog.com/2015/03/maintaining-digital-certificate-security.html>

As a result of misissuance of a sub-CA certificate, CNNIC end-entity
certificates were temporarily allowlisted, and then trust in the root fully
removed.

  * [1c01c6f4dbb2fefc22558b2bca32563f49844acfc32b7be4b0ff599f9e8c7af7.pem](1c01c6f4dbb2fefc22558b2bca32563f49844acfc32b7be4b0ff599f9e8c7af7.pem)
  * [e28393773da845a679f2080cc7fb44a3b7a1c3792cb7eb7729fdcb6a8d99aea7.pem](e28393773da845a679f2080cc7fb44a3b7a1c3792cb7eb7729fdcb6a8d99aea7.pem)
  * [2740d956b1127b791aa1b3cc644a4dbedba76186a23638b95102351a834ea861.pem](2740d956b1127b791aa1b3cc644a4dbedba76186a23638b95102351a834ea861.pem)

### Comodo

For details, see <https://www.comodo.com/Comodo-Fraud-Incident-2011-03-23.html>,
<https://blog.mozilla.org/security/2011/03/25/comodo-certificate-issue-follow-up/>,
and <https://technet.microsoft.com/en-us/library/security/2524375.aspx>.

As the result of a compromise of a partner RA of Comodo, nine certificates were
misissued, for a variety of online services.

  * [2a3699deca1e9fd099ba45de8489e205977c9f2a5e29d5dd747381eec0744d71.pem](2a3699deca1e9fd099ba45de8489e205977c9f2a5e29d5dd747381eec0744d71.pem)
  * [4bf6bb839b03b72839329b4ea70bb1b2f0d07e014d9d24aa9cc596114702bee3.pem](4bf6bb839b03b72839329b4ea70bb1b2f0d07e014d9d24aa9cc596114702bee3.pem)
  * [79f69a47cfd6c4b4ceae8030d04b49f6171d3b5d6c812f58d040e586f1cb3f14.pem](79f69a47cfd6c4b4ceae8030d04b49f6171d3b5d6c812f58d040e586f1cb3f14.pem)
  * [8290cc3fc1c3aac3239782c141ace8f88aeef4e9576a43d01867cf19d025be66.pem](8290cc3fc1c3aac3239782c141ace8f88aeef4e9576a43d01867cf19d025be66.pem)
  * [933f7d8cda9f0d7c8bfd3c22bf4653f4161fd38ccdcf66b22e95a2f49c2650f8.pem](933f7d8cda9f0d7c8bfd3c22bf4653f4161fd38ccdcf66b22e95a2f49c2650f8.pem)
  * [9532e8b504964331c271f3f5f10070131a08bf8ba438978ce394c34feeae246f.pem](9532e8b504964331c271f3f5f10070131a08bf8ba438978ce394c34feeae246f.pem)
  * [be144b56fb1163c49c9a0e6b5a458df6b29f7e6449985960c178a4744624b7bc.pem](be144b56fb1163c49c9a0e6b5a458df6b29f7e6449985960c178a4744624b7bc.pem)
  * [ead610e6e90b439f2ecb51628b0932620f6ef340bd843fca38d3181b8f4ba197.pem](ead610e6e90b439f2ecb51628b0932620f6ef340bd843fca38d3181b8f4ba197.pem)
  * [f8a5ff189fedbfe34e21103389a68340174439ad12974a4e8d4d784d1f3a0faa.pem](f8a5ff189fedbfe34e21103389a68340174439ad12974a4e8d4d784d1f3a0faa.pem)

### DCSSI

SPKI for an intermediate under the DCSSI root (French government) that was used
to misissue gstatic.com certificates.

  * [e54e9fc27e7350ff63a77764a40267b7e95ae5df3ed7df5336e8f8541356c845.pem](e54e9fc27e7350ff63a77764a40267b7e95ae5df3ed7df5336e8f8541356c845.pem)

### DigiNotar

For details, see <https://googleonlinesecurity.blogspot.com/2011/08/update-on-attempted-man-in-middle.html>
and <https://en.wikipedia.org/wiki/DigiNotar>.

As a result of a complete CA compromise, the following certificates (and
their associated public keypairs) are revoked.

  * [0d136e439f0ab6e97f3a02a540da9f0641aa554e1d66ea51ae2920d51b2f7217.pem](0d136e439f0ab6e97f3a02a540da9f0641aa554e1d66ea51ae2920d51b2f7217.pem)
  * [294f55ef3bd7244c6ff8a68ab797e9186ec27582751a791515e3292e48372d61.pem](294f55ef3bd7244c6ff8a68ab797e9186ec27582751a791515e3292e48372d61.pem)
  * [31c8fd37db9b56e708b03d1f01848b068c6da66f36fb5d82c008c6040fa3e133.pem](31c8fd37db9b56e708b03d1f01848b068c6da66f36fb5d82c008c6040fa3e133.pem)
  * [3946901f46b0071e90d78279e82fababca177231a704be72c5b0e8918566ea66.pem](3946901f46b0071e90d78279e82fababca177231a704be72c5b0e8918566ea66.pem)
  * [450f1b421bb05c8609854884559c323319619e8b06b001ea2dcbb74a23aa3be2.pem](450f1b421bb05c8609854884559c323319619e8b06b001ea2dcbb74a23aa3be2.pem)
  * [4fee0163686ecbd65db968e7494f55d84b25486d438e9de558d629d28cd4d176.pem](4fee0163686ecbd65db968e7494f55d84b25486d438e9de558d629d28cd4d176.pem)
  * [8a1bd21661c60015065212cc98b1abb50dfd14c872a208e66bae890f25c448af.pem](8a1bd21661c60015065212cc98b1abb50dfd14c872a208e66bae890f25c448af.pem)
  * [9ed8f9b0e8e42a1656b8e1dd18f42ba42dc06fe52686173ba2fc70e756f207dc.pem](9ed8f9b0e8e42a1656b8e1dd18f42ba42dc06fe52686173ba2fc70e756f207dc.pem)
  * [a686fee577c88ab664d0787ecdfff035f4806f3de418dc9e4d516324fff02083.pem](a686fee577c88ab664d0787ecdfff035f4806f3de418dc9e4d516324fff02083.pem)
  * [b8686723e415534bc0dbd16326f9486f85b0b0799bf6639334e61daae67f36cd.pem](b8686723e415534bc0dbd16326f9486f85b0b0799bf6639334e61daae67f36cd.pem)
  * [fdedb5bdfcb67411513a61aee5cb5b5d7c52af06028efc996cc1b05b1d6cea2b.pem](fdedb5bdfcb67411513a61aee5cb5b5d7c52af06028efc996cc1b05b1d6cea2b.pem)

### India CCA

For details, see <https://googleonlinesecurity.blogspot.com/2014/07/maintaining-digital-certificate-security.html>
and <https://technet.microsoft.com/en-us/library/security/2982792.aspx>

An unknown number of misissued certificates were issued by a sub-CA of
India CCA, the India NIC. Due to the scope of the misissuance, the sub-CA
was wholly revoked, and India CCA was constrained to a subset of India's
ccTLD namespace.

  * [67ed4b703d15dc555f8c444b3a05a32579cb7599bd19c9babe10c584ea327ae0.pem](67ed4b703d15dc555f8c444b3a05a32579cb7599bd19c9babe10c584ea327ae0.pem)
  * [a8e1dfd9cd8e470aa2f443914f931cfd61c323e94d75827affee985241c35ce5.pem](a8e1dfd9cd8e470aa2f443914f931cfd61c323e94d75827affee985241c35ce5.pem)
  * [e4f9a3235df7330255f36412bc849fb630f8519961ec3538301deb896c953da5.pem](e4f9a3235df7330255f36412bc849fb630f8519961ec3538301deb896c953da5.pem)

### Sri Lanka

google.lk certificate from Sectigo. https://crt.sh/?id=4037732415

  * [91018fcd3e0dc73f48d011a123f604d846d66821c58304474f949d7449dd600a.pem]
  (91018fcd3e0dc73f48d011a123f604d846d66821c58304474f949d7449dd600a.pem)

### Thawte

A precert that appeared in the CT logs for (www.)google.com, issued by
Thawte. See https://crt.sh/?id=9314698.

  * [0d90cd8e35209b4cefebdd62b644bed8eb55c74dddff26e75caf8ae70491f0bd.pem](0d90cd8e35209b4cefebdd62b644bed8eb55c74dddff26e75caf8ae70491f0bd.pem)

### Togo

google.tg certificates from Let's Encrypt. https://crt.sh/?id=245397170 and
others.

  * [0ef7c54a3af101a2cfedb0c9f36fe8214d51a504fdc2ad1e243019cefd7d03c2.pem](0ef7c54a3af101a2cfedb0c9f36fe8214d51a504fdc2ad1e243019cefd7d03c2.pem)
  * [2a4397aafa6227fa11f9f9d76ecbb022b0a4494852c2b93fb2085c8afb19b62a.pem](2a4397aafa6227fa11f9f9d76ecbb022b0a4494852c2b93fb2085c8afb19b62a.pem)
  * [5472692abe5d02cd22eae3e0a0077f17802721d6576cde1cba2263ee803410c5.pem](5472692abe5d02cd22eae3e0a0077f17802721d6576cde1cba2263ee803410c5.pem)
  * [5ccaf9f8f2bb3a0d215922eca383354b6ee3c62407ed32e30f6fb2618edeea10.pem](5ccaf9f8f2bb3a0d215922eca383354b6ee3c62407ed32e30f6fb2618edeea10.pem)
  * [5e8e77aafdda2ba5ce442f27d8246650bbd6508befbeda35966a4dc7e6174edc.pem](5e8e77aafdda2ba5ce442f27d8246650bbd6508befbeda35966a4dc7e6174edc.pem)
  * [a2e3bdaacaaf2d2e8204b3bc7eddc805d54d3ab8bdfe7bf102c035f67d8f898a.pem](a2e3bdaacaaf2d2e8204b3bc7eddc805d54d3ab8bdfe7bf102c035f67d8f898a.pem)
  * [c71f33c36d8efeefbed9d44e85e21cfe96b36fb0e132c52dca2415868492bf8a.pem](c71f33c36d8efeefbed9d44e85e21cfe96b36fb0e132c52dca2415868492bf8a.pem)
  * [fa5a828c9a7e732692682e60b14c634309cbb2bb79eb12aef44318d853ee97e3.pem](fa5a828c9a7e732692682e60b14c634309cbb2bb79eb12aef44318d853ee97e3.pem)

Another incident in August 2019.

  * [82a4cedbc7f61ce5cb04482aa27ea3145bb0cea58ab63ba1931a1654bfbdbb4f.pem](82a4cedbc7f61ce5cb04482aa27ea3145bb0cea58ab63ba1931a1654bfbdbb4f.pem)

### TrustCor

To coincide with the release of M111, the Chrome Root Program announced a
distrust of the CA Owner "TrustCor".

For details, see <https://groups.google.com/a/mozilla.org/g/dev-security-policy/c/oxX69KFvsm4/m/PKpJf5W6AQAJ>

  * [5a885db19c01d912c5759388938cafbbdf031ab2d48e91ee15589b42971d039c.pem](5a885db19c01d912c5759388938cafbbdf031ab2d48e91ee15589b42971d039c.pem)
  * [0753e940378c1bd5e3836e395daea5cb839e5046f1bd0eae1951cf10fec7c965.pem](0753e940378c1bd5e3836e395daea5cb839e5046f1bd0eae1951cf10fec7c965.pem)
  * [d40e9c86cd8fe468c1776959f49ea774fa548684b6c406f3909261f4dce2575c.pem](d40e9c86cd8fe468c1776959f49ea774fa548684b6c406f3909261f4dce2575c.pem)

### Trustwave

For details, see <https://www.trustwave.com/Resources/SpiderLabs-Blog/Clarifying-The-Trustwave-CA-Policy-Update/>
and <https://bugzilla.mozilla.org/show_bug.cgi?id=724929>

Two certificates were issued by Trustwave for use in enterprise
Man-in-the-Middle. The following public key was used for both certificates,
and is revoked.

  * [32ecc96f912f96d889e73088cd031c7ded2c651c805016157a23b6f32f798a3b.key](32ecc96f912f96d889e73088cd031c7ded2c651c805016157a23b6f32f798a3b.key)

### TurkTrust

For details, see <https://googleonlinesecurity.blogspot.com/2013/01/enhancing-digital-certificate-security.html>
and <https://web.archive.org/web/20130326152502/http://turktrust.com.tr/kamuoyu-aciklamasi.2.html>

As a result of a software configuration issue, two certificates were misissued
by Turktrust that failed to properly set the basicConstraints extension.
Because these certificates can be used to issue additional certificates, they
have been revoked.

  * [372447c43185c38edd2ce0e9c853f9ac1576ddd1704c2f54d96076c089cb4227.pem](372447c43185c38edd2ce0e9c853f9ac1576ddd1704c2f54d96076c089cb4227.pem)
  * [42187727be39faf667aeb92bf0cc4e268f6e2ead2cefbec575bdc90430024f69.pem](42187727be39faf667aeb92bf0cc4e268f6e2ead2cefbec575bdc90430024f69.pem)

## Private Key Leakages

### Cyberoam

For details, see <https://blog.torproject.org/blog/security-vulnerability-found-cyberoam-dpi-devices-cve-2012-3372>

Device manufacturer Cyberoam used the same private key for all devices by
default, which subsequently leaked and is included below. The associated
public key is blocked.

  * [1af56c98ff043ef92bebff54cebb4dd67a25ba956c817f3e6dd3c1e52eb584c1.key](1af56c98ff043ef92bebff54cebb4dd67a25ba956c817f3e6dd3c1e52eb584c1.key)

### Dell

For details, see <http://www.dell.com/support/article/us/en/19/SLN300321>
and <http://en.community.dell.com/dell-blogs/direct2dell/b/direct2dell/archive/2015/11/23/response-to-concerns-regarding-edellroot-certificate>

The private keys for both the eDellRoot and DSDTestProvider certificates were
trivially extracted, and thus their associated public keys are
blocked.

  * [0f912fd7be760be25afbc56bdc09cd9e5dcc9c6f6a55a778aefcb6aa30e31554.pem](0f912fd7be760be25afbc56bdc09cd9e5dcc9c6f6a55a778aefcb6aa30e31554.pem)
  * [ec30c9c3065a06bb07dc5b1c6b497f370c1ca65c0f30c08e042ba6bcecc78f2c.pem](ec30c9c3065a06bb07dc5b1c6b497f370c1ca65c0f30c08e042ba6bcecc78f2c.pem)

### Mitel

For details, see <https://www.mitel.com/support/security-advisories/mitel-product-security-advisory-17-0001>

Certain Mitel products shipped with extractable private keys, the public certs for which users were encouraged to install as anchors.

  * [2a33f5b48176523fd3c0d854f20093417175bfd498ef354cc7f38b54adabaf1a.pem](2a33f5b48176523fd3c0d854f20093417175bfd498ef354cc7f38b54adabaf1a.pem)
  * [2d11e736f0427fd6ba4b372755d34a0edd8d83f7e9e7f6c01b388c9b7afa850d.pem](2d11e736f0427fd6ba4b372755d34a0edd8d83f7e9e7f6c01b388c9b7afa850d.pem)
  * [3ab0fcc7287454c405863e3aa204fea8eb0c50a524d2a7e15524a830cd4ab0fe.pem](3ab0fcc7287454c405863e3aa204fea8eb0c50a524d2a7e15524a830cd4ab0fe.pem)
  * [60911c79835c3739432d08c45df64311e06985c5889dc5420ce3d142c8c7ef58.pem](60911c79835c3739432d08c45df64311e06985c5889dc5420ce3d142c8c7ef58.pem)

### Sennheiser

Certs with disclosed private keys from Sennheiser HeadSetup software.

  * [91e5cc32910686c5cac25c18cc805696c7b33868c280caf0c72844a2a8eb91e2.pem](91e5cc32910686c5cac25c18cc805696c7b33868c280caf0c72844a2a8eb91e2.pem)
  * [ddd8ab9178c99cbd9685ea4ae66dc28bfdc9a5a8a166f7f69ad0b5042ad6eb28.pem](ddd8ab9178c99cbd9685ea4ae66dc28bfdc9a5a8a166f7f69ad0b5042ad6eb28.pem)

### sslip.io

For details, see <https://blog.pivotal.io/labs/labs/sslip-io-a-valid-ssl-certificate-for-every-ip-address>

A subscriber of Comodo's acquired a wildcard certificate for sslip.io, and
then subsequently published the private key, as a means for developers
to avoid having to acquire certificates.

As the private key could be used to intercept all communications to this
domain, the associated public key was blocked.

  * [f3bae5e9c0adbfbfb6dbf7e04e74be6ead3ca98a5604ffe591cea86c241848ec.pem](f3bae5e9c0adbfbfb6dbf7e04e74be6ead3ca98a5604ffe591cea86c241848ec.pem)

### xs4all.nl

For details, see <https://raymii.org/s/blog/How_I_got_a_valid_SSL_certificate_for_my_ISPs_main_website.html>

A user of xs4all was able to register a reserved email address that can be
used to cause certificate issuance, as described in the CA/Browser Forum's
Baseline Requirements, and then subsequently published the private key.

  * [83618f932d6947744d5ecca299d4b2820c01483947bd16be814e683f7436be24.pem](83618f932d6947744d5ecca299d4b2820c01483947bd16be814e683f7436be24.pem)

### Superfish

For details, see <https://www.eff.org/deeplinks/2015/02/how-remove-superfish-adware-your-lenovo-computer>

Superfish software with an associated root certificate came preinstalled on
Lenovo computers. The software used a single root certificate across all
computers, and the private key was trivially extracted; thus the associated
public key was blocked.

  * [b6fe9151402bad1c06d7e66db67a26aa7356f2e6c644dbcf9f98968ff632e1b7.pem](b6fe9151402bad1c06d7e66db67a26aa7356f2e6c644dbcf9f98968ff632e1b7.pem)

## Miscellaneous

### DigiCert

For details, see <https://bugzilla.mozilla.org/show_bug.cgi?id=1242758> and
<https://bugzilla.mozilla.org/show_bug.cgi?id=1224104>

These two intermediates were retired by DigiCert, and blocked for
robustness at their request.

  * [159ca03a88897c8f13817a212629df84ce824709492b8c9adb8e5437d2fc72be.pem](159ca03a88897c8f13817a212629df84ce824709492b8c9adb8e5437d2fc72be.pem)
  * [b8c1b957c077ea76e00b0f45bff5ae3acb696f221d2e062164fe37125e5a8d25.pem](b8c1b957c077ea76e00b0f45bff5ae3acb696f221d2e062164fe37125e5a8d25.pem)

### E-GUVEN

X.509v1 CA cert issued by E-GUVEN.  Removed from some but not all root stores.

  * [8253da6738b60c5c0bb139c78e045428a0c841272abdcb952f95ff05ed1ab476.pem](8253da6738b60c5c0bb139c78e045428a0c841272abdcb952f95ff05ed1ab476.pem)

### Hacking Team

The following keys were reported as used by Hacking Team to compromise users,
and are blocked for robustness.

  * [c4387d45364a313fbfe79812b35b815d42852ab03b06f11589638021c8f2cb44.key](c4387d45364a313fbfe79812b35b815d42852ab03b06f11589638021c8f2cb44.key)
  * [ea08c8d45d52ca593de524f0513ca6418da9859f7b08ef13ff9dd7bf612d6a37.key](ea08c8d45d52ca593de524f0513ca6418da9859f7b08ef13ff9dd7bf612d6a37.key)

### JCSI

"Lost" intermediate from Japan Certification Services.  See
https://bugzilla.mozilla.org/show_bug.cgi?id=1314464, https://crt.sh/?id=6320.

  * [d0d672c2547d574ae055d9e78a993ddbcc74044c4253fbfaca573a67d368e1db.pem](d0d672c2547d574ae055d9e78a993ddbcc74044c4253fbfaca573a67d368e1db.pem)


### live.fi

For details, see <https://technet.microsoft.com/en-us/library/security/3046310.aspx>

A user of live.fi was able to register a reserved email address that can be
used to cause certificate issuance, as described in the CA/Browser Forum's
Baseline Requirements. This was not intended by Microsoft, the operators of
live.fi, but conformed to the Baseline Requirements. It was blocked for
robustness.

  * [c67d722c1495be02cbf9ef1159f5ca4aa782dc832dc6aa60c9aa076a0ad1e69d.pem](c67d722c1495be02cbf9ef1159f5ca4aa782dc832dc6aa60c9aa076a0ad1e69d.pem)

### Microsoft Dynamics 365

https://bugzilla.mozilla.org/show_bug.cgi?id=1423400

  * [3d3d823fad13dfeef32da580166d4a4992bed5a22d695d12c8b08cc3463c67a2.pem](3d3d823fad13dfeef32da580166d4a4992bed5a22d695d12c8b08cc3463c67a2.pem)
  * [c43807a64c51a3fbde5421011698013d8b46f4e315c46186dc23aea2670cd34f.pem](c43807a64c51a3fbde5421011698013d8b46f4e315c46186dc23aea2670cd34f.pem)

### Qaznet Trust Network

For details, see <https://security.googleblog.com/2019/08/protecting-chrome-users-in-kazakhstan.html>

  * [00309c736dd661da6f1eb24173aa849944c168a43a15bffd192eecfdb6f8dbd2.pem](00309c736dd661da6f1eb24173aa849944c168a43a15bffd192eecfdb6f8dbd2.pem)
  * [61c0fc2e38b5b6f9071b42cee54a9013d858b6697c68b460948551b3249576a1.pem](61c0fc2e38b5b6f9071b42cee54a9013d858b6697c68b460948551b3249576a1.pem)
  * [1df696f021ab1c3ace9a376b07ed7256a40214cd3396d7934087614924e2d7ef.pem](1df696f021ab1c3ace9a376b07ed7256a40214cd3396d7934087614924e2d7ef.pem)
  * [0230a604d99220e5612ee7862ab9f7a6e18e4f1ac4c9e27075788cc5220169ab.pem](0230a604d99220e5612ee7862ab9f7a6e18e4f1ac4c9e27075788cc5220169ab.pem)
  * [06fd20629c143b9eab28d2799caefc5d23fde267d16c631e3f5b8b4bab3f68e6.pem](06fd20629c143b9eab28d2799caefc5d23fde267d16c631e3f5b8b4bab3f68e6.pem)
  * [0bd39de4793cdc117138f47708aa4d583acf67adb059a0d91f668d1803bf6489.pem](0bd39de4793cdc117138f47708aa4d583acf67adb059a0d91f668d1803bf6489.pem)
  * [c95c133b68319ee516b5f41e377f589878af1556567cc2834ef03b1d10830fd3.pem](c95c133b68319ee516b5f41e377f589878af1556567cc2834ef03b1d10830fd3.pem)
  * [c530fadc9bfa265e63b755cc6ee04c2d70d60bb916ce2f331dc7359362571b25.pem](c530fadc9bfa265e63b755cc6ee04c2d70d60bb916ce2f331dc7359362571b25.pem)
  * [89107c8e50e029b7b5f4ff0ccd2956bcc9d0c8ba2bfb6a58374ed63a6b034a30.pem](89107c8e50e029b7b5f4ff0ccd2956bcc9d0c8ba2bfb6a58374ed63a6b034a30.pem)
  * [3472e4f16c570e0dd388aaaa4a64a34a4b939f1ca770996b5be0037c1aded9c1.pem](3472e4f16c570e0dd388aaaa4a64a34a4b939f1ca770996b5be0037c1aded9c1.pem)

### revoked.badssl.com

  * [29abf614b2870ed70df11225e9ae2068e3074eb9845ae252c2064e31ce9fe8a1.pem](29abf614b2870ed70df11225e9ae2068e3074eb9845ae252c2064e31ce9fe8a1.pem)

### blocked-interception.badssl.com

  * [44a244105569a730791f509b24c3d7838a462216bb0f560ef87fbe76c2e6005a](44a244105569a730791f509b24c3d7838a462216bb0f560ef87fbe76c2e6005a.pem)

### known-interception.badssl.com

  * [143315c857a9386973ed16840899c3f96b894a7a612c444efb691f14b0dedd87](143315c857a9386973ed16840899c3f96b894a7a612c444efb691f14b0dedd87.pem)

### revoked.grc.com

  * [53d48e7b8869a3314f213fd2e0178219ca09022dbe50053bf6f76fccd61e8112.pem](53d48e7b8869a3314f213fd2e0178219ca09022dbe50053bf6f76fccd61e8112.pem)

### SECOM

For details, see <https://bugzilla.mozilla.org/show_bug.cgi?id=1188582>

This intermediate certificate was retired by SECOM, and blocked for
robustness at their request.

  * [817d4e05063d5942869c47d8504dc56a5208f7569c3d6d67f3457cfe921b3e29.pem](817d4e05063d5942869c47d8504dc56a5208f7569c3d6d67f3457cfe921b3e29.pem)

### Symantec

For details, see <https://bugzilla.mozilla.org/show_bug.cgi?id=966060>

These three intermediate certificates were retired by Symantec, and
blocked for robustness at their request.

  * [1f17f2cbb109f01c885c94d9e74a48625ae9659665d6d7e7bc5a10332976370f.pem](1f17f2cbb109f01c885c94d9e74a48625ae9659665d6d7e7bc5a10332976370f.pem)
  * [3e26492e20b52de79e15766e6cb4251a1d566b0dbfb225aa7d08dda1dcebbf0a.pem](3e26492e20b52de79e15766e6cb4251a1d566b0dbfb225aa7d08dda1dcebbf0a.pem)
  * [7abd72a323c9d179c722564f4e27a51dd4afd24006b38a40ce918b94960bcf18.pem](7abd72a323c9d179c722564f4e27a51dd4afd24006b38a40ce918b94960bcf18.pem)

### T-Systems

For details, see <https://bugzilla.mozilla.org/show_bug.cgi?id=1076940>

This intermediate certificate was retired by T-Systems, and blocked
for robustness at their request.

  * [f4a5984324de98bd979ef181a100cf940f2166173319a86a0d9d7c8fac3b0a8f.pem](f4a5984324de98bd979ef181a100cf940f2166173319a86a0d9d7c8fac3b0a8f.pem)

### WoSign/StartCom

For details, see <https://security.googleblog.com/2016/10/distrusting-wosign-and-startcom.html>

  * [4b22d5a6aec99f3cdb79aa5ec06838479cd5ecba7164f7f22dc1d65f63d85708.pem](4b22d5a6aec99f3cdb79aa5ec06838479cd5ecba7164f7f22dc1d65f63d85708.pem)
  * [7d8ce822222b90c0b14342c7a8145d1f24351f4d1a1fe0edfd312ee73fb00149.pem](7d8ce822222b90c0b14342c7a8145d1f24351f4d1a1fe0edfd312ee73fb00149.pem)
  * [8b45da1c06f791eb0cabf26be588f5fb23165c2e614bf885562d0dce50b29b02.pem](8b45da1c06f791eb0cabf26be588f5fb23165c2e614bf885562d0dce50b29b02.pem)
  * [c766a9bef2d4071c863a31aa4920e813b2d198608cb7b7cfe21143b836df09ea.pem](c766a9bef2d4071c863a31aa4920e813b2d198608cb7b7cfe21143b836df09ea.pem)
  * [c7ba6567de93a798ae1faa791e712d378fae1f93c4397fea441bb7cbe6fd5995.pem](c7ba6567de93a798ae1faa791e712d378fae1f93c4397fea441bb7cbe6fd5995.pem)
  * [d487a56f83b07482e85e963394c1ecc2c9e51d0903ee946b02c301581ed99e16.pem](d487a56f83b07482e85e963394c1ecc2c9e51d0903ee946b02c301581ed99e16.pem)
  * [d6f034bd94aa233f0297eca4245b283973e447aa590f310c77f48fdf83112254.pem](d6f034bd94aa233f0297eca4245b283973e447aa590f310c77f48fdf83112254.pem)
  * [e17890ee09a3fbf4f48b9c414a17d637b7a50647e9bc752322727fcc1742a911.pem](e17890ee09a3fbf4f48b9c414a17d637b7a50647e9bc752322727fcc1742a911.pem)
  * [4aefc3d39ef59e4d4b0304b20f53a8af2efb69edece66def74494abfc10a2d66.pem](4aefc3d39ef59e4d4b0304b20f53a8af2efb69edece66def74494abfc10a2d66.pem)
  * [cb954e9d80a3e520ac71f1a84511657f2f309d172d0bb55e0ec2c236e74ff4b4.pem](cb954e9d80a3e520ac71f1a84511657f2f309d172d0bb55e0ec2c236e74ff4b4.pem)

### www.cloudflarechallenge.com

  * [e757fd60d8dd4c26f77aca6a87f63ea4d38d0b736c7f79b56cad932d4c400fb5.pem](e757fd60d8dd4c26f77aca6a87f63ea4d38d0b736c7f79b56cad932d4c400fb5.pem)
