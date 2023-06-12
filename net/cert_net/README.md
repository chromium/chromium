`cert_net/` contains certificate functionality that depends on network loading
(OCSP, CRL, AIA fetching).

The implementation in this directory is built on `URLRequest`. See also
`services/cert_verifier/cert_net_url_loader/` for an implementation built on
`URLLoader`.

Conceptually certificates (`net/cert/`) is a separable concept from `net/` and
may end up becoming its own build target. This file organization encourages not
adding dependencies in `cert/` for doing network loading. Instead that code
should be placed here.
