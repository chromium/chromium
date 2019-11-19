# Certificate Blocklist

This directory contains a number of certificates and public keys which are to be
treated as named-constrained during certiicate validation within Chromium-based
products. 

When applicable, additional information and the full certificate or key
are included.

## Compromises & Misissuances

### India CCA

For details, see <https://googleonlinesecurity.blogspot.com/2014/07/maintaining-digital-certificate-security.html>
and <https://technet.microsoft.com/en-us/library/security/2982792.aspx>

An unknown number of misissued certificates were issued by a sub-CA of
India CCA, the India NIC. Due to the scope of the misissuance, the sub-CA
was wholly revoked, and India CCA was constrained to a subset of India's
ccTLD namespace.

  * [2d66a702ae81ba03af8cff55ab318afa919039d9f31b4d64388680f81311b65a.pem](2d66a702ae81ba03af8cff55ab318afa919039d9f31b4d64388680f81311b65a.pem)
  * [60109bc6c38328598a112c7a25e38b0f23e5a7511cb815fb64e0c4ff05db7df7.pem](60109bc6c38328598a112c7a25e38b0f23e5a7511cb815fb64e0c4ff05db7df7.pem)
  * [b9bea7860a962ea3611dab97ab6da3e21c1068b97d55575ed0e11279c11c8932.pem](b9bea7860a962ea3611dab97ab6da3e21c1068b97d55575ed0e11279c11c8932.pem)
  * [f375e2f77a108bacc4234894a9af308edeca1acd8fbde0e7aaa9634e9daf7e1c.pem](f375e2f77a108bacc4234894a9af308edeca1acd8fbde0e7aaa9634e9daf7e1c.pem)
