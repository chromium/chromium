# TLS SHA-1 Server Signatures

* Specification: [RFC 9155](https://www.rfc-editor.org/rfc/rfc9155.html)
* Chrome Status: [Deprecate TLS SHA-1 server signatures](https://chromestatus.com/feature/4832850040324096)

[SHA-1](https://www.nist.gov/news-events/news/2022/12/nist-retires-sha-1-cryptographic-algorithm) is an insecure hash algorithm with increasingly significant attacks discovered ([2015](https://en.wikipedia.org/wiki/SHA-1#The_SHAppening), [2017](https://shattered.io/), [2020](https://sha-mbles.github.io/)). To ensure these attacks cannot be used to impersonate a web server, browsers have steadily removed SHA-1 dependencies from HTTPS. In 2017, Chrome removed support for SHA-1 in [certificates](https://www.chromium.org/Home/chromium-security/education/tls/sha-1). In 2020, Chrome disabled [TLS 1.0 and 1.1](https://www.chromestatus.com/feature/5759116003770368), which use SHA-1 throughout.

Chrome 117 is [removing](https://chromestatus.com/feature/4832850040324096) support for SHA-1 from the TLS 1.2 [server signature](https://www.rfc-editor.org/rfc/rfc9155.html).


## Background

TLS authenticates servers in two parts. First, the certificate, signed by the CA, tells the client what is `example.com`'s public key. Second, the TLS server software uses the corresponding private key in the handshake, to bind it to the connection. In most cipher suites, this involves the server making a signature. This change is to no longer allow SHA-1 in this signature. That is, Chrome will no longer [offer SHA-1 in the signature\_algorithms field](https://www.rfc-editor.org/rfc/rfc9155.html) and, correspondingly will no longer [accept it in ServerKeyExchange signatures](https://www.rfc-editor.org/rfc/rfc9155.html#section-4).

This use of SHA-1 is _not_ the same as:

*   SHA-1 in the server's certificate
*   "SHA" in cipher suites, which refers to HMAC-SHA-1 in legacy CBC cipher suites


## Is my website affected?

You can test your website by toggling the "Allow SHA-1 server signatures in TLS" flag. Go to `chrome://flags/#use-sha1-server-handshakes`. If setting it to "Enabled" causes the site to work, but setting it to "Disabled" causes it to break, the website is affected. This flag is temporary and will be removed in the future. It may be used for now to help diagnose issues, but, long-term, the server should be fixed.

Depending on the exact cause, this issue can appear differently, such as an `ERR_SSL_PROTOCOL_ERROR` or `ERR_CONNECTION_RESET`, though this is not the only possible cause of those errors.


## Troubleshooting

All correctly-implemented TLS 1.2 servers already support SHA-2 and pick a common algorithm based on server support and what the client offers. While it is possible to misconfigure servers such that SHA-1 is the only algorithm, this is rare. This is most commonly a bug in the server software.

Here are known issues and how to fix them. If your server software is not listed, contact your software vendor for a fix.


### OpenSSL

OpenSSL versions from 1.0.1 to 1.0.1i (August 2014), as 1.0.2 to 1.0.2l (May 2017) did not correctly track signature algorithm state and, as a result, some server applications would lose track of the peer's preferences and only sign SHA-1 when [SNI](https://en.wikipedia.org/wiki/Server_Name_Indication) is offered.

Impacted servers will usually fail with `ERR_SSL_PROTOCOL_ERROR`. Impacted servers will also respond to connections with and without SNI distinctively. Replace `EXAMPLE.COM` with the name of the server:


```
$ openssl s_client -connect EXAMPLE.COM:443 -servername EXAMPLE.COM -sigalgs rsa_pkcs1_sha256 -tls1_2 -quiet
...
40E7F950417F0000:error:0A000172:SSL routines:tls12_check_peer_sigalg:wrong signature type:../ssl/t1_lib.c:1594:

$ openssl s_client -connect EXAMPLE.COM:443 -noservername -sigalgs rsa_pkcs1_sha256 -tls1_2 -quiet
(connection succeeds)
```


This was [fixed for the 1.0.1](https://github.com/openssl/openssl/commit/4e05aedbcab7f7f83a887e952ebdcc5d4f2291e4) series in 1.0.1j, released October 2014. There was a [partial fix](https://github.com/openssl/openssl/commit/1ce95f19601bbc6bfd24092c76c8f8105124e857) for 1.0.2, but it was incomplete, so some applications continued to be affected until a [complete fix](https://github.com/openssl/openssl/pull/4577) in 1.0.2m, released November 2017. Impacted servers should update to a sufficiently new version. The 1.0.1 and 1.0.2 series have been end-of-life since December 2016 and December 2019, respectively, so updating to a supported version is recommended.

Additionally, there have been many [OpenSSL security advisories](https://www.openssl.org/news/vulnerabilities.html) since these bugs were fixed. While unrelated to this Chrome change, we would also recommend reviewing the missed advisories for further actions. In particular, very old OpenSSL versions may be vulnerable to [Heartbleed](https://heartbleed.com/), in which case the server private key should be assumed compromised.


### IIS with SHA-1 server certificates

The `signature_algorithms` field is primarily used for negotiating the server signature, but it is also used to guide server certificate selection. Some servers, notably IIS, apply this strictly and will reject the connection if any server certificate is inconsistent with the client's `signature_algorithms`.

Chrome already removed support for SHA-1 server certificates in 2017. However, some servers are configured to send an unnecessary self-signed SHA-1 root certificate. This certificate is normally ignored by the client and thus harmless. However, older versions of IIS will still apply strict checks to it and then reject the connection.

If the impacted server uses IIS and sends a server certificate chain with SHA-1 at the root (or elsewhere), this is a likely cause.

Newer versions of Windows Schannel no longer check the extraneous certificate. Impacted servers should apply Windows updates. Alternatively, impacted servers can reconfigure their servers to [not send the unnecessary certificate](https://www.rfc-editor.org/rfc/rfc5246.html#section-7.4.2) and reduce bandwidth.


### IIS with SHA-1 client certificates

TLS client certificates (sometimes referred to with the non-standard term “mTLS”) authenticate analogously to server certificates: the certificate tells the server the public key, then the client makes a client signature with the client private key. As in the analogous direction, the server sends a list of signature algorithms, which the client picks from.

While this change does not impact SHA-1 in either client certificates or client private key, older versions of IIS would apply the client's preferences (which control the server certificate and signature) to the client ones. Thus, deployments that depend on SHA-1 client certificates or signatures would break when SHA-1 _server_ signatures are no longer allowed. Impacted servers will usually fail _after_ the client sends the client certificate.

Newer versions of Windows Schannel have fixed this issue. Impacted servers should apply Windows updates. Additionally, while unrelated to this deprecation, enterprises relying on SHA-1 client certificates or signatures are recommended to migrate to SHA-2, to ensure attackers cannot use SHA-1 weaknesses to impersonate clients.


## Enterprise policy

Enterprise administrators who need more time can set the [InsecureHashesInTLSHandshakesEnabled](https://chromeenterprise.google/policies/#InsecureHashesInTLSHandshakesEnabled) enterprise policy as a temporary workaround. However, this is a temporary policy and will be removed in Chrome 123. Additionally, as this allows an insecure hash function in a critical part of the TLS handshake, enabling this policy does increase the risk of attackers impersonating servers within an enterprise deployment.
