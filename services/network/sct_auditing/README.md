# SCT Auditing

`//services/network/sct_auditing` contains the core of Chrome's implementation
of Signed Certificate Timestamp (SCT) auditing. SCT auditing is an approach to
verify that server certificates are being properly logged via Certificate
Transparency (CT).

The current implementation is described in the
[Opt-in SCT Auditing design doc][1].

[1]: https://docs.google.com/document/d/1G1Jy8LJgSqJ-B673GnTYIG4b7XRw2ZLtvvSlrqFcl4A/edit
