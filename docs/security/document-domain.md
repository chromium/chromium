# `document.domain` Setting is Deprecated

Setting [`document.domain`](https://developer.mozilla.org/en-US/docs/Web/API/Document/domain)
is [deprecated](https://html.spec.whatwg.org/multipage/origin.html#relaxing-the-same-origin-restriction).
`document.domain` setting can be used to relax the same-origin restrictions
between different frames, hosted within the same site but on different origins.
Doing so make themselves effectively same-origin for the purpose of
synchronous access.

For example, `https.//www.example.test/` might host a media player on
`media.example.org`. If both the main page and the frame execute
`document.domain = "example.org";` they may then access each others' DOM
tree, which they normally couldn't. (A better way would be to cooperate
by `postMessage()`.)

This usage is now being deprecated. (More information can be found
[here](https://developer.chrome.com/blog/immutable-document-domain/) and
[there](https://github.com/mikewest/deprecating-document-domain)).

## What is happening, and when?

* M100: Chrome will show deprecation warnings when document.domain is set.
* M106: Chrome will disable document.domain by default.

Note that the second milestone is tentative: When the time comes, we will
examine how many pages will be impacted by this change, and will start a
separate discussion (intent to remove) on the
[blink-dev mailing list](https://groups.google.com/a/chromium.org/g/blink-dev).

##  `document.domain` and Origin-keyed Agent Clusters

Most documentation on this change is phrased in terms of origin-keyed
agent clusters. This is [a concept in the HTML
specification](https://html.spec.whatwg.org/multipage/origin.html#origin-keyed-agent-clusters).
Here we focus on the behaviour of the `document.domain` setter, which is
the visible effect.

A web browser can cluster sites (in order to assign them to operating
system processes) and sites can be clustered by origin, or by site.
Origin-keyed agent clustering is preferable for security reasons. However
when sites are clustered by origin, synchronous access to frames outside of
that origin (but within the same site) is no longer possible. Thus sites in
origin-keyed agent clusters disable the `document.domain` setter. This is the
mechanism underlying this change: From M106 on pages will be assigned to
origin-keyed agent clusters by default and therefore `document.domain`
will no longer be settable by default, either.

This also gives us an opt-out mechanism for pages who do not wish to follow
this change: By setting the `Origin-Agent-Cluster: ?0` http header, a site
can request assignment to a site-keyed agent cluster, and `document.domain`
will continue to work for them as it currently does. Note that adding this
header has no other observable effect and thus retains the current
(pre-M106) behaviour. This makes it an easy and _safe_ way to opt-out.

Setting this header is a no-op in current versions of Chromium since it
matches the default setting, and will preserve this behaviour in the future.
It is also a no-op in other browsers, since they either match Chromium's
current default or have not implemented the `Origin-Agent-Cluster` header at
all.

## Where are the deprecation warnings found?

The deprecation warnings are found in the [issues tab](https://developer.chrome.com/docs/devtools/issues/).

## What does the deprecation warning tell me?

There are two deprecation warnings: One for setting the `document.domain`
accessors, which modifies the security behaviour. And from M101 on,
a second warning when a cross-domain access is made that is facilitated by
the modified `document.domain` property. The first warning tells you where
the setup happens, and the second one tells you where it is being used (and
thus likely why this is being done in the first place).

## How Can I Test This?

In the DevTools console, for a page `www.example.test`:

```
document.domain = "example.test";
document.domain;  // "example.test" in a site-keyed agent cluster.
                  // "www.example.test" in an origin-keyed agent cluster.
```

One can also directly query whether a page is assigned to an origin-keyed
agent cluster, by querying `window.originAgentCluster`.

```
window.originAgentCluster;  // true, if page is assigned to an origin-keyed
                            // agent cluster.
```

How to enable/disable the deprecation:

### Enable the Warning (Before M100)

* Start Chrome with `--enable-features=OriginAgentClusterDefaultWarning`

### Enable the Deprecation (Scheduled for M106)

* In [chrome://flags](chrome://flags#origin-agent-cluster-default), go to
  the "Origin Agent Cluster Default" setting and enable it.
* Or start Chrome with `--enable-features=OriginAgentClusterDefaultEnable`
* Or add the `Origin-Agent-Cluster: ?1` header to your pages and frames.
  (E.g. in a testing instance of your site.)

### Testing at Scale / Reporting API

The deprecation warnings are delivered through the
[Reporting API](https://web.dev/reporting-api/). They can
be pragrammatically processed using `ReportingObserver`. For example, the
first code snippet in
https://developers.google.com/web/updates/2018/07/reportingobserver
will report these warnings. The message object delivered by this API is a
[`DeprecationReportBody`](https://developer.mozilla.org/en-US/docs/Web/API/DeprecationReportBody)
instance which offers information about the source code location that triggered
the warning.

## What can I do?

If your site does not use `document.domain` setting you don't have to do
anything. You could explicitly set the `Origin-Agent-Cluster: ?1` header.
But after M106 this would be the default behaviour anyhow.

If your site uses `document.domain` setting to enable cross-origin Javascript
access, you should refactor the code to instead use
[`window.postMessage()`](https://developer.mozilla.org/en-US/docs/Web/API/Window/postMessage) (or any other mechanism) to cooperate across origins. Or alternatively
reduce the need for cross-origin cooperation by moving the cooperating pieces
onto the same origin.

## What if I don't have the time right now, or want to continue setting `document.domain`?

You can add the `Origin-Agent-Cluster: ?0` HTTP header to your site. Note that
the header must be set both for the main page, as well as for the embedded
frame(s) that wish to use `document.domain` setting.

## Enterprise Users

Users of [Chrome for Enterprise](https://chromeenterprise.google/) can set
the `OriginAgentClusterDefaultEnabled` policy to `False` to retain the
current (pre-M106) default for all of their users, until all their internal
sites and customers have migrated off of `document.domain` usage.

