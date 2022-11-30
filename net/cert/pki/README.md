# Web PKI Certificate path building and verification

This directory contains the core internal code used for path building
and verifying certificates. This is existing temporarily in this
directory while changes are made to remove chromium specific
dependencies. This code will be moving to it's own separate library
in boringssl (Issue 1322914).

Please do not depend on this directory continuing to exist, and please
try to avoid adding dependencies on anything in this directory from
outside of //net/cert.
