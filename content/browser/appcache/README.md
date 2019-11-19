# AppCache

AppCache is the well-known shorthand for `Application Cache`, the key mechanism
in the
[Offline Web applications specification](https://html.spec.whatwg.org/multipage/offline.html#offline).

*** promo
AppCache is deprecated and slated for removal from the Web Platform. Chrome's
implementation is in maintenance mode. We're only tacking critical bugs and code
health improvements that allow us to reason about bugs easier. Long-term efforts
should be focused on Service Workers.
***


## Overview

AppCache is aimed at SPAs (single-page Web applications).

The application's HTML page (Document) points to a **manifest** in an `<html
manifest=...>` attribute. The manifest lists all the sub-resources (style
sheets, scripts, images, etc.) that the page needs to work offline. When a
user navigates to the HTML page for the first time, the browser caches all
the resources in the manifest. Future navigations use the cached resources,
so the application still works even if the network is down.

The simplified model above misses two critical pieces, which are responsible for
the bulk of AppCache's complexity. The sections below can be skimmed on a first
reading.

### Updates (Why AppCache is Hard, Part 1)

The ease of deploying updates is a key strength of Web applications. Browsers
automatically (barring misconfigured HTTP caching) load the latest version of
an application's resources when a user navigates to one of the application's
pages.

AppCache aims for comparable ease by automatically updating its locally cached
copy of the manifest and its resources whenever a page is visited. This comes
with some significant caveats:

1. AppCache bails early in the update process if the manifest hasn't changed
   (byte for byte). This behavior is intended to save network bandwidth.
   The downside is that developers must change their manifest whenever any of
   the sub-resources change.
2. The manifest does not have any versioning information in it. So, when a
   manifest changes, the browser must reload all the resources referenced by
   it.
3. The manifest is only checked for updates when a page is visited, to keep the
   Web ephemeral. The update check is performed concurrently with page loading,
   for performance reasons. If the manifest changed, all the resources used
   by the page are served from the outdated cache. This is necessary, because by
   the time the browser can detect a manifest update, the page has been
   partially loaded using the (now known to be outdated) cached resources.
   It's not reasonable to ask Web developers to support mixing resources from
   different application versions.
4. While the browser is downloading a page's cache (the manifest and its
   resources), the user could navigate a different tab to the same page. The
   second tab uses the result of the ongoing cache download, rather than
   updating the cache on its own. This removes many race conditions from the
   cache update process, at the cost of having the browser coordinate between
   all instances of a page that uses AppCache.
5. AppCache also supports application-driven updates. The support is aimed at
   applications that may be left open in the same tab for a long time, like
   e-mail and chat clients. This means browsers must support both
   navigation-driven cache updates and application-driven updates.

### Multi-Page Applications (Why AppCache is Hard, Part 2)

AppCache supports multi-page applications by allowing multiple pages to share
the same manifest, and therefore use the same cached resources.

Manifest sharing is particularly complex when combined with implicit caching.
An AppCache manifest is not required to list the HTML pages that refer to it
via an `<html manifest>` attribute. (Listing the pages is however recommended.)
This allowance introduces the following complexities:

1. When a browser encounters an HTML page that refers to a manifest it hasn't
   seen before, the browser creates an implicit resource entry for the HTML
   page. The HTML page is cached together with the other resources listed in
   the manifest, so it can be available for offline browsing.
2. When a browser encounters an HTML page that refers to a manifest it has
   already cached, the browser also creates an implicit resource entry for
   the HTML page. The existing cache must be changed to include the new
   implicit resource.
3. When a manifest changes, the browser must update all the implicit resources
   (HTML pages that refer to the manifest) as well as the resources explicitly
   mentioned in the manifest. If any of the HTML pages using the manifest are
   opened, they must be notified that a manifest update is available.
4. When a browser encounters an HTML page that refers to a manifest whose
   resources are still being downloaded, it needs to ensure that the page's
   implicit resource eventually gets associated with the manifest. To avoid race
   conditions, the browser must add the HTML page to a list of pages that need
   updating. The manifest update logic must also process this list, after
   downloading the resources already associated with the manifest.

While the pages in multi-page applications can share a manifest, they are not
required to do so. In other words, an application's pages can use different
manifests. However, each manifest conceptually spawns its own resource cache,
which is updated independently from other manifests' caches. So, different pages
from the same application may use different versions of the same sub-resource,
if they are associated with different manifests.

A particularly complex case is loading an HTML page that is associated with a
cached manifest, discovering that the manifest has changed and requires an
update, updating the HTML page, and obtaining a new version of the HTML page
that refers to a different manifest. In this case, loading a single page ends up
downloading two manifests and all the resources associated with them.


## Data Model

AppCache uses the following terms:

* A **manifest** is a list of URLs to resources. The listed resources should be
  be sufficient for the page to be used while offline.
* An **application cache** contains one version of a manifest and all the
  resources associated with it. This includes the resources explicitly listed in
  the manifest, and the implicitly cached HTML pages that refer to the manifest.
  The HTTP responses are stored in a disk_cache (//net term), then all other
  AppCache information is stored in a per-profile SQLite database that points
  into the disk_cache.  The disk_cache scope is per-profile.
* A **response** represents the headers and body for a given server response.
  This response is first served by a server and may then be stored and retrieved
  in the disk_cache.  The application cache in the SQLite database updates each
  entry to track the associated response id in the disk_cache for that entry.
* An **application cache group** is a collection of all the application caches
  that have the same manifest.
* A **cache host** is a name used to refer to a Document (HTML page) when the
  emphasis is on the connection between the page, the manifest it references,
  and the application cache / cache group associated with that manifest.

### Application Cache

An application cache has the following components:

1. **Entries** that identify resources to be cached.
2. **Namespaces** that direct the loading of sub-resource URLs for a page
   associated with the cache.
3. **Flags** that influence the cache's behavior.

All of these components are stored in and retrieved from a SQLite database.

Entries have the following types:

* **manifest** - the AppCache manifest; the absolute URL of this entry is used
  to identify the group that this application cache belongs to
* **master** - documents (HTML pages) whose `<html manifest>` attribute points
  to the cache's manifest; these are added to an application cache as they are
  discovered during user navigations
* **explicit** - listed in the manifest's explicit section (`CACHE:`)
* **fallback** - listed in the manifest's fallback section (`FALLBACK:`)

Explicit and fallback entries can also be marked as **foreign**. A foreign entry
indicates a document whose `<html manifest>` attribute does not point to this
cache's manifest.

Each entry can refer to its response, which allows AppCache to know where to
find a given entry's cached response data in its disk or memory cache.

Namespaces are conceptually patterns that match resource URLs. AppCache supports
the following namespaces:

* **fallback** - URLs matching the namespace are first fetched from the network.
  If the fetch fails, a cached fallback resource is used instead. Fallback
  namespaces are listed in the `FALLBACK:` manifest section.
* **online safelist** -- URLs matching the namespace are always fetched from the
  network. Online safelist namespaces are listed in the `NETWORK:` manifest
  section.

*** promo
Chrome's AppCache implementation also supports **intercept** namespaces, listed
in the `CHROMIUM-INTERCEPT:` manifest section. URLs matching an intercept
namespace are loaded as if the fetch request encountered an HTTP redirect.
***

The AppCache specification supports specifying namespaces as URL prefixes. Given
a list of namespaces in an application cache, a resource URL matches the longest
namespace that is a prefix of the URL.

*** promo
Our AppCache implementation also supports specifying namespaces as regular
expressions that match URLs. This extension is invoked by adding the `isPattern`
keyword after the namespace in the manifest.
***

An application cache has the following flags:

* **completeness** - the application cache is *complete* when all the resources
  in the manifest have been fetched and cached, and *incomplete* otherwise
* **online safelist wildcard** - *blocking* by default, which means that all
  resources not listed in the manifest are considered unavailable; can be set
  to *open* by adding an `*` entry in the `NETWORK:` manifest section, causing
  all unlisted resources to be fetched from the network
* **cache mode** - not supported by Chrome, which does not implement the
  `SETTINGS:` manifest section
