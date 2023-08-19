# Certificate verification

This directory contains the core code for verifying server certificates.
Limited support is also included for verifying client certificates, but only to
the extent they chain to a server-supplied set of issuers.

Server certificate verification emphasizes the standards/policy for
publicly trusted certificates:

 * Basic X.509 digital certificates
 * RFC 5280
 * CA/Browser Forum Baseline Requirements
 * CRLSets
 * Certificate Transparency

The core logic of certificate verification is implemented synchronously, as it
may need to integrate with synchronous OS-provided APIs. This synchronous
implementation is performed through the [CertVerifyProc](cert_verify_proc.h)
interface, which is a thread-agnostic/thread-safe interface that can be used to
verify certificates synchronously on arbitrary worker threads.

The top-level interface for verifying server certificates is the asynchronous
[CertVerifier](cert_verifier.h).

[MultiThreadedCertVerifier](multi_threaded_cert_verifier.h) is an
implementation of `CertVerifier` that executes `CertVerifyProc` synchronously
on worker threads.

[CertVerifyProcBuiltin](cert_verify_proc_builtin.h) is a cross-platform
implementation which implements path building internally. It only relies on
platform integrations for obtaining user and enterprise configured trusted root
certificates. The publicly trusted root certificates are supplied by the
[Chrome Root Store](../data/ssl/chrome_root_store/README.md).

The other `CertVerifyProc` implementations are for integrating
with the underlying platform's certificate verification library.
There are 2 platform implementations:
[CertVerifyProcAndroid](cert_verify_proc_android.h) and
[CertVerifyProcIOS](cert_verify_proc_ios.h).

Browser-specific policy checks are applied even when using the platform's
certificate verifier. For instance, a certificate chain the OS deemed valid
could ultimately be rejected by `CertVerifyProc` since it independently
checks the chain for CRLSet revocation, use of weak keys, Baseline Requirements
validity, name constraints, weak signature algorithms, and more.
