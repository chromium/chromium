# Telemetry Extension API

This document describes the Architecture for the third-party Telemetry
Extensions Platform.

[TOC]

# Overview

The Telemetry Extension Platform provides means for accessing Telemetry and
Diagnostics data of a ChromeOS device. The Telemetry Extension Platform
consists of 2 parts:

1. A Chrome headless extension for API access, background data gathering and
processing (Telemetry Extension)

2. A companion [Progressive Web App (PWA)](https://web.dev/progressive-web-apps/)
to show UI to a user.

The Telemetry Extension uses
[Manifest V3](https://developer.chrome.com/docs/extensions/mv3/intro/) to
benefit from using
[Service Workers](https://developer.chrome.com/docs/workbox/service-worker-overview/)
instead of a background script. Only when the PWA is open, the Telemetry
Extension periodically receives user driven requests from the PWA to fetch
telemetry and run diagnostics routines. The PWA also has its own launch icon
and it doesn’t have direct access to the telemetry and diagnostics API, but it
communicates with the Telemetry Extension using message passing.

This image shows an overview of the architecture.
<br>
<br>
![](images/TelemetryExtensionArchitecture.png)

# Components

## Telemetry Extension

The Telemetry Extension's role is to handle requests from the UI and gather
telemetry and diagnostics data. The Telemetry Extension has a service worker
and does not have a launch icon. For this specific situation a new Chrome
extension type was declared, the ChromeOS System extension. In a ChromeOS
System extension, the service worker has direct access to normal
[web APIs](https://developer.mozilla.org/en-US/docs/Web/API). Most of the
[common Chrome Extension APIs](https://developer.chrome.com/docs/extensions/reference/)
are disabled (besides `chrome.runtime` to communicate with the PWA) and access
to the Telemetry API is granted either directly through `os.` or via DPSL.js
the recommended way of using the API). Please note that each extension ID needs
to be allowlisted by Google in the Chrome codebase to have access to APIs.

## PWA

It is the role of the PWA to show UI to the user and communicate with the
Telemetry Extension to get telemetry and diagnostics data. The PWA has a
launch icon, UI, and access to web APIs.

## DPSL.js

DPSL.js stands for Diagnostic Processor Support Library for Javascript, it’s a
JS wrapper around the underlying Telemetry Extensions APIs. It offers an
abstracted way for querying the API and is supported and updated by Google to
always support the latest APIs. DPSL.js is Google’s recommended way to interact
with the telemetry extension APIs. The library is hosted on
[Github](https://github.com/GoogleChromeLabs/telemetry-support-extension-for-chromeos)
and published to [npm](https://www.npmjs.com/package/cros-dpsl-js), please refer
to the documentation.
[These tables](https://github.com/GoogleChromeLabs/telemetry-support-extension-for-chromeos/tree/main/src#functions)
show an overview of all available API functions.

## Communication

A Chrome extension and a PWA can communicate with each other using message
passing. Only PWAs can initiate communication because a PWA is running only
when the user launches it.
The following example shows how the communication between the two components
could look like. The PWA uses `chrome.runtime.sendMessage` to communicate with
the Chrome extension:

```javascript
// PWA code

// The ID of the extension we want to talk to.
var editorExtensionId = "abcdefghijklmnoabcdefhijklmnoabc";

// Make a simple request:
chrome.runtime.sendMessage(
  /* extensionId */ editorExtensionId,
  /* message in JSON */ {openUrlInEditor: url},
  /* callback */ function(response) {
    if (!response.success)
      handleError(url);
  });
```

The extension side can handle incoming requests as follows:

```javascript
// Extension code

chrome.runtime.onMessageExternal.addListener(
 function(request, sender, sendResponse) {
   if (sender.url == blocklistedWebsite)
     return;  // don't allow this web page access
   if (request.openUrlInEditor)
     openUrl(request.openUrlInEditor);
 });
```

# The Chrome extension

In order for a Chrome extension to have access to telemetry and diagnostics
APIs, the following requirements need to be satisfied:

1. The user must be either:

    a. managed and the Telemetry extension was force-installed via policy, or

    b. The user is the device owner (the first user of the device).

2. The PWA UI associated with the Telemetry extension must be opened for the
extension to have access to APIs.

3. The device hardware must belong to the OEM associated with the Telemetry
extension (e.g. HP Support Assist only runs on HP devices).

4. Only an allowlisted extension ID can access Telemetry Extension APIs. Each
allowlisted extension ID can be connected to one PWA origin. It is mandatory
to declare one entry in `externally_connectable.matches` list.
An example can be found here:
```json
"externally_connectable": {
 "matches": ["https://third-party.com/*"]
}
```

The `externally_connectable` key determines whether or not a website (or other
Chrome extension) can initiate communication with a Telemetry Extension. A
Telemetry Extension can always choose to communicate with sites indirectly via
content scripts injected into a tab. Please note that no matter what,
extensions should always validate and sanitize messages.  An extension should
never have a listener that evals random code sent in a message. This helps
mitigate the damage a sender (whether untrusted or compromised) may be able to
perform. This is also critical because other Chrome extensions may be running
on the site that tries to connect to the Telemetry Extension and can initiate
that connection.

# Development / Testing support

## Overriding PWA origin (Since M98) / Manufacturer (Since M105)

Support for overriding the PWA origin and manufacturer to use while development
and testing was added in version M98 and M105. The PWA origin option overrides
the allowlisted PWA origin while the manufacturer overrides the actual device
manufacturer fetched from cros_config. Here is what you need to do in order to
access the flags (you can skip to step 3 if you already have read/write access
to rootfs):

1. On your testing device, make sure you enable developer mode.

2. Make sure you have write access to rootfs, if not run `/usr/share/vboot/bin/make_dev_ssd.sh --remove_rootfs_verification`

3. Open `/etc/chrome_dev.conf` and add these lines to the end:
```
--telemetry-extension-pwa-origin-override-for-testing=<new-pwa-origin>
--telemetry-extension-manufacturer-override-for-testing=<new-manufacturer>
```

<ol start="4">
<li>In your Telemetry Extension's manifest.json, set the
externally_connectable.matches entry to match <new-pwa-origin>.</li>
</ol>

After following those steps, the new PWA and the Telemetry Extension should be
able to communicate via message passing.

## A Note on Serial Number

The device serial number can be accessed using `dpsl.telemetry.getVpdInfo()`
and the battery serial number is accessed using `dpsl.telemetry.getOemDataInfo()`.
Because of the sensitive nature of serial numbers, this information is guarded
using a runtime permission: `os.telemetry.serial_number`.
For technical limitation, this is now a mandatory permission, meaning the
chrome extension’s manifest.json needs to include it in the permissions list in
order to have access to serial numbers.

# FAQs

Q: I found a bug, how do I report it?<br>
A: Thank you for your contribution. Please create a new bug with the description
and logs (if possible) on our
[bugtracker](https://b.corp.google.com/components/1225577). You need a partner
account to do that.

Q: Have a question?<br>
A: Please reach out to cros-oem-services-team@google.com.
