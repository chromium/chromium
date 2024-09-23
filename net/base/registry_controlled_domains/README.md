# Registry-Controlled Domain Service

This directory contains Chromium's copy of the
[Public Suffix List](https://publicsuffix.org/) and utilities for accessing it.
This is useful for callers who need to understand what the eTLD ("effective
Top-Level Domain") + 1 is for some hostname, which in turn is necessary to
determine things like "this host should be allowed to read/write cookies from
this other host".

Update instructions can be found at src/net/tools/tld_cleanup/README.
