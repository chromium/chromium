# Android IPC Security Considerations

Generally Chrome communicates between its processes using the
[Mojo](../../mojo/README.md) [inter-process communication (IPC)
mechanism](mojo.md). For most features, this is the preferred IPC mechanism to
use. However, as an Android application, there are certain interactions with
other applications and the Android OS that necessitate using different IPC
mechanisms to communicate. This document covers security concerns related to
those Android-specific IPC mechanisms.

The Chrome browser process is typically the only process type that will interact
with these different IPC mechanisms.

## Intents

[Intents](https://developer.android.com/guide/components/intents-filters) are
the most common type of inter-process communication mechanism on Android. They
are most commonly used to start Activities and they internally carry data
associated with that Activity (e.g. using the `ACTION_SEND` Intent to share a
piece of content and including either text or image data in the Intent body).

### Inbound Intents

Because any application can dispatch Intents with Chrome as the receiver, when
receiving an inbound Intent, you should never fully trust the data contained
within. Data sent from other applications could be malicious or malformed, and
so you must validate or sanitze the data before passing it to other trusted
components of the browser process. Intents are handled in Java though, so
following the [Rule of 2](rule-of-2.md) is generally easy. (Though take note
that certain Android classes are just Java wrappers around native code, which
would not be considered safe by that rule.)

Inbound Intents may also pose deserialization issues via the data stored in an
Intent's extras. These issues may result in non-exploitable crashes (e.g.
https://crbug.com/1232099), but it is also possible to have deserialization
vulnerabilities with security implications. Always use the
[`IntentUtils.safe*Extra()`](https://source.chromium.org/chromium/chromium/src/+/main:base/android/java/src/org/chromium/base/IntentUtils.java;l=58;drc=7f1297bacd32fe668d4c99cb8963b56aed363acc)
family of methods to access Intent extra fields from inbound Intents.

It is **fundamentally impossible** to determine the sender of an Intent, unless
the Activity was started with
[`startActivityForResult`](https://developer.android.com/reference/android/app/Activity#startActivityForResult(android.content.Intent,%20int)).
For Intents that are started via `startActivityForResult`, you can use
[`getCallingActivity`](https://developer.android.com/reference/android/app/Activity#getCallingActivity())
or
[`getCallingPackage`](https://developer.android.com/reference/android/app/Activity#getCallingPackage())
to retrieve the identity of the component that called
[`setResult`](https://developer.android.com/reference/android/app/Activity#setResult(int))
on the started Activity. For all other cases, the security model of your feature
cannot depend on authenticating the sender of an Intent. Do not trust
`Intent.EXTRA_REFERRER`. See also the discussion below about [capability
tokens](#capability-tokens).

One way to authorize Intents is to use the system's
[`android:permission`](https://developer.android.com/guide/topics/permissions/overview#permission_enforcement)
attribute on a component's (e.g. Activity, Service, etc.) manifest declaration.
You can [define a custom permission](https://developer.android.com/guide/topics/permissions/defining) and
set the `android:protectionLevel` of the permission to `"signature"` or
`"signatureOrSystem"` to restrict access to just components signed by the same
certificate (or trusted system components).

## Outbound Intents {#outbound-intents}

There are [two types of Intents](https://developer.android.com/guide/components/intents-filters?hl=en#Types):
implicit and explicit. With implicit Intents, the receiving application is not
specified by the sender and the system uses a resolution process to find the
most suitable component to handle it. An implicit Intent can sometimes result in
a chooser being shown to the user when multiple applications could handle it.
Explicit Intents specify either the package name or a fully qualified
`ComponentName`, so the recipient is known at the time it is dispatched.
Implicit Intents can result in an unexpected (and maybe malicious) application
receiving user data. If it is possible to know the target application when
sending an Intent, always prefer using an explicit Intent.

## PendingIntents

A [PendingIntent](https://developer.android.com/reference/android/app/PendingIntent)
is created by one application and vended to another. The object allows the
receiving application to start the component (i.e. Activity, Service, Broadcast)
_as if the creating application started it_. Similar to a [setuid binary](https://en.wikipedia.org/wiki/Setuid),
you must use this with care, as it can even be used to start non-exported
components of the creating application.

It is possible to retrieve information about the creator package of the
PendingIntent using the [`getCreatorPackage()`](https://developer.android.com/reference/android/app/PendingIntent.html#getCreatorPackage())
method. This is the identity under which the Intent, which the PendingIntent
represents, will be started. Note that you cannot retrieve specific information
about the Intent (e.g. its target and extras). And as discussed above with
Intents, it is not possible to determine the application that called
`PendingIntent.send()`.

## Binder

[Binder](https://developer.android.com/reference/android/os/Binder) is the low
level IPC mechanism on Android, and it is what Intents and other Framework-level
primitives are built upon.

### Bound Services

To communicate between components using Binder, you declare a `<service>` in
your manifest and connect to it using [`Context.bindService()`](https://developer.android.com/reference/android/content/Context.html#bindService(android.content.Intent,%2520android.content.ServiceConnection,%2520int)).
This is referred to a as a [bound service](https://developer.android.com/guide/components/bound-services).

One of the powerful properties of a bound service is that you can determine the
identity of your communicating peer. This can only be done during a Binder
transaction (e.g. in an [AIDL](https://developer.android.com/guide/components/aidl)
method implementation or a [`Handler.Callback`](https://developer.android.com/reference/android/os/Handler.Callback.html))
that is **not** marked [`FLAG_ONEWAY`](https://developer.android.com/reference/android/os/IBinder).
During the transaction use [`Binder.getCallingUid()`](https://developer.android.com/reference/android/os/Binder.html#getCallingUid())
to retrieve the package's UID.

In Android, every installed application is given a unique user ID (UID). This
can be used as a key to query the [PackageManager](https://developer.android.com/reference/android/content/pm/PackageManager),
to retrieve the [PackageInfo](https://developer.android.com/reference/android/content/pm/PackageInfo)
for the application. With the PackageInfo, information about the applications
code signing certificates can be retrieved and cryptographically authenticated.
This is a strong authentication check and it is the **only** reliable mechanism
by which you can authenticate your peer.

In Chrome, the helper functions
[`ExternalAuthUtils.isCallerValid()`](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/externalauth/ExternalAuthUtils.java?l=157&rcl=fa790f69ce80bf2e192d710ea08b8343cad93fbb)
and `isCallerValidForPackage()` can perform these checks for you.

## Capability Tokens {#capability-tokens}

We define a **capability token** to be an unforgeable object that the holder may
present to another application as authentication to access a specific
capability. Binder objects are backed by the kernel (i.e. are unforgeable), are
transferable, and are comparable using `isEqual()`, so Binders can be used as
capability tokens.

One security factor to bear in mind is that because capability tokens are
transferable, they do not strongly authenticate a caller's identity. One
application may deliberately or accidentally transfer a capability token to
another application, or a token could be exfiltrated via an application logic
vulnerability. Therefore, only use capability tokens for access control, not
identity authentication.

While noting the above factor, capability tokens can be useful for
authenticating Intents. If two applications have established a Binder
connection, they can use the channel to exchange a capability token. One
application constructs a generic Binder (using the
[`Binder(String)`](https://developer.android.com/reference/android/os/Binder.html#Binder(java.lang.String))
constructor) and sends the object over that `ServiceConnection` to the other
application, while retaining a reference to it.

The generic Binder object can then be transmitted as an Intent extra when
sending Intents between the two applications. By comparing the object with
`Binder.isEqual()`, you can validate the capability token. Be sure to use an
[explicit Intent](#outbound-intents) when sending such an Intent.

This same approach can also be done with using a PendingIntent to a non-exported
component as a capability token. Internally PendingIntents use a Binder token
approach, so the only significant difference is the additional capability
conferred by the PendingIntent to start a component.
