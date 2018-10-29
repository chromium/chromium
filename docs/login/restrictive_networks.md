# Working with Restrictive Networks

ChromeOS login supports letting the user connect to a restricted network,
such as one that has a captive portal (e.g. a terms of service screen).

Testing on a restricted network can be tricky. The most robust way to test is
to actually connect your device to a restricted network, but this can greatly
slow iteration time. An alternative is to run chrome locally with
`--proxy-server` and run a proxy HTTP server locally that emulates the
restricted network.

## go-authproxy

[go-authproxy](https://github.com/jacobdufault/go-authproxy) is a proxy that
implements HTTP basic authentication and supports serving a captive portal.

To require HTTP basic authentication

```sh
# terminal A
$ go-authproxy -basic-auth user:pass # interrupt (e.g. <c-c>) to shutdown
# terminal B
$ chrome --proxy-server="127.0.0.1:8080"
```

To show a captive portal

```sh
# terminal A
$ go-authproxy -captive-portal # interrupt (e.g. <c-c>) to shutdown
# terminal B
$ chrome --proxy-server="127.0.0.1:8080"
```