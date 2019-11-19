# Service Worker Security FAQ

[TOC]

This FAQ is specifically about service workers. Also see the [general security
FAQ](faq.md).

Like the general security FAQ, this document is a collaborative effort by many
Chromium developers. (rsesek, estark, falken, slightlyoff, jakearchibald, evn,
raymes, ainslie, mek, lgarron, elawrence, kinuko, palmer, your name here...)
Last updated 12 May 2017. If you see an error or have an additional question,
and have a Chromium account, go ahead and fix it. If you don't have a Chromium
account, email security-dev@chromium.org for a fix.

## Service Workers seem extremely risky! Why are they OK?

Service Workers (SW) are indeed powerful. They support compelling web
applications that can run offline or with intermittent connectivity. You can
edit documents, browse and buy from catalogs, send social media messages, write
email, etc. even in the subway! Service Workers can make the web platform more
viable than ever before, enabling web apps to better compete with native apps
even while essentially retaining the browse-to-use, sandboxed nature of the
[Open Web Platform](https://www.w3.org/wiki/Open_Web_Platform) (OWP) that we all
love. The rest of this FAQ will explain how the SW designers and implementers
have mitigated the risks that necessarily come with this functionality.

Service Workers are a replacement for and an improvement on the legacy
[Application Cache
API](https://developer.mozilla.org/en-US/docs/Web/HTML/Using_the_application_cache),
which has been available in the OWP for a very long time.

For more background on Service Workers, see [Service Workers
Explained](https://github.com/w3c/ServiceWorker/blob/master/explainer.md).

## Do Service Workers run in a sandbox?

Yes, SWs run in renderer processes. When Chrome starts a SW, it chooses a
renderer process that is associated with the SW’s origin. If one does not exist,
the browser creates a new one using a new
[`SiteInstance`](https://cs.chromium.org/chromium/src/content/public/browser/site_instance.h)
for the origin.

## What APIs can Service Workers access?

The [HTML specification partially enumerates the API surface available to
Workers](https://html.spec.whatwg.org/#apis-available-to-workers). See also
[`Client`](https://developer.mozilla.org/en-US/docs/Web/API/Client), and
[`ServiceWorkerGlobalScope`](https://w3c.github.io/ServiceWorker/#serviceworkerglobalscope-interface).
(Note that SWs do not have access to synchronous APIs.)

However, other web platform specifications can add new API surface. For example,
the Permissions API exposes a permissions attribute to workers. Generally, SWs
have access to a subset of the web platform APIs, although there are some
Worker- and Service Worker-specific APIs that do not make sense for in-page
JavaScript.

(`[Service]WorkerGlobalScope` is of course not necessarily a strict subset of
`Window`, and similarly `WorkerNavigator` is not necessarily a strict subset of
`Navigator`. And the various SW events are of course only exposed to SWs.)

## Do Service Workers obey the same-origin policy?

Service Worker registration specifies that [Service Workers must run in the same
origin as their
callers](https://w3c.github.io/ServiceWorker/#register-algorithm).

The origin comparison for finding a Service Worker registration for a request is
[specified](https://w3c.github.io/ServiceWorker/#scope-match-algorithm) to be to
be a longest-prefix match of serialized URLs, including their path. (E.g.
`https://example.com/` != `https://example.com.evil.com/`.) This specification
gap seems fragile to us, [and should be fixed to be specified and implemented as
actual origin equality](https://github.com/w3c/ServiceWorker/issues/1118), but
doesn’t currently seem exploitable.

Only [Secure Contexts can register or use Service
Workers](https://w3c.github.io/webappsec-secure-contexts/#example-c52936fc).

Because SWs can call `importScripts` to import scripts (from any other origin),
it is a good idea for site operators to set a Content-Security-Policy response
header on the ServiceWorker’s JavaScript response, instructing the browser what
sources of script the origin considers trustworthy. That would reduce an XSS
attacker’s ability to pull in their own code.

## Do Service Workers live forever?

There are two concepts of “live” here. One is about the installed registration
and one is about the running Service Worker thread.

The installed registration lasts indefinitely, similar to origin-scoped storage
like
[IndexedDB](https://developer.mozilla.org/en-US/docs/Web/API/IndexedDB_API). The
browser performs an update check after any navigation using the Service Worker,
invalidating the HTTP cache every 24 hours. Additionally, [browsers will
revalidate the HTTP cache for SW
scripts](https://w3c.github.io/ServiceWorker/#dfn-use-cache) unless the site
opts into using the cache.

The browser also performs an update check whenever the SW starts and
periodically while the worker is running, if it has not checked in the last 24
hours (86,400 seconds, [as specified in the Handle Functional Event
algorithm](https://w3c.github.io/ServiceWorker/#handle-functional-event-algorithm)).

The browser can terminate a running SW thread at almost any time. Chrome
terminates a SW if the SW has been idle for 30 seconds. Chrome also detects
long-running workers and terminates them. It does this if an event takes more
than 5 minutes to settle, or if the worker is busy running synchronous
JavaScript and does not respond to a ping within 30 seconds. When a SW is not
running, Developer Tools and chrome://serviceworker-internals show its status as
STOPPED.

## How can I see Service Workers in Chrome?

You can see them in the **Service Workers** field in the **Application** tab of
**Developer Tools**. You can also look at
[chrome://serviceworker-internals](chrome://serviceworker-internals).

## Do Service Workers keep running after I close the tab?

If an origin has any Service Workers running, each worker will be shut down soon
after it processes the last event. Events that can keep a worker alive include
push notifications. (Note that the [push notifications will trigger a
user-visible notification if the SW does not create
one](https://cs.chromium.org/chromium/src/chrome/browser/push_messaging/push_messaging_notification_manager.cc?type=cs&l=270),
and they also require the person to grant the origin permission in a prompt. you
can see that in action in this [push notifications demo
app](https://gauntface.github.io/simple-push-demo/).)

## Can attackers use Service Workers to trigger attacks developed after SW registration?

For example, could an attacker convince users to visit a malicious website, then
wait for (e.g.) a V8 bug to show up in Chrome's repository, then write an
exploit, and then somehow run that exploit on the machines of everyone who
visited the malicious website in the last month or so?

Without explicit permission from the user, the browser won't let the SW poll
for/receive any push notification events the attacker's server may (try to)
send, and hence the SW won't get a chance to handle the events.

Similarly, you might imagine a SW that tries to use `importScripts` to
periodically (re-)load `maybe-v8-payload.js`. But, the SW would only get to do
that as part of an event handler. And if the SW isn't getting any events
(because the person is not browsing or navigating to attacker.com, and because
the person never granted attacker.com push notification permission), then it
will never get to run its event handlers, and so again the SW won't get a chance
to attack.

If the person is currently browsing attacker.com, then the attacker doesn't gain
any additional attack benefit from a Service Worker. They can just `<script
src="maybe-v8-payload.js">` as usual, from the in-page JavaScript.

## If a site has an XSS vulnerability, can the attacker permanently compromise that origin for me?

An XSS attacker can indeed register an evil SW. As before SWs, XSS is a very
powerful mode of attack on a web origin. To mitigate the risk that an XSS attack
will register a malicious SW, the browser requires that the SW registration URL
come from the origin itself. Thus, to use an XSS attack to register a malicious
SW, the attacker needs the additional capability to host their own scripts on
the server.

Here is another exploit scenario: If the page with an XSS vulnerability also has
a JSONP endpoint, the attacker could use it to (1) bypass CSP; (2) register a
SW; and (3) call `importScripts` to import a third-party script to persist until

*    the site operators detect and remediates the issue; and
*    users navigate to the site again while online.

In an XSS situation, the 24 hour cache directive limit ensures that a malicious
or compromised SW will outlive a fix to the XSS vulnerability by a maximum of 24
hours (assuming the client is online). Site operators can shrink the window of
vulnerability by setting lower TTLs on SW scripts. We also encourage developers
to [build a kill-switch
SW](https://stackoverflow.com/questions/33986976/how-can-i-remove-a-buggy-service-worker-or-implement-a-kill-switch/38980776#38980776).

The right cleanup strategy (for this and other issues) is
[Clear-Site-Data](https://www.w3.org/TR/clear-site-data/).

Additionally, site operators should ignore (e.g. respond with `400 Bad Request`)
requests that have the Service-Worker request header for domains or paths that
the server doesn’t expect to be serving SW scripts for.

## Can sites opt out of Service Workers?

Sites that do not intend to serve Service Workers on particular domains or paths
can check for and explicitly reject requests for worker scripts, by checking for
[the Service-Worker request
header](https://w3c.github.io/ServiceWorker/#service-worker-script-request).

## How many SWs can an origin, or Chrome itself, spawn?

The current specification and the current implementation in Chrome do not define
any limits.

## Can attackers 'hide' Service Worker scripts in JPEGs or other non-script MIME types?

Can an attacker upload (for example) JPEG files to a site that supports the
capability, and then use the uploaded files as SW scripts?

The SW specification and implementation require that a SW script have the right
JavaScript MIME type. (Additionally, as noted elsewhere in this document, server
operators should reject SW script requests except for the exact endopints they
intend to serve SW scripts from.)

## Can iframes register Service Workers?

Yes, if and only if they are themselves secure contexts. [By
definition](https://w3c.github.io/webappsec-secure-contexts/#examples-framed),
that means that they must be nested inside secure contexts, all the way up to
the top-level document.

Additionally, third-party iframes can’t register Service Workers if third party
cookies are blocked. (See chrome://settings/content.)

## Why doesn’t Chrome prompt the user before registering a Service Worker?

The Chrome Team generally prefers to ask people about things that are
privacy-relevant, using nouns and verbs that are simple and precise (camera,
mic, geo-location, and so on). But we avoid asking questions about resource-use
(caching, persistence, CPU, and so on). We’re better prepared to make those
types of resource decisions automatically. (Consider, for example, that the HTTP
cache, AppCache, and even [Google
Gears](https://en.wikipedia.org/wiki/Gears_(software)) also do not/did not
prompt the user.)

[An informal study by Chrome team members Rebecca Rolfe, Ben Wells, and Raymes
Khoury](https://docs.google.com/presentation/d/1suzMhtvMtA11jxPUdH1jL1oPh-82rTymCnslgR3ehEE/edit#slide=id.p)
suggests that people do not generally have sufficient context to understand
permission requests triggered by API calls from origins in iframes. It seems
reasonable that people would similarly lack the context to understand requests
from Service Workers.

## What if I don't want *any* SWs?

You can disable SWs by disabling storage in chrome://settings. SW are gated on
cookie/local data storage settings. (That is, the **Block sites from setting any
data** radio button in **Content Settings**.)

Clearing browser data (CBD; the **Clear browsing data...** button in
**Settings** or chrome://settings/clearBrowserData) also deletes SWs. You can
verify that by following this test procedure:

1. Visit https://gauntface.github.io/simple-push-demo/
1. In a second tab, visit chrome://serviceworker-internals/ to see the ACTIVATED
   and RUNNING SW
   *    Note that the origin/the origin's SW cannot actually send any push notifications
        until you grant it that permission
1. In a third tab, go to chrome://settings/clearBrowserData to clear browsing data;
   clear it by clicking **Clear browsing data**
1. Reload chrome://serviceworker-internals/ to see that the SW's status is now
   REDUNDANT and STOPPED
1. Close the Simple Push Demo tab
1. Reload chrome://serviceworker-internals/ to see that the SW is now gone

You can also remove individual SW registrations with
chrome://serviceworker-internals/.

Another way to avid SWs is to use one of the browsers that don't (yet) support
SWs. But, eventually, the Open Web Platform will continue to evolve into a
powerful, useful platform supporting applications that are [secure, linkable,
indexable, composable, and ephemeral](https://paul.kinlan.me/slice-the-web/).
Yes, SWs make web apps somewhat less ephemeral, but we believe the increased
applicability of the OWP is worth it.

Browser vendors are committed to ensuring the security of the OWP improves even
as we give it new capabilities. This process happens in the open, in fora like
[W3C Technical Architecture Group](https://www.w3.org/2001/tag/), [W3C’s Web
Platform Incubator Community Group](https://www.w3.org/blog/2015/07/wicg/), and
[blink-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/blink-dev).
Security and privacy reviews are part of the process and we invite knowledgeable
experts to participate in those open fora.

## What are some SW best practices for site operators?

*    [Build a kill-switch SW](https://stackoverflow.com/questions/33986976/how-can-i-remove-a-buggy-service-worker-or-implement-a-kill-switch/38980776#38980776).
*    Use [Clear-Site-Data](https://www.w3.org/TR/clear-site-data/).
*    Be aware of the need for longer session lifetimes, since clients may go
     offline and SWs might need to POST cached requests after coming back
     online. [Here is one way to handle
     that](https://developers.google.com/web/updates/2016/06/2-cookie-handoff).

## What SW bugs would quality for a bounty under [Chrome’s VRP](https://www.google.com/about/appsecurity/chrome-rewards/index.html)?

If you could break one or more of the security assertions we make in this FAQ,
that would be potentially rewardable under the Vulnerability Rewards Program
(VRP). Here is a non-exhaustive list of examples:

*    Over-long registration/lifetime (e.g. a SW able to run or stay alive even
     without incoming events to handle)
*    Same-origin bypass or off-origin SW registration Access to APIs that
*    require prompts, choosers, or permissions, without
     permission having been granted to the origin
     *    Geolocation
     *    Hardware sensors
     *    Microphone, camera, media devices
     *    USB, Bluetooth

Here is [a list of historical SW security
bugs](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=Type%3DBug-Security+serviceworker&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids)
in Chromium’s bug tracker.

If you believe you have found a bug in the SW specification, please [file a new
Chromium bug using the Security
template](https://bugs.chromium.org/p/chromium/issues/entry?template=Security%20Bug).
It’s a good idea to file bugs with all browser vendors that implement the buggy
section of the spec.

If you believe you have found a bug in Chrome’s implementation of SW, please
[file a new bug using the Security
template](https://bugs.chromium.org/p/chromium/issues/entry?template=Security%20Bug).
The Chrome Security Team will triage it within 1 or 2 business days. Good bug
reports come with minimal test cases that demonstrate the problem!
