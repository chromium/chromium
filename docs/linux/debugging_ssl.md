# Debugging SSL on Linux

To help anyone looking at the SSL code, here are a few tips I've found handy.

[TOC]

## Logging

There are several flavors of logging you can turn on.

*   `SSLClientSocketImpl` can log its state transitions and function calls
     using `base/logging.cc`.  To enable this, edit
     `net/socket/ssl_client_socket_impl.cc` and change `#if 1` to `#if 0`. See
     `base/logging.cc` for where the output goes (on Linux, usually stderr).
     
*   `HttpNetworkTransaction` and friends can log its state transitions using
    `base/trace_event.cc`. To enable this, arrange for your app to call
    `base::TraceLog::StartTracing()`. The output goes to a file named
    `trace...pid.log` in the same directory as the executable (e.g.
    `Hammer/trace_15323.log`).

## Network Traces

http://wiki.wireshark.org/SSL describes how to decode SSL traffic. Chromium SSL
unit tests that use `net/base/ssl_test_util.cc` to set up their servers always
use port 9443 with `net/data/ssl/certificates/ok_cert.pem`, and port 9666 with
`net/data/ssl/certificates/expired_cert.pem` This makes it easy to configure
Wireshark to decode the traffic: do

Edit / Preferences / Protocols / SSL, and in the "RSA Keys List" box, enter

    127.0.0.1,9443,http,<path to ok_cert.pem>;127.0.0.1,9666,http,<path to expired_cert.pem>

e.g.

    127.0.0.1,9443,http,/home/dank/chromium/src/net/data/ssl/certificates/ok_cert.pem;127.0.0.1,9666,http,/home/dank/chromium/src/net/data/ssl/certificates/expired_cert.pem

Then capture all tcp traffic on interface lo, and run your test.
