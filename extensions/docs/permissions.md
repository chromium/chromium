# Extension Permissions

[TOC]

## Summary

Extension Permissions are used to determine which capabilities are exposed to a
given extension.  They are specified in the extension’s manifest.json file.

This document describes the internal implementation of extension permissions.
For more general extension permission information, see the public documentation
on [declaring permissions](https://developer.chrome.com/extensions/declare_permissions)
and [permission warnings](https://developer.chrome.com/extensions/permission_warnings).

## Types of Permissions
There are three basic classes of permissions.

### API Permissions
API permissions are typically used to grant an extension access
to a specific API.  These are usually specified by a reserved string (like the
API name).  Examples include `tabs`, `cookies`, `storage`, and others.

### Match Patterns
Match patterns provide access to a set of urls.  Match patterns can encompass
multiple hosts, so a single pattern may provide access to many different domains
(or even, every domain).  For more information, see the public
[match pattern documentation](https://developer.chrome.com/extensions/match_patterns).
In the codebase, match patterns are implemented as
[URLPattern](https://chromium.googlesource.com/chromium/src/+/main/extensions/common/url_pattern.h).

#### Explicit Hosts
Explicit host permissions are match patterns specified in the `permissions` key
of the extension’s manifest.  These patterns control access to APIs that read or
modify host data, such as `cookies`, `webRequest`, and `tabs.executeScript`.

Note: Any path component is currently ignored for explicit hosts in permissions
parsing.

#### Scriptable Hosts
Scriptable hosts are match patterns used in the content\_scripts
entry of the extension’s manifest.  These only control which sites an extension
can use content scripts on, and do not affect any other API.

From a messaging standpoint, we treat both explicit hosts and scriptable hosts
the same (with each allowing the extension to read and modify data on one or
more domains).  The distinction is only to provide granular permissions in order
to restrict what we provide the extension.

### Manifest Permissions
A manifest permission is a specified manifest entry that provides an extension
with some permission.  Examples of this include page overrides (to allow
extensions to override a page, like the New Tab Page), externally\_connectable
(allowing extensions to communicate with websites or extensions), and bluetooth
(allowing the extension to communicate with specific bluetooth devices).
Manifest permissions are effectively similar to API permissions in that they
provide access to a certain capability.  From an implementation standpoint,
permissions can often be implemented as either an API Permission or a Manifest
Permission.  For the rest of this document, we group both API and manifest
permissions into "API permissions".

## Storing Permissions
Permissions are stored in the Preferences file, under the extension’s entry.  We
always store two different sets of permissions: granted permissions and active
permissions (with the runtime host permissions feature, we will store a third,
runtime granted permissions - this is described below).  Note that neither of
these include any [non-persistent permissions](#non_persistent-permissions).

### Granted Permissions
Granted permissions are the permissions that the extension has ever been granted
by the user (and have not been revoked by the user).  When the user installs an
extension, we grant the accepted permissions.  If the extension is granted more
permissions at any point, we update the set of granted permissions to include
those.  Extensions can be granted additional permissions through either an
extension update that requires new permissions (assuming the user agrees) or
through the permissions API (which is used for optional permissions).  Granted
permissions can be thought of as the maximum level of permissions an extension
may have.

### Active Permissions
Active permissions are the permissions that an extension currently has.  This
set is used to initialize extension permission state when the extension is
loaded, and determines the APIs and domains an extension can access.

### Differences
The sets of active permissions and granted permissions can differ in a few
scenarios.
* **Extension Versions:** Consider a scenario in which version 1 of an
extension requires permission "foo", and version 2 of the extension removes
permission "foo".  If the user installed version 1, then they granted the
extension permission "foo" (at installation).  When the user is upgraded to
version 2, the extension removes the permission, and so the active permissions
change (reflecting the permissions the extension currently has).  However, the
granted permissions do not change, and still include "foo".
* **Optional Permissions:** Extensions can use the permissions API to request
optional permissions.  When an extension first requests a permission (using
`chrome.permissions.request()`), the user is prompted to accept or refuse it.
If the user accepts, the permission is added to both the active and granted
permissions.  The extension can then remove this permission using
`chrome.permissions.remove()`.  Removing the permission removes it from the
active permissions (reducing the extension’s immediate capabilities), but does
not remove it from the granted permissions.  If the extension uses
`chrome.permissions.request()` for the same permission after removing it, the
user will not be prompted again because the permission is present in the granted
permissions.

## Non-Persistent Permissions
Some permissions are granted ephemerally, and are not stored or persisted. These
are tab-specific permissions in the code, and are used with the `activeTab`
permission. This provides the extension with temporary access to a tab, so
that it can use `chrome.tabs.executeScript()`, the `webRequest` API, see the
tab's URL and favicon with the `tabs` API, and capture the screen with the
`chrome.tabs.captureTab()` function. When the user closes or navigates the tab,
the permission is revoked.

## Permission Increases
When an extension is updated to a new version, the new version can include new
permissions.  Chrome will disable the extension if it is detected that the new
version contains higher privilege than what is currently granted to the
extension (see also
[Determining Privilege Increase](#determining-privilege-increase)).  Note that
this compares the new set of requested permissions to the granted permissions,
rather than the active permissions for the extension.  This distinction is
important when the granted set is different than the active set. Consider the
two following cases:
* An extension has permission "foo" in version 1, removes it in version 2, and
  re-adds it in version 3.  This extension will not be disabled for any users
  that installed the extension at version 1, because those users already granted
  the extension permission "foo".  The extension will be disabled for any users
  who only installed version 2 (and not version 1) of the extension, since this
  version did not include the "foo" permission.
* An extension that has been granted permission "foo" through the permissions
  API and then adds permission "foo" as a required permission in an update.
  This extension will not be disabled for users who granted the permission, and
  will be disabled for any users that did not grant permission "foo".

## Determining Permission Warnings
Most permissions have an associated warning string, which describes what the
permission allows the extension to do.  Permission warnings are shown to the
user at a number of different times:
* Installation
* Requesting optional permissions
* Updating to a new version that requires more permissions
* Viewing an installed extension’s details

In order to maximize the benefit to users, Chrome tries to show as few warnings
as possible, while ensuring they are still accurate.

### Messageless Permissions
Some permissions (like the `storage` API) are considered relatively harmless,
and will not generate a permission warning.

### Permission Collapsing
Certain permissions "contain" or imply other permissions.
With host permissions, this is easy to understand: if an extension has access to
all google.com sites (i.e., `*://*.google.com/*`), requesting access to a
specific google.com site (`https://maps.google.com/*`) is redundant.  We should
not warn the user that the extension wants to both "access all google.com sites"
and "access maps.google.com".

API permissions can be similarly redundant, if one API provides the same or more
privilege than another.  For instance, if an extension has access to a user’s
history, then it implicitly has access to the user’s top sites.  Thus, if an
extension requests both the `history` permission and the `topSites` permission,
we will only display a single warning to the user.

### Determining Privilege Increase
Not all permission changes result in disabling the extension.  In order to
calculate if an extension has escalated privilege and should be disabled, Chrome
compares the permission messages that are associated with the granted
permissions and the newly-requested permissions.  This is different than
comparing the permissions themselves because of messageless permissions and
permission collapsing.

This has implications when determining privilege increase.  If an extension
adds new permissions but all of those permissions are either a) messageless or
b) collapsed into already-granted permissions, then the extension is not
disabled.  Otherwise, Chrome considers it a privilege increase, disables the
extension, and notifies the user (allowing them to accept the new permissions
and re-enable it, if they want).

Note: since optional permissions are not granted automatically, adding optional
permissions in a new version will never disable the extension.

## Sync
Granted permissions are not currently synced.  Instead, we sync the disable
reasons for an extension (which can include that an extension was disabled for
a permissions increase) and the extension version.  In most cases, this allows
users to approve an extension only once and have it enabled on all devices.
When the extension is updated to a new version, if it has increased permissions,
it will be disabled on the device.  The user can then approve the new
permissions, which enables the extension.  When the user goes to a new device,
the extension will be updated, and Chrome will notice the increased permissions,
but since the extension has been approved in sync, it will not be disabled.

### Known Issues
There are two known issues related to syncing extensions:
* If the extension is updated and approved on device A, and then updated on
  device B before sync is applied, it can be disabled on device B for a period
  of time (until sync finishes applying). This can result in a temporary,
  user-visible error, which then disappears.
* Consider the scenario in which an extension is installed at version 1 on
  device A.  The user doesn't sign into device A for a period of time, and
  version 2 of the extension is published (but never installed on device A).
  The user then signs into device B.  Version 2 of the extension will be
  installed (because it is the latest version pulled from the store); however,
  sync only knows that version 1 has been approved (and doesn't know about
  version 2). Chrome will disable version 2 of the extension on device B, *even
  if no new permissions are added*. This is because Chrome cannot know whether
  version 2 added new permissions or not, and behaves conservatively. Syncing
  granted permissions would solve this issue. See https://crbug.com/809797.

## Runtime Host Permissions
Runtime host permissions is a new feature that allows users to have more control
over when and where their extensions run. This allows the user to install an
extension that requests permission to multiple hosts, but have a choice over if
it runs.

The runtime host permissions feature introduces a number of permissions-related
concepts.

### Withheld Permissions
Withheld permissions are stored on the Extension object (as part of the
PermissionsData class). Withheld permissions are the set of permissions that
were listed as required permissions by the extension, but were withheld as part
of the runtime host permissions feature. The extension does not have access
to these permissions.

### Runtime-Granted Permissions
Runtime-granted permissions are stored within extension preferences, and
represent the permissions that the user has granted the extension at runtime.
This is deliberately kept as a separate set from granted permissions in order
to ensure that experimentation with the feature is independent, and will not
affect extensions when the feature is disabled.

Runtime-granted permissions include permissions granted through dedicated UI
for the runtime host permissions feature (such as context menu controls and
controls in the chrome://extensions page) as well as optional permissions
granted through the `permissions` API.

The controls for granting runtime permissions allow granting permissions beyond
what the extension specifically requests. This is so that the user can grant
`https://google.com/*`, even if the extension only requested
`https://google.com/maps`. This is useful for two reasons. First, it presents
a simpler UI to the user (who doesn't need to worry about granting exactly the
correct URL pattern); secondly, it means if the extension later requests
an additional pattern within the same host, it will be automatically granted.

However, though the runtime-granted permissions may extend beyond what is
explicitly requested, the current permissions on the extension object itself
(or granted to the extension process) should not. This provides us increased
security, since we don't want to have extensions privileged beyond what they
should need.

### Calculating Current Permissions
With the runtime host permissions feature, calculating current permissions is
a little more complex. Typically, an extension's current permissions are
calculated as

```
current_permissions =
    intersection(active_permissions,
                 union(required_permissions, optional_permissions))
```

Said differently, an extension's permissions are equal to any permissions in
the active permission set that also appear in either the required or optional
sets. This ensures that the extension never has access to permissions that it
didn't request, which is important for security reasons (we don't want an
over-privileged process if we can avoid it).

With runtime host permissions, this calculation is a little more difficult:

```
current_permissions_without_feature =
    intersection(active_permissions,
                 union(required_permissions, optional_permissions))
current_permissions =
    intersection(current_permissions_without_feature,
                 runtime_granted_permissions)
```

The system will withhold permissions that were not within the set of
runtime-granted permissions. As noted above, the runtime-granted permissions
may include more than what was explicitly requested by the extension; however,
we will not extend these permissions to the extension object because these
additional permissions will not be present on in the required or optional
permissions.

### Permissions Intersections
With runtime host permissions, it's possible that the user will grant a host
permission that overlaps with a requested host, but is neither a direct match
nor a strict subset. For instance, an extension may request the pattern
`*://google.com/maps`, and a user may grant `https://*.google.com/*`. In this
case, we should neither grant `*://google.com/maps` (which includes origins the
user did not approve, nor grant `https://*.google.com/*` (which includes more
than the extension requires). Instead, we should grant the extension
`https://google.com/maps` - the intersection of the granted permission and the
requested permission.

We perform this calculation with runtime host permissions. This has the
implication that permissions granted to the extension object may not be
explicitly present within the extension's required or optional permissions, but
rather contained by one of those sets.
