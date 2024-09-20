# Extensions Security FAQ

[TOC]

## Purpose

This document outlines many common questions we've received about extensions
security and what we do or do not consider to be security bugs. This is
primarily written for an audience of Chromium developers and security
researchers.

It is analogous to the general
[Chrome Security FAQ](https://chromium.googlesource.com/chromium/src/+/main/docs/security/faq.md).

## Future Additions

Add canonical examples of bugs for each of these (particularly WontFix bugs for
examples of what is not considered a security bug).

## FAQ

### I've found / written an extension that can access sensitive user data, like passwords and emails. Is this a security bug in Chromium?

In most cases, this is _not_ a security bug. Extensions are designed to have
access to user data if and only if they have the appropriate
[permissions](https://developer.chrome.com/docs/extensions/mv3/declare_permissions/).
For instance, an extension may be able to access a user's data on a set of sites
(or all sites) if the extension requests the appropriate permissions in its
manifest file. Before an extension can use these capabilities, they must be
granted by the user - as an example, the install prompt may say that the
extension can "Read and change your data on google.com".

If the extension is only able to access user data through permissions the
extension has requested (for instance, it is able to access data on google.com
because it specified host or content script permissions that match google.com),
then this access is working as intended. If an extension is able to access data
_without_ appropriate permission, then this would be a security bug. Please
report any such bugs [here][new-security-bug].

### Why do you allow extensions to access sensitive user data? Surely there's no reason to allow it to read my passwords!

It may seem alarming that certain extensions can access certain types of
sensitive data, such as passwords. However, this access can be critical to
extensions' functionality. Consider, for instance, password managers (which
store and retrieve your passwords) - these extensions, fundamentally, must
access this type of sensitive data. There is very little data that we can
deterministically say should never be available to extensions.

When installing extensions, users should be sure to look both at the permissions
requested by the extension (displayed in the installation prompt) and also at
the extension's privacy policy and other disclosures (which may be linked from
the extension's listing on the
[Chrome Web Store](https://chrome.google.com/webstore)).

### If extensions can read sensitive user data, what prevents an extension from stealing my sensitive data?

First, extensions should only be allowed to access data that they have
permission to (and these permissions must be approved by the user before they
can be used, either at installation or at runtime). Users should only install
extensions and grant permissions to extensions they trust.

In addition to these platform mitigations, developers distributing through the
Chrome Web Store are required to adhere to a number of different policies, which
describe which types of behaviors are allowed. For instance, the
[user data](https://developer.chrome.com/docs/webstore/user_data/) FAQ here
describes the types of data and permissions that extensions are allowed to
gather and use.

Google also subjects extensions to a combination of automated and manual review
systems.

### I've found / written an extension that can access site data without any host patterns specified in the "permissions" or "host\_permissions" manifest keys. Is this a security bug?

In most cases, this is _not_ a security bug. Extensions can also access certain
sites with content scripts (specified in the `content_scripts` key) or with the
`activeTab` permission. Hosts specified in `content_scripts` (under the
`matches` key) are displayed to the user in permission requests in the same way
as host permissions requested under `permissions` or `host_permissions` - that
is, it indicates the extension can "Read and change your data on <site>".
ActiveTab requires a user to explicitly invoke the extension on a page before
the extension can access data on the page, which is a form of runtime permission
grant (similar to how "sharing" a page with an app on Android grants the app
access to the content of the page).

If an extension is able to access site data without any API or permission that
allows this access, then it may be a security bug. Please report any such bugs
[here][new-security-bug].

### Are extensions able to keep running after they have been uninstalled?

An extension is not able to directly keep running after it has been uninstalled
(or disabled) by the user or the browser. However, any changes made by an
extension to a currently loaded site (e.g. script injection or data
modification) will remain in place after the extension is uninstalled until the
user leaves the site (e.g. by navigating away or refreshing), and potentially
beyond. This has a few implications:

*   Exfiltrated data is still exfiltrated. If an extension sent data from the
    local device to a remote server, Chromium cannot delete the data on that
    remote server.
*   Content scripts (and other injected scripts) will continue running on pages
    they have already been injected in after the extension is disabled. Since
    Chromium cannot "un-inject" these scripts, they may continue running until
    the user leaves the site.
*   Data modified by the extension remains modified. This includes data like
    cookies, which may influence how a site operates. Additionally, extensions
    may have mutated the cache, installed service workers on sites, or similar
    modifications. These will persist.

Other extension behavior, such as running background scripts, network handlers,
URL overrides, proxy settings, and preference modifications should be reverted
upon uninstallation.

### I've found an extension that violates Chrome Web Store policy. Is this considered a bug in Chromium?

Individual extensions are generally not considered a part of Chromium, so
extensions violating Chrome Web Store policies (including distribution policies)
are not considered security bugs in Chromium itself.

You can report the extension with the "Report abuse" link in the extension's
entry on the Chrome Web Store.
<!-- TODO(devlin): Link to
     https://support.google.com/chrome_webstore/answer/7508032?hl=en when we
     have a p-link for this. Otherwise, it violates
     https://crbug.com/679462. -->

### Is adding a malicious extension to a user's profile considered a security bug?

Chromium
[does not consider physically-local attacks to be security bugs][physically-local-attacks].
This includes attacks like loading extensions on a user's machine. As such,
attacks like loading extensions physically through loading an unpacked
extension in the chrome://extensions page, via malicious software that executes
outside of the Chromium browser, or updating enterprise policies to load
extensions are all not within Chromium's threat model.

Adding a malicious extension to the user's profile is only a security bug if you
find a way to add the extension without direct access to the user's profile and
bypassing the normal extension installation flow. For instance, if an extension
could be installed (without user consent) when the user visits a malicious site,
this would be considered a security bug. Please report any such bugs
[here][new-security-bug].

### I've found / written an extension that can make the browser unusable once installed (i.e. constantly reload pages, close every new window). Is this a security bug?

No. Annoyance extensions like this are treated similarly to [Denial of Service
issues][denial-of-service] rather than as security vulnerabilities.

### What is our stance on unpacked extensions?

Attacks that involve loading an unpacked extension are _typically_ not security
bugs. Two common approaches are:
* Loading the extension with the `--load-extension` commandline switch, and
* Persuading the user to load an unpacked extension by enabling developer
  mode in chrome://extensions and drag-and-dropping a file or click on
  "Load unpacked extension".

Loading an unpacked extension in these ways is not considered a security bug.
The former requires local access, which is
[not in our threat model][physically-local-attacks]. The latter is very
similar to persuading the user to open devtools and paste code, which is also
[not considered a security bug][devtools-execution] (in both these cases, the
user is allowing arbitrary untrusted JavaScript code to run via tools intended
for developers).

If you identify another way to load an unpacked extension, it may be considered
a security bug.

### Is accessing private APIs via unpacked extensions a security bug?

Some extension APIs are restricted to extensions with a specified ID. Developers
can give an unpacked extension a given ID by setting the "key" entry in the
manifest, as described [here][setting-unpacked-id]. This allows unpacked
extensions to access powerful APIs.

This is not considered a security bug ([example](https://crbug.com/1301966)).
See above for [our stance on unpacked extensions][unpacked-extensions-stance].
The addition of access to private APIs does not change this stance, as it is
still most similar to a physically-local attack or devtools execution in a
trusted context (for instance, inspecting a component extension allows access
to trusted APIs).

### Is spoofing an extension with its ID in an unpacked extension a security bug?

Developers can give an unpacked extension a given ID by setting the "key" entry
in the manifest, as described [here][setting-unpacked-id]. This would allow a
developer to imitate a legitimate extension and have access to its ID.

This is not considered a security bug. See above for
[our stance on unpacked extensions][unpacked-extensions-stance].  If the
extension is instead treated as being from the webstore (as opposed to an
unpacked extension), Chromium will validate the content of the extension.

### Chrome silently syncs extensions across devices. Is this a security vulnerability?

If an attacker has access to one of a victim‘s devices, the attacker can install
an extension which will be synced to the victim’s other sync-enabled devices.
Similarly, an attacker who phishes a victim‘s Google credentials can sign in to
Chrome as the victim and install an extension, which will be synced to the
victim’s other sync-enabled devices. Sync thereby enables an attacker to elevate
phished credentials or physical access to persistent access on all of a victim's
sync-enabled devices.

To mitigate this issue, Chrome only syncs extensions that have been installed
from the Chrome Web Store. Extensions in the Chrome Web Store are monitored for
abusive behavior.

In the future, we may pursue further mitigations. However, because an attacker
must already have the victim‘s Google credentials and/or
[physical access to a device][physically-local-attacks], we don’t consider this
attack a security vulnerability.

We **do** consider it a vulnerability if an attacker can get an extension to
sync to a victim‘s device without either of the above preconditions. For
example, we consider it a vulnerability if an attacker could craft a request to
Google’s sync servers without proper credentials that causes an extension to be
installed to a user's device, or if an attacker could entice a victim to visit a
webpage that causes an extension to be installed on their device(s). Please
report any such bugs [here][new-security-bug].

### Why do some permissions (and APIs) requested by developers not display a permission warning?

Permissions and APIs may not have an associated warning for a number of reasons.
There is not a 1-to-1 mapping of permissions listed in the manifest and warnings
shown to the user.

Some permissions may grant innocuous capabilities to extensions. For instance,
the `storage` permission allows an extension to store _its own data_. This does
not grant the extension access to any additional data; further, this type of
behavior is already possible using open web APIs like `localStorage` and Indexed
DB. Since this does not provide the extension any access to any new data or
dangerous capabilities, the API does not have an associated warning.

Other permissions, like `webRequest`, may only apply to sites the extension has
access to. The webRequest API allows extensions to monitor (and potentially
modify) network requests made by web pages. However, an extension can _only_
intercept these requests for sites it has access to. Requesting access to a site
already displays a permission warning ("Read and change your data on <site>");
the webRequest API does not provide any additional access. This is also the case
for the `scripting` permission and others.

Some APIs may also have runtime permission prompts, such as dialogs or choosers,
that are shown to the user in context. Since these APIs do not immediately grant
any data access to extensions, no permission warning is shown.

Finally, some permissions may be subsumed by other, more powerful, permissions.
For instance, if an extension has access to the history API (which allows
extensions to read and change a user's browsing history), we do not also show a
warning for the `topSites` API (which allows extensions to see the top sites a
user has visited). Since the history API is strictly more powerful, it subsumes
the `topSites` API. You can read more
[here](https://chromium.googlesource.com/chromium/src/+/main/extensions/docs/permissions.md#Determining-Permission-Warnings).

### Why do optional permissions not display a permission warning at install time?

Permissions listed in the `optional_permissions` key in the manifest are not
granted to the extension at install-time. Instead, they are granted through the
use of the
[Permissions API](https://developer.chrome.com/docs/extensions/reference/permissions/).
When the extension requests new capabilities (i.e., permissions that have not
been previously granted and are not superseded by other granted permissions), a
dialog is shown to the user, allowing them to grant or refuse the permissions.

### If an extension is updated to include new permissions, are users notified?

If an extension updates and includes new permissions that are not already
contained within its current granted capabilities, the extension is disabled on
users' machines and the user is notified (and asked if they'd like to grant the
new permissions and enable the extension). You can read more
[here](https://chromium.googlesource.com/chromium/src/+/main/extensions/docs/permissions.md#Permission-Increases).

This does not apply to newly-requested optional permissions, which are not
granted by default.

### I've found / written an extension that can execute code from a remote server. Is this a security bug?

This is not considered a security bug in Chromium
([example](https://crbug.com/1025017)). Prior to Manifest Version 3, extensions
were allowed to execute code that was not contained in the extension package
(also called remotely-hosted code); this is a pattern that is extremely common
in web development. Beginning in Manifest Version 3, all logic must be contained
within the extension package for developers distributing through the Chrome Web
store; however, this is a _policy_ requirement (described
[here](https://developer.chrome.com/docs/webstore/program_policies/)), and is
not enforced by the Chromium browser.

While the platform includes some restrictions through the inclusion of a default
content security policy for extensions, this is not meant to be a guaranteed
deterrent, and does not prevent all types of remote code execution. For
instance, it is impossible for the Chromium browser to guard against an
extension that includes an interpreter that processes remotely-fetched JSON
commands, even though this type of behavior is prohibited by policy in Manifest
V3.

### Why do we allow extensions to open or close chrome:-scheme pages?

Web pages with the chrome:-scheme (such as chrome://settings) are generally
protected from extensions - extensions are not allowed to read or change data
on these pages (without the use of the `--extensions-on-chrome-urls` command
line flag). However, extensions _are_ allowed to open and close these pages
through APIs like the tabs and windows APIs. This is critical for certain types
of extensions, such as tab and session managers, bookmark managers, and history
managers.

### Why do we not allow extensions to open or close chrome-untrusted:-scheme pages?

The chrome-untrusted:-scheme (such as chrome-untrusted://terminal) is generally
used for Chrome OS System Web Apps. Some of these apps such as Terminal which
starts the Linux VM can perform operations on startup, or start other systems
which may have security vulnerabilities. We
[intentionally](../../security/chromeos_security_whitepaper.md#principles-of-chrome-os-security)
disallow auto-start to avoid
[persistent attacks](https://chromium.googlesource.com/chromiumos/docs/+/HEAD/containers_and_vms.md#security-persistence).

### Why are extensions allowed to bypass a web page's Content Security Policy?

Extensions are considered more privileged than the web pages they are allowed to
run on. As such, they are allowed to circumvent restrictions put in place by
those web pages. This can be critical for extension functionality.

### Is executing in the main world of a document a security bug?

No ([example](https://bugs.chromium.org/p/chromium/issues/detail?id=760419)). By
default, extension scripts (like content scripts and those injected with
[tabs.executeScript()](https://developer.chrome.com/docs/extensions/reference/tabs/#method-executeScript)
or
[scripting.executeScript()](https://developer.chrome.com/docs/extensions/reference/scripting/#method-executeScript)
execute in an isolated world. However, this is _not_ intended as a security
boundary to protect the main page from the extension (rather, it is a soft
boundary in the other direction, _slightly_ protecting the extension from the
webpage and other extensions active on the page). More than anything else, this
is designed to prevent collision of JavaScript variables, so that `foo` in the
content script does not reference `foo` from the main world.

Extensions can trivially execute in the main world of a document if they so
desire - for instance, by appending a `<script>` element. Assuming the extension
has access to the site, it can already read and change the data on that site. As
such, injecting in the main world does not represent any increased capability or
access, and is not a security bug.

### Is a web page accessing content script data in an isolated world considered a security bug?

Isolated worlds are separate JavaScript execution contexts in V8. By default,
script in one world cannot access the data in another. Circumventing this _may_
be a security bug, depending on the attack.

**Process-based attacks:** Since isolated worlds are necessarily in the same
process as the main world, any attack that allows the attacker to read
process-level data (such as a Spectre or Meltdown attack, or other renderer
compromise) could potentially access content script data or access APIs exposed
to content scripts.

**Shared variable attacks:** Certain content is shared between the isolated
world and the main world. For instance, references to an undeclared variable can
result in accessing a frame with the same name from the window object.
Similarly, the content of the DOM is shared between different JS worlds (though
the JS objects are distinct to each). Attackers may be able to "trick" a content
script into using these variables as a flavor of XSS - an example of this is
[this Project Zero bug](https://bugs.chromium.org/p/project-zero/issues/detail?id=1225).
These are bugs in the particular extension, rather than in Chromium. Please
report them to the extension developer.

Other attacks that cross from the main world into an extension's isolated world
may be considered security bugs; please report any such bugs
[here][new-security-bug].

### Extensions can modify, relax, or remove site security by removing security-sensitive headers like Content-Security-Policy, HTTP Strict-Transport-Security, Access-Control-Allow-Origin, and others. Is this a security bug?

Assuming the extension has access to the site, this is not considered a security
bug. If the extension has access to a site, it can read and change all data
associated with that site, including affecting how it can be shared with other
parties.

However, modifying security-sensitive headers is generally discouraged, unless
absolutely necessary (such as for developer tools).

### An extension is able to open a native application. Is this a security bug?

_Maybe_. Extensions are allowed to open (and potentially communicate with)
native applications in a variety of ways. One of the main ways is the
[nativeMessaging API](https://developer.chrome.com/docs/extensions/nativeMessaging/),
which allows for communication with installed native applications that have
registered a manifest on the user's machine. Extensions are also able to open
native applications with the
[downloads.open()](https://developer.chrome.com/docs/extensions/reference/downloads/#method-open)
method. Finally, extensions are allowed to navigate to file:-scheme URLs, which
could, depending on the user's system configuration, result in execution. Using
these methods to open a native application is not considered a security bug.

If an extension is able to open a native application or execute native code in
another way, it may be a security bug; please report any such bugs
[here][new-security-bug].

### An extension is able to read file contents from the local machine. Is this a security bug?

Extensions can read file contents if they are granted file permission by the
user. This is toggled in the chrome://extensions page for the given extension
(under "Allow access to file URLs"; note that this is the default for unpacked
extensions). If this setting is enabled, extensions can read all files on disk.
Additionally, extensions are allowed to read any files that were explicitly
shared with them, such as through the HTML5 Filesystem API. Additionally,
extensions (with appropriate API permissions) can read the URLs of tabs and
history entries, including file:-scheme URLs; however, this should not allow
access to the _contents_ of the file.

If an extension is able to read file contents from the local machine in another
way, this may be a security bug; please report any such bugs
[here][new-security-bug].

### An extension is able to read and store data from incognito browsing. Is this a security bug?

Extensions are allowed to run in an incognito profile if the "Allow in
incognito" setting is enabled for the given extension in the
chrome://extensions page. In this scenario, the extension has access to all the
types of data it does in normal browsing - such as URLs and the contents of
websites. Chrome does not limit what an extension does with this data.

If an extension is able to access incognito contexts without this setting
enabled, this may be a security bug; please report any such bugs
[here][new-security-bug].

### What privileges does the Debugger permission grant an extension? What privileges should it lack?

The debugger permission should grant an extension the power to automate any
website. This may extend to driving interactions with that site which are not
possible using JavaScript on the site itself, but instead normally require
user interaction with Chrome features.

The debugger permission does not allow automating parts of the Chromium
browser unrelated to websites. Automating WebUI or settings, installing
extensions, downloading and executing a native binary, or executing custom
code outside the sandbox should not be possible for an extension with the
debugger permission.

### What security does the Native Messaging API provide?

The [Native Messaging API](https://developer.chrome.com/docs/extensions/develop/concepts/native-messaging)
is not a secure communication channel and if required, secure communication
between the extension and a native app must be established by the extension
developer with an additional transport layer.

The JSON files contained in the NativeMessagingHosts folder support an
`allowed_origins` key. Chrome will not allow an extension to communicate with a
host unless the extension ID is listed here. However, an extension from outside
of the Chrome Web Store can easily use an arbitrary ID with the [`key`](https://developer.chrome.com/docs/extensions/reference/manifest/key)
field, and other binaries on a machine could launch the Native Messaging Host
and communicate with it. Both of these are outside of Chrome's security model
which [does not consider physically-local attacks to be security bugs][physically-local-attacks].

### I've found a security bug in an extension. Is this a security bug in Chromium?

This depends on the extension.

**Component Extensions:**
[Component extensions](https://chromium.googlesource.com/chromium/src/+/main/extensions/docs/component_extensions.md)
are bundled by the Chromium browser and implement core browser functionality. If
you find a security bug in a component extension, this _is_ considered a
security bug in Chromium. Please report any such bugs [here][new-security-bug].

**Google Extensions:** A security bug in an extension developed by Google, but
not distributed directly with the Chrome browser, would likely not be considered
a bug in Chromium. However, they may be covered by the
[Google Vulnerability Reward Program](https://www.google.com/about/appsecurity/reward-program/).

**Other Extensions:** A security bug in a third-party extension _would not_ be
considered a security bug in Chromium. This is true even if the extension has
sensitive and powerful permissions, which could leak user data  or allow
cross-site scripting attacks
([example](https://bugs.chromium.org/p/chromium/issues/detail?id=1213523)).
Some third-party extensions may have their own vulnerability reward programs;
please check with the extension developer. It may also be eligible for a reward
through the Developer Data Protection Reward Program (though this typically
targets abuse, rather than vulnerabilities); visit
[this site](https://www.google.com/about/appsecurity/ddprp/) for more
information.

[new-security-bug]: https://bugs.chromium.org/p/chromium/issues/entry?template=Security+Bug
[physically-local-attacks]: https://chromium.googlesource.com/chromium/src/+/main/docs/security/faq.md#why-arent-physically_local-attacks-in-chromes-threat-model
[devtools-execution]: https://chromium.googlesource.com/chromium/src/+/main/docs/security/faq.md#Does-entering-JavaScript_URLs-in-the-URL-bar-or-running-script-in-the-developer-tools-mean-there_s-an-XSS-vulnerability
[setting-unpacked-id]: https://developer.chrome.com/docs/extensions/mv3/manifest/key/
[unpacked-extensions-stance]: https://chromium.googlesource.com/chromium/src/+/main/extensions/docs/security_faq.md#what-is-our-stance-on-unpacked-extensions
[denial-of-service]: https://chromium.googlesource.com/chromium/src/+/main/docs/security/faq.md#Are-denial-of-service-issues-considered-security-bugs
