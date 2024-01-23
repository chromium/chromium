# ForceField: An iOS Sandbox Primitive

_**Status:** Filed as FB9007081_ \
_**Author:** rsesek@, palmer@_ \
_**Created:** 2021-02-04_ \
_**Updated:** 2021-02-16_

## Description

This is a request for a new iOS feature (here called **ForceField**), which
would provide app developers a primitive to process-isolate and sandbox
memory-unsafe code in a way that is safe for manipulating untrustworthy and
potentially malicious data.

## Objective

The goal of ForceField is to improve the safety and security of users, by
reducing the privilege level of memory-unsafe code that processes untrustworthy
data from the Internet.

Many complex applications have components that are written in memory-unsafe
languages like C/C++. While iOS does offer Swift as a (mostly) memory-safe
language, often these components are shared across platforms or are third-party
dependencies. Today, if an iOS application uses these components to process data
from the Internet, they make themselves vulnerable to memory unsafety bugs.

A common solution to mitigate these vulnerabilities is to perform the operations
on untrustworthy data in a separate process that runs under a tight sandbox,
following the [principle of least privilege](https://en.wikipedia.org/wiki/Principle_of_least_privilege).
Currently iOS does not offer apps a mechanism to compose their components into
high- and low-privilege execution environments. However, iOS itself uses
[process isolation and sandboxing](https://googleprojectzero.blogspot.com/2021/01/a-look-at-imessage-in-ios-14.html)
for privilege reduction in similar situations. ForceField would give developers
a primitive to do the same, which would help protect the people who use their
apps.

## The Ideal Solution

A perfect implementation of ForceField would allow app developers to create a
new component that is packaged in their application bundle. iOS would launch
this component as a new, sandboxed process running under a security principal
distinct from the bundle’s primary process. ForceField processes would:

*   Not have access to the containing app’s data storage
*   Not have access to privileged shared system resources (Keychain, clipboard,
    persistent storage locations)
*   Not have access to system services that access user data, such as Location
    Services, Photos, HomeKit, HealthKit, AutoFill, etc.
*   By default, not have access to draw to the screen
*   By default, not have network access

Thus, ForceField would provide a compute-only process, which the main app
process would communicate with over an IPC mechanism. By default, the only
resources FourceField could access would be the ones explicitly brokered in; and
the only way to extract data from the ForceField process would be for it to
likewise send results over IPC to the primary app. ForceField would enable
running memory-unsafe code in such a way that it would be much safer to process
untrustworthy and potentially malicious data.

Furthermore, ForceField could be enhanced by allowing the developer to opt-in to
specific privileged capabilities, for example network access. This would be
useful for initiating NSURLSession connections, allowing the ForceField
component to directly process untrustworthy network data without needing to
round-trip it through the primary app component. Removing the round-trip would
be more performant and reduce the risk of mis-handling the data in the trusted,
primary app component.

## Leverage Existing Technologies

iOS’s existing technologies provide all the necessary pieces to create ForceField:

### App Extensions

iOS provides a mechanism for apps to run context-limited code in a distinct
process through [App Extensions](https://developer.apple.com/app-extensions/). A
new type of “Compute” App Extension could be created to implement ForceField.
The app could engage one of its Compute Extensions at any point during its
lifecycle, whenever it is necessary to process data in a sandbox. An app should
be able to launch a Compute App Extension when its foreground, but also
potentially when processing data downloaded during
[background refresh](https://developer.apple.com/documentation/uikit/app_and_environment/scenes/preparing_your_ui_to_run_in_the_background/updating_your_app_with_background_app_refresh?).

The app should also have the ability to forcefully terminate a Compute
Extension, in response to e.g. user cancellation or memory pressure signals. And
the app should be able to register a termination handler for the Compute
Extension, so it can determine if the process exited cleanly, crashed, was
killed by the app, or was killed by the operating system. The operating system
could kill all Compute App Extension processes a few seconds after the
foreground app moves to the background. And the operating system could place
limits on total resource consumption (CPU and memory) of the Compute App
Extension. The Compute App Extension should be tightly sandboxed, per the
description of ForceField above.

### XPC

iOS already exposes
[NSXPCConnection](https://developer.apple.com/documentation/foundation/nsxpcconnection?language=objc)
and
[NSXPCInterface](https://developer.apple.com/documentation/foundation/nsxpcinterface?language=objc)
in the iPhoneOS SDK, and it is used to implement e.g.
[File Provider](https://developer.apple.com/documentation/fileprovider/nsfileproviderservicesource/2915876-makelistenerendpointandreturnerr?language=objc)
app extensions. The NSXPC API is also already [used by developers on macOS](https://developer.apple.com/library/archive/documentation/MacOSX/Conceptual/BPSystemStartup/Chapters/CreatingXPCServices.html)
to create application-specific IPC protocols between components. iOS developers
could use the NSXPC API to define the IPC protocol between the primary app and
its ForceField components.

### Entitlements

New iOS entitlements could be created to enable ForceField components to opt
into additional, privileged capabilities on top of the tightly-sandboxed
baseline. For example, an entitlement could be created to enable a ForceField
component to access the network. Another entitlement could enable rendering into
a CALayer brokered in over IPC, to enable remote rendering of UI by the
ForceField process into the primary application process.
