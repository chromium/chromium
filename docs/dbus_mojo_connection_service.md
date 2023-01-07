# D-Bus Mojo Connection Service

## Overview

D-Bus Mojo Connection Service in Chrome is a D-Bus service that helps to
bootstrap CrOS services' Mojo connection.

## Bootstrap a new CrOS service

D-Bus Mojo Connection Service lives in [//chrome/browser/ash/dbus/mojo_connection_service_provider.h](https://chromium.googlesource.com/chromium/src.git/+/main/chrome/browser/ash/dbus/mojo_connection_service_provider.h).

Follow the example of CrOS Sensors' [changelist](https://chromium-review.googlesource.com/c/chromium/src/+/2352298).

### Steps to Add a usage for a CrOS process with a new D-Bus method:
1. Add a method name in both CrOS platform and Chrome.
   (Recommend: `platform2/system_api`)
2. Add the busconfig policy in [MojoConnectionService.conf].
3. Upon a D-Bus request coming from the CrOS service, pass one endpoint of the
   generated Mojo pipe to the component in Chrome that needs a Mojo channel to
   the CrOS service. Ex: [RegisterServer](https://chromium-review.googlesource.com/c/chromium/src/+/2352298/16/chrome/browser/ash/dbus/mojo_connection_service_provider.cc#74) in CrOS Sensors' usage.
4. Respond to the D-Bus request with the other endpoint of the generated Mojo
   pipe. (Recommend: use the helper function [SendResponse](https://chromium-review.googlesource.com/c/chromium/src/+/2352298/16/chrome/browser/ash/dbus/mojo_connection_service_provider.h#75))

The Mojo pipe can also be generated in the CrOS process, and pass the endpoint
of it as the D-Bus argument to the service provider, instead of allowing Chrome
to generate the pipe.

### Steps to Add a usage for a CrOS process with an existing D-Bus method:
1. Add the busconfig policy in [MojoConnectionService.conf].

And that’s it. The method name and the logic in the service provider can be
reused.

## Security

UID filtering should be used to ensure only the needed processes are calling
the specific D-Bus methods, as processes/applications calling D-Bus APIs are
trusted (written and reviewed by Chromium/CrOS teams) and should have a
well-known UID to be filtered.

UID filtering: Define access permission for each UID in
[MojoConnectionService.conf]. Only the processes run under the specific UIDs can
send respective D-Bus requests to the service provider.

Arguments/tokens in D-Bus methods are still available if needed, which should
be enough for multi-login situations and handling failures. The arguments can
also be used to determine if Chromium should accept the request, and which Mojo
interface should be used to establish the Mojo channel.

[MojoConnectionService.conf]: https://chromium.googlesource.com/chromium/src.git/+/main/chrome/browser/ash/dbus/org.chromium.MojoConnectionService.conf
