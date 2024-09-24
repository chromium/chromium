# Process Model and Site Isolation

As the early Web matured, web sites evolved from simple documents to active
programs, changing the web browser's role from a simple document renderer to an
operating system for programs. Modern browsers like Chromium use multiple
operating system processes to manage this workload, improving stability,
security, and performance.

Chromium's **process model** determines how documents, workers, and other web
content are divided into processes. First, the process model must identify
which parts of a "program" on the web need to coexist in a single process.
Somewhat surprisingly, a program on the web is not a single document plus its
subresources, but rather a group of same (or similar) origin documents that can
fully access each other's contents. Once these atomic groups are defined, the
process model can then decide which groups will share a process. These
decisions can be tuned based on platform, available resources, etc, to achieve
the right level of isolation for different scenarios.

This document outlines the goals and design of Chromium's process model and the
various ways it is used today, including its support for Site Isolation.

[TOC]


## Goals

At a high level, Chromium aims to use separate processes for different instances
of web sites when possible. A **web site instance** is a group of documents or
workers that must share a process with each other to support their needs, such
as cross-document scripting. (This roughly corresponds to an "[agent
cluster](https://html.spec.whatwg.org/multipage/webappapis.html#integration-with-the-javascript-agent-cluster-formalism)"
from the HTML Standard, as described below.)

For stability, putting web site instances in separate processes limits the
impact of a renderer process crash or hang, allowing other content to continue
working. For performance, this allows different web site instances to run in
parallel with better responsiveness, at the cost of some memory overhead for
each process.

For security, strictly using separate processes for different web sites allows
significantly stronger defenses against malicious web sites. In addition to
running web content within a low-privilege
[sandbox](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/design/sandbox.md)
that limits an attacker's access to the user's machine, Chromium's
multi-process architecture can support [Site
Isolation](https://www.chromium.org/Home/chromium-security/site-isolation),
where each renderer process is only allowed to access data from a single site.
Site Isolation involves:

* **Locked Renderer Processes**: A renderer process can be limited to documents
    and workers from a single web site or origin, even if such documents are in
    iframes.
* **Browser-Enforced Restrictions**: The privileged browser process can monitor
    IPC messages from renderer processes to limit their actions or access to
    site data (e.g., using ChildProcessSecurityPolicy::CanAccessDataForOrigin).
    This [prevents compromised renderer
    processes](https://chromium.googlesource.com/chromium/src/+/main/docs/security/compromised-renderers.md)
    from asking for cross-site data, using permissions granted to other sites,
    etc. These restrictions take two main forms:
  * _"Jail" checks_: Ensure that a process locked to a particular site can only
      access data belonging to that site. If all processes are locked, this is
      sufficient protection.
  * _"Citadel" checks_: Ensure that unlocked processes cannot access data
      for sites that require a dedicated process. This adds protection in cases
      where full Site Isolation is not available, such as Android.
* **Network Response Limitations**: Chromium can ensure that locked renderer
    processes are only allowed to receive sensitive data (e.g., HTML, XML,
    JSON) from their designated site or origin, while still allowing
    cross-origin subresource requests (e.g., images, media) as needed for
    compatibility. This is achieved using [Cross-Origin Read
    Blocking](https://www.chromium.org/Home/chromium-security/corb-for-developers)
    (CORB) or [Opaque Response Blocking](https://github.com/annevk/orb) (ORB).


## Abstractions and Implementations

Chromium uses several abstractions to track which documents and workers need
synchronous access to each other, as a constraint for process model decisions.

* **Security Principal** (implemented by
    [SiteInfo](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/site_info.h;drc=c79153d6f931dbe2ce2c992962512eaca6766623;l=22)):
    In security terminology, a **principal** is an entity with certain
    privileges. Chromium associates a security principal with execution
    contexts (e.g., documents, workers) to track which data their process is
    allowed to access. This principal is typically a
    "[site](https://html.spec.whatwg.org/multipage/origin.html#site)" (i.e.,
    scheme plus eTLD+1, such as `https://example.com`), because web pages can
    modify their document.domain value to access other same-site documents, and
    not just same-origin documents. In some cases, though, the principal may be
    an origin or have a coarser granularity (e.g., `file:`). The SiteInfo class
    tracks all values that identify a security principal.

* **Principal Instance** (implemented by
    [SiteInstance](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/site_instance.h;drc=858df4ab8b73f2418f51385954760f2154512029;l=32)):
    A principal instance is the core unit of Chromium's process model. Any two
    documents with the same principal in the same browsing context group
    (see below) must live in the same process, because they have synchronous
    access to each other's content. This access includes cross-document
    scripting and synchronous communication through shared memory (e.g.,
    SharedArrayBuffer). If such documents were in different processes, data
    races or deadlocks would occur if they concurrently accessed objects in
    their shared DOM or JavaScript heaps.

    This roughly corresponds to the [agent
    cluster](https://html.spec.whatwg.org/multipage/webappapis.html#integration-with-the-javascript-agent-cluster-formalism)
    concept in the spec, although they do not match exactly: multiple agent
    clusters may sometimes share a principal instance (e.g., with `data:` URLs
    in the same principal instance as their creator), and principals may keep
    track of more factors than [agent cluster
    keys](https://html.spec.whatwg.org/multipage/webappapis.html#agent-cluster-key)
    (e.g., whether the StoragePartition differs).

    Note that the user may visit multiple instances of a given principal in the
    browser, sometimes in unrelated tabs (i.e., separate browsing context
    groups). These separate instances do not need synchronous access to each
    other and can safely run in separate processes.

* **Browsing Context Group** (implemented by
    [BrowsingInstance](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/browsing_instance.h;drc=df269acf8de952b68b2fbec49365457ff1f6266b;l=34)):
    A browsing context group is a group of tabs and frames (i.e., containers of
    documents) that have references to each other (e.g., frames within the same
    page, popups with window.opener references, etc). Any two documents within
    a browsing context group may find each other by name, so it is important
    that any same-principal documents in the group live in the same process. In
    other words, there is only one principal instance per principal in a given
    browsing context group. Note that a tab may change its browsing context
    group on some types of navigations (e.g., due to a
    Cross-Origin-Opener-Policy header, browser-initiated cross-site
    navigations, and other reasons).

From an implementation perspective, Chromium keeps track of the SiteInstance of
each RenderFrameHost, to determine which renderer process to use for the
RenderFrameHost's documents. SiteInstances are also tracked for workers, such
as ServiceWorker or SharedWorkerHost.


## Modes and Availability

### Full Site Isolation (site-per-process)

_Used on: Desktop platforms (Windows, Mac, Linux, ChromeOS)._

In (one-)site-per-process mode, each process is locked to documents from a
single site. Sites are defined as scheme plus eTLD+1, since different origins
within a given site may have synchronous access to each other if they each
modify their document.domain. This mode provides all sites protection against
compromised renderers and Spectre-like attacks, without breaking backwards
compatibility.

This mode can be enabled on Android using
`chrome://flags/#enable-site-per-process`.


### Partial Site Isolation

_Used on: Chrome for Android (2+ GB RAM)._

On platforms like Android with more significant resource constraints, Chromium
only uses dedicated (locked) processes for some sites, putting the rest in
unlocked processes that can be used for any web site. (Note that there is a
threshold of about 2 GB of device RAM required to support any level of Site
Isolation on Android.)

Locked processes are only allowed to access data from their own site. Unlocked
processes can generally access data from any site that does not require a
locked process. Chromium usually creates one unlocked process per browsing
context group.

Currently, several heuristics are used to isolate the sites that are most likely
to have user-specific information. As on all platforms, privileged pages like
WebUI are always isolated. Chromium also isolates sites that users tend to log
into in general, as well as sites on which a given user has entered a password,
logged in via an OAuth provider, or encountered a Cross-Origin-Opener-Policy
(COOP) header.


### No Site Isolation

_Used on: Low-memory Chrome for Android (<2 GB RAM), Android WebView, Chrome for
iOS._

On some platforms, Site Isolation is not available, due to implementation or
resource constraints.

* On Android devices with less than 2 GB of RAM, Site Isolation is disabled to
  avoid requiring multiple renderer processes in a given tab (for out-of-process
  iframes). Cross-process navigations in the main frame are still possible
  (e.g., for browser-initiated cross-site navigations with no other pages in the
  browsing context group, when a new browsing context group may be created).
* Android WebView does not yet support multiple renderer processes or
  out-of-process iframes.
* Chrome for iOS uses WebKit, which does not currently have support for
  out-of-process iframes or Site Isolation.


### Origin Isolation

_Available on: Desktop platforms, Chrome for Android (2+ GB RAM)._

There are several optional ways to lock processes at an origin granularity
rather than a site granularity, with various tradeoffs for compatibility
(e.g., breaking pages that modify document.domain). These are available on
platforms that support some level of Site Isolation.

* **Built-in**: //content embedders can designate particular origins that
    require isolation from the rest of their site, using
    ContentBrowserClient::GetOriginsRequiringDedicatedProcess.
* **Configurable**: Users and administrators can list particular origins that
    should be isolated from the rest of their site, using the command line
    (`--isolate-origins=`...), `chrome://flags#isolate-origins`, or
    [enterprise policy](https://support.google.com/chrome/a/answer/7581529)
    ([IsolateOrigins](https://chromeenterprise.google/policies/#IsolateOrigins)
    or
    [IsolateOriginsAndroid](https://chromeenterprise.google/policies/#IsolateOriginsAndroid)).
    It is also possible to isolate all origins (except those that opt-out) using
    `chrome://flags/#origin-keyed-processes-by-default`.
* **Opt-in**: The [Origin-Agent-Cluster](https://web.dev/origin-agent-cluster)
    HTTP response header can be used by web developers to hint to the browser
    that an origin locked process can be used. This is not a security guarantee
    and may not always be honored (e.g., to keep all same-origin documents
    consistent within a given browsing context group), though it allows finer
    grained isolation in the common case. Note that
    [Origin-Agent-Cluster is now enabled by default](https://github.com/mikewest/deprecating-document-domain),
    effectively disabling changes to document.domain unless an OAC opt-out
    header is used.


### CrossOriginIsolated

Certain powerful web platform features now require an opt-in
[CrossOriginIsolated](https://web.dev/coop-coep/) mode, which ensures that all
cross-origin content (e.g., documents and workers, as well as subresources like
media or scripts) consents to being loaded in the same process as an origin
using these features. This opt-in is required because these powerful features
(e.g., SharedArrayBuffers) can be used for very precise timing, which can make
attacks that leak data from the process (e.g., using Spectre or other transient
execution attacks) more effective. This mode is important because not all
browsers support out-of-process iframes for cross-origin documents, and not all
cross-origin subresources can be put in a separate process.

CrossOriginIsolated mode requires the main document to have
Cross-Origin-Opener-Policy and Cross-Origin-Embedder-Policy headers. These
headers impose restrictions on all content that may load within the page or
process (e.g., requiring similar headers on subframes, and CORS, CORP, or a
credentialless mode for subresources).


### Historical Modes

Before Site Isolation was introduced, Chromium initially supported a few other
process models that affected the number of renderer processes.

* **Process-per-site-instance**: This model was the default when Chromium first
    launched. It used a new process when navigating to a different site in some
    scenarios (e.g., via the address bar but not link clicks), as well as when
    visiting different instances of the same site in different tabs. At the
    time, cross-site subframes stayed in the same process as their parent
    frames.
* **Process-per-site**: This model consolidated all instances of a given site
    into a single process (per profile), to reduce the process count. It
    generally led to poor usability when a single process was used for too many
    tabs. This mode is still used for certain limited cases (e.g., the New Tab
    Page) to reduce the process count and process creation latency. It is also
    used for extensions to allow synchronous scripting from a background page.
    Note that having a single process for a site might not be guaranteed (e.g.,
    due to multiple profiles, or races).
* **Process-per-tab**: This model used a separate process for each browsing
    context group (i.e., possibly multiple related tabs), but did not attempt
    to switch processes on cross-site navigations. In practice, though, this
    model still needed to swap processes for privileged pages like `chrome://`
    URLs.
* **Single process**: Chromium also allows a single process model which runs all
    of the browser and renderer code in a single OS process. This is generally
    not a safe or robust process model, since it prevents the use of the
    sandbox and cannot survive any crash in renderer process code. It is mainly
    used for older low-resource Android WebView scenarios, and for debugging or
    testing.


## Visualizations

Chromium provides several ways to view the current state of the process model:

* **Chromium's Task Manager**: This can be found under "More Tools" in the menu,
    and shows live resource usage for each of Chromium's processes. The Task
    Manager also shows which documents and workers are grouped together in a
    given process: only the first row of a given group displays process ID and
    most statistics, and all rows of a group are highlighted when one is
    clicked. Note that double clicking any row attempts to switch to the tab it
    is associated with. In the default sort order (i.e., when clicking the Task
    column header until the up/down triangle disappears), processes for
    subframes are listed under the process for their tab when possible,
    although this may not be possible if subframes from multiple tabs are in a
    given process.
* **`chrome://process-internals/#web-contents`**: This is an internal diagnostic
    page which shows information about the SiteInstances and processes for each
    open document.
* **`chrome://discards/graph`**: This is an internal diagnostic page that
    includes a visualization of how the open documents and workers map to
    processes. Clicking on any node provides more details.


## Process Reuse

For performance, Chromium attempts to strike a balance between using more
processes to improve parallelism and using fewer processes to conserve memory.
There are some cases where a new process is always required (e.g., for a
cross-site page when Site Isolation is enabled), and other cases where
heuristics can determine whether to create a new process or reuse an old one.
Generally, process reuse can only happen in suitable cases, such as within a
given profile or respecting a process lock.  Several factors go into this
decision.

* **Suitability**: Several properties are global to a given renderer process:
    profile (including Incognito), StoragePartition (which may differ between
    tabs and Chrome Apps), and crossOriginIsolated status. For example, two
    documents from different profiles or StoragePartitions can never share the
    same renderer process. The ProcessLock (described below) also restricts
    which documents are allowed in a process.
* **Soft Process Limit**: On desktop platforms, Chromium sets a "soft" process
    limit based on the memory available on a given client. While this can be
    exceeded (e.g., if Site Isolation is enabled and the user has more open
    sites than the limit), Chromium makes an attempt to start randomly reusing
    same-site processes when over this limit. For example, if the limit is 100
    processes and the user has 50 open tabs to `example.com` and 50 open tabs to
    `example.org`, then a new `example.com` tab will share a process with a
    random existing `example.com` tab, while a `chromium.org` tab will create a
    101st process. Note that Chromium on Android does not set this soft process
    limit, and instead relies on the OS to discard processes.
* **Aggressive Reuse**: For some cases (including on Android), Chromium will
    aggressively look for existing same-site processes to reuse even before
    reaching the process limit. Out-of-process iframes (OOPIFs) and [fenced
    frames](https://developer.chrome.com/en/docs/privacy-sandbox/fenced-frame/)
    use this approach, such that an `example.com` iframe in a cross-site page
    will be placed in an existing `example.com` process (in any browsing context
    group), even if the process limit has not been reached. This keeps the
    process count lower, based on the assumption that most iframes/fenced frames
    are less resource demanding than top-level documents. Similarly,
    ServiceWorkers are generally placed in the same process as a document that
    is likely to rely on them.
* **Extensions**: Chromium ensures that extensions do not share a process with
    each other or with web pages, but also that a large number of extensions
    will not consume the entire soft process limit, forcing same-site web pages
    into too few processes. Chromium only allows extensions to consume [one
    third](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/extensions/chrome_content_browser_client_extensions_part.cc;drc=8d6a246c9be4f6b731dc7f6e680b7d5e13a512b5;l=454-458)
    of the process limit before disregarding further extension processes from
    the process limit computation.
* **Process-per-site**: As noted above, pages like the New Tab Page (NTP) and
    extensions use a model where all instances of the page are placed in the
    same process.


## Process Locks

Chromium assigns a
[ProcessLock](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/process_lock.h;drc=47457a6923c0527261d0503998cbeb7de9bab489;l=19)
to some or all RenderProcessHosts, to restrict which sites are allowed to load
in the process and which data the process has access to. A RenderProcessHost is
an object in the browser process that represents a given renderer process,
though it can be reused if that renderer process crashes and is restarted. Some
ProcessLock cases are used on all platforms (e.g., `chrome://` URLs are never
allowed to share a process with other sites), while other cases may depend on
the mode (e.g., Full Site Isolation requires all processes to be locked, once
content has been loaded in the process).

ProcessLocks may have varying granularity, such as a single site
(e.g., `https://example.com`), a single origin
(e.g., `https://accounts.example.com`), an entire scheme (e.g., `file://`), or
a special "allow-any-site" value for processes allowed to host multiple sites
(which may have other restrictions, such as whether they are
crossOriginIsolated). RenderProcessHosts begin with an "invalid" or unlocked
ProcessLock before one is assigned.

ProcessLocks are always assigned before any content is loaded in a renderer
process, either at the start of a navigation or at OnResponseStarted time, just
before a navigation commits. Note that a process may initially receive
an "allow-any-site" lock for some empty document schemes (e.g., `about:blank`),
which may later be refined to a site-specific lock when the first actual
content commits. Once a site-specific lock is assigned, it remains constant for
the lifetime of the RenderProcessHost, even if the renderer process itself
exits and is recreated.

Note that content may be allowed in a locked process based on its origin
(e.g., an `about:blank` page with an inherited `https://example.com` origin is
allowed in a process locked to `https://example.com`). Also, some opaque origin
cases are allowed into a locked process as well, such as `data:` URLs created
within that process.


## Special Cases

There are many special cases to consider in Chromium's process model, which may
affect invariants or how features are designed.

* **WebUI**: Pages like `chrome://settings` are considered part of Chromium and
    are highly privileged, usually hosted in the `chrome://` scheme. They are
    strictly isolated from non-WebUI pages as well as other types of WebUI
    pages (based on "site"), on all platforms. They are also generally not
    allowed to load content from the network (apart from a shrinking
    [list](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/webui/chrome_web_ui_controller_factory.cc;drc=3344b61f7c7f06cf96069751c3bd64d8ec3e3428;l=1405)
    of allowlisted pages), unless it is from a separate unprivileged
    `chrome-untrusted://` document. Additionally, normal web pages are not
    allowed to navigate to WebUI pages, which makes privilege escalation
    attacks more difficult.
* **New Tab Page**: On desktop platforms, the default "local" NTP is a WebUI
    page using process-per-site mode, which loads content from the network via
    `chrome-untrusted://` iframes. Third party NTPs are also possible, which
    load a "remote" non-WebUI web page with limited privileges. On Android, the
    NTP is instead a native Android surface with no privileged renderer
    process. Chrome on Android creates an unused renderer process in the
    background while the NTP surface is visible, so that the next page can use
    it.
* **Extensions**: On desktop platforms, extension documents and workers are
    semi-privileged and run in dedicated renderer processes. In contrast,
    extension content scripts run within the same unprivileged web renderer
    process as the pages they modify, and thus Chrome Extensions need to
    [treat content scripts as less
    trustworthy](https://groups.google.com/a/chromium.org/g/chromium-extensions/c/0ei-UCHNm34/m/lDaXwQhzBAAJ).
    The browser process makes an effort to enforce that renderer processes have
    access to any extension APIs or capabilities that they attempt to use.
* **Hosted Apps**: A hosted app is a deprecated type of extension which allows a
    normal web site to have a special type of renderer process. For example, a
    hosted app for `https://example.com/app/` will have an "effective URL" that
    looks like a `chrome-extension://` URL, causing it to be treated
    differently in the process model. This support may eventually be removed.
* **Chrome Web Store**: The [Chrome Web
    Store](https://chromewebstore.google.com/) is a rare example of a privileged
    web origin, to which Chrome grants special APIs for installing extensions.
* **[Isolated Web Apps](https://github.com/WICG/isolated-web-apps/blob/main/README.md)**: Isolated
    Web Apps (IWAs) are a type of web app that has stricter security and
    isolation requirements compared to normal web apps. The StoragePartition
    used for each IWA will be separate from the default StoragePartition used
    for common browsing and separate from other IWAs. IWAs require strict CSP,
    [CrossOriginIsolated](#crossoriginisolated), along with other isolation
    criteria. These contexts are claimed to be
    "[IsolatedContext](https://wicg.github.io/isolated-web-apps/isolated-contexts)"
    and
    "[IsolatedApplication](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/web_exposed_isolation_level.h;l=62;drc=998312ac45f85e53257049c5891dff558f203c00)".
* **GuestView**: The Chrome Apps `<webview>` tag and similar cases like
    MimeHandlerView and ExtensionOptionsGuest embed one WebContents within
    another. All of these cases use strict site isolation for content they
    embed. Note that Chrome Apps allow `<webview>` tags to load normal web pages
    and the app's own `data:` or `chrome-extension://` URLs, but not URLs from
    other extensions or apps. The IWA
    [<controlledframe>](/chrome/common/controlled_frame/README.md) tag is built
    on top of the '<webview>' tag's implementation and exposed to contexts
    that meet the proper security and isolation requirements, such as IWAs that
    provide IsolatedContexts. See the
    [Isolated Contexts spec](https://wicg.github.io/isolated-web-apps/isolated-contexts)
    for more info.
* **Sandboxed iframes**: Documents with the sandbox attribute and without
    `allow-same-origin` (either iframes or popups) may be same-site with their
    parent or opener but use an opaque origin. Since 127.0.6483.0, Desktop
    Chromium moves these documents into a separate process from their
    parent or opener. On Android, these documents will only be in a separate
    process if their parent/opener uses
    [Partial Site Isolation](#partial-site-isolation). Sandboxed frames embedded
    in extension pages are in a separate process if they are listed in the
    "sandbox" section of the extension's manifest, otherwise they are in the
    same process as the parent.
* **`data:` URLs**: Chromium generally keeps documents with `data:` URLs in the
    same process as the site that created them, since that site has control
    over their content. The exception is when restoring a previous session, in
    which case each document with a `data:` URL ends up in its own process.
* **File URLs**: Chromium currently treats all `file://` URLs as part of the
    same site. Normal web pages are not allowed to load `file://` URLs, and
    renderer processes are only granted access to particular `file://` URLs via
    file chooser dialogs (e.g., for uploads). These URLs may be further isolated
    from each other in bug [780770](https://crbug.com/780770).
* **Error Pages**: Chromium uses a special type of process for error pages
    provided by the browser (as opposed to error pages provided by a web site,
    like a 404 page), using process-per-site mode to keep all such pages in the
    same process. Currently this only applies to error pages in a main frame.
* **Spare Process**: Chromium often creates a spare RenderProcessHost with a
    live but unlocked renderer process, which is used the next time a renderer
    process is needed. This avoids the need to wait for a new process to
    start.
* **Android WebView**: While Android WebView uses much of the same code as
    Chromium, it currently only supports a single renderer process in most
    cases.


## Further Reading

Several academic papers have covered topics about Chromium's process model.

[**Security Architecture of the Chromium
Browser**](https://crypto.stanford.edu/websec/chromium/)

Adam Barth, Collin Jackson, Charles Reis, and The Google Chrome Team. Stanford
Technical Report, September 2008.

_Abstract:_

Most current web browsers employ a monolithic architecture that combines "the
user" and "the web" into a single protection domain. An attacker who exploits
an arbitrary code execution vulnerability in such a browser can steal sensitive
files or install malware. In this paper, we present the security architecture
of Chromium, the open-source browser upon which Google Chrome is built.
Chromium has two modules in separate protection domains: a browser kernel,
which interacts with the operating system, and a rendering engine, which runs
with restricted privileges in a sandbox. This architecture helps mitigate
high-severity attacks without sacrificing compatibility with existing web
sites. We define a threat model for browser exploits and evaluate how the
architecture would have mitigated past vulnerabilities.

[**Isolating Web Programs in Modern Browser
Architectures**](https://research.google.com/pubs/archive/34924.pdf)

Charles Reis, Steven D. Gribble (both authors at UW + Google). Eurosys,
April 2009.

_Abstract:_

Many of today's web sites contain substantial amounts of client-side code, and
consequently, they act more like programs than simple documents. This creates
robustness and performance challenges for web browsers. To give users a robust
and responsive platform, the browser must identify program boundaries and
provide isolation between them.

We provide three contributions in this paper. First, we present abstractions of
web programs and program instances, and we show that these abstractions clarify
how browser components interact and how appropriate program boundaries can be
identified. Second, we identify backwards compatibility tradeoffs that
constrain how web content can be divided into programs without disrupting
existing web sites. Third, we present a multi-process browser architecture that
isolates these web program instances from each other, improving fault
tolerance, resource management, and performance. We discuss how this
architecture is implemented in Google Chrome, and we provide a quantitative
performance evaluation examining its benefits and costs.

[**Site Isolation: Process Separation for Web Sites within the
Browser**](https://www.usenix.org/conference/usenixsecurity19/presentation/reis)

Charles Reis, Alexander Moshchuk, and Nasko Oskov, Google. Usenix Security,
August 2019.

_Abstract:_

Current production web browsers are multi-process but place different web sites
in the same renderer process, which is not sufficient to mitigate threats
present on the web today. With the prevalence of private user data stored on
web sites, the risk posed by compromised renderer processes, and the advent of
transient execution attacks like Spectre and Meltdown that can leak data via
microarchitectural state, it is no longer safe to render documents from
different web sites in the same process. In this paper, we describe our
successful deployment of the Site Isolation architecture to all desktop users
of Google Chrome as a mitigation for process-wide attacks. Site Isolation locks
each renderer process to documents from a single site and filters certain
cross-site data from each process. We overcame performance and compatibility
challenges to adapt a production browser to this new architecture. We find that
this architecture offers the best path to protection against compromised
renderer processes and same-process transient execution attacks, despite
current limitations. Our performance results indicate it is practical to deploy
this level of isolation while sufficiently preserving compatibility with
existing web content. Finally, we discuss future directions and how the current
limitations of Site Isolation might be addressed.
