# A Crash Course in Debugging with chrome://net-internals

This document is intended to help get people started debugging network errors
with chrome://net-internals, with some commonly useful tips and tricks.  This
document is aimed more at how to get started using some of its features to
investigate bug reports, rather than as a feature overview.

It would probably be useful to read
[life-of-a-url-request.md](life-of-a-url-request.md) before this document.

# What Data Net-Internals Contains

chrome://net-internals provides a view of browser activity from net/'s
perspective.  For this reason, it lacks knowledge of tabs, navigation, frames,
resource types, etc.

The leftmost column presents a list of views.  Most debugging is done with the
Events view, which will be all this document covers.

The top level network stack object is the URLRequestContext.  The Events view
has information for all Chrome URLRequestContexts that are hooked up to the
single, global, NetLog object.  This includes both incognito and
non-incognito profiles, among other things.  The Events view only shows events
for the period that net-internals was open and running, and is incrementally
updated as events occur.  The code attempts to add a top level event for
URLRequests that were active when the chrome://net-internals tab was opened, to
help debug hung requests, but that's best-effort only, and only includes
requests for the current profile and the system URLRequestContext.

The other views are all snapshots of the current state of the main
URLRequestContext's components, and are updated on a 5 second timer.  These will
show objects that were created before chrome://net-internals was opened.

# Events vs Sources

The Events view shows events logged by the NetLog.  The NetLog model is that
long-lived network stack objects, called sources, emit events over their
lifetime.  A NetLogWithSource object contains a source ID, a NetLogSourceType,
and a pointer to the NetLog the source emits events to.

The Events view has a list of sources in a column adjacent to the list of views.
Sources that include an event with a net_error parameter with negative value
(that is, some kind of ERR_) are shown with red background.  Sources whose
opening event has not ended yet are shown with white background.  Other events
have green background.  The search queries corresponding to the first two kinds
are `is:error` and `is:active`.

When one or more sources are selected, corresponding events show up in another
column to the right, sorted by source, and by time within each source.  There
are two time values: t is measured from some reference point common to all
sources, and st is measured from the first event for each source.  Time is
displayed in milliseconds.

Since the network stack is asynchronous, events from different sources will
often be interlaced in time, but Events view does not feature showing events from
different sources ordered by time.  Large time gaps in the event list of a
single source usually mean that time is spent in the context of another source.

Some events come in pairs: a beginning and end event, between which other events
may occur.  They are shown with + and - prefixes, respectively.  The begin event
has a dt value which shows the duration.  If the end event was captured, then
duration is calculated as the time difference between the begin and the end
events.  Otherwise the time elapsed from the begin event until capturing
was stopped is displayed (a lower bound for actual duration), followed by a +
sign (for example, "dt=120+").

If there are no other events in between the begin and end, and the end event has
no parameters, then they are collapsed in a single line without a sign prefix.

Some other events only occur at a single point in time, and will not have either
a sign prefix, or a dt duration value.

Generally only one event can be occuring for a source at a time.  If there can
be multiple events doing completely independent things, the code often uses new
sources to represent the parallelism.

Most, but not all events correspond to a source.  Exceptions are global events,
which have no source, and show up as individual entries in the source list.
Examples of global events include NETWORK_CHANGED, DNS_CONFIG_CHANGED, and
PROXY_CONFIG_CHANGED.

# Common source types

"Sources" correspond to certain net objects, however, multiple layers of net/
will often log to a single source.  Here are the main source types and what they
include (excluding HTTP2 [SPDY]/QUIC):

* URL_REQUEST:  This corresponds to the URLRequest object.  It includes events
from all the URLRequestJobs, HttpCache::Transactions, NetworkTransactions,
HttpStreamRequests, HttpStream implementations, and HttpStreamParsers used to
service a response.  If the URL_REQUEST follows HTTP redirects, it will include
each redirect.  This is a lot of stuff, but generally only one object is doing
work at a time.  This event source includes the full URL and generally includes
the request / response headers (except when the cache handles the response).

* HTTP_STREAM_JOB:  This corresponds to HttpStreamFactory::Job (note that one
  Request can have multiple Jobs).  It also includes its proxy and DNS lookups.
  HTTP_STREAM_JOB log events are separate from URL_REQUEST because two stream
  jobs may be created and races against each other, in some cases -- one for
  QUIC, and one for HTTP.

    One of the final events of this source, before the
    HTTP_STREAM_JOB_BOUND_TO_REQUEST event, indicates how an HttpStream was
    created:

    + A SOCKET_POOL_BOUND_TO_CONNECT_JOB event means that a new TCP socket was
    created, whereas a SOCKET_POOL_REUSED_AN_EXISTING_SOCKET event indicates that
    an existing TCP socket was reused for a non-HTTP/2 request.

    + An HTTP2_SESSION_POOL_IMPORTED_SESSION_FROM_SOCKET event indicates that a
    new HTTP/2 session was opened by this Job.

    + An HTTP2_SESSION_POOL_FOUND_EXISTING_SESSION event indicates that the request
    was served on a preexisting HTTP/2 session.

    + An HTTP2_SESSION_POOL_FOUND_EXISTING_SESSION_FROM_IP_POOL event means that
    the request was pooled to a preexisting HTTP/2 session which had a different
    SpdySessionKey, but DNS resolution resulted in the same IP, and the
    certificate matches.

    + There are currently no events logged for opening new QUIC sessions or
    reusing existing ones.

* \*_CONNECT_JOB:  This corresponds to the ConnectJob subclasses that each socket
pool uses.  A successful CONNECT_JOB returns a SOCKET.  The events here vary a
lot by job type.  Their main event is generally either to create a socket, or
request a socket from another socket pool (which creates another CONNECT_JOB)
and then do some extra work on top of that -- like establish an SSL connection on
top of a TCP connection.

* SOCKET:  These correspond to TCPSockets, but may also have other classes
layered on top of them (like an SSLClientSocket).  This is a bit different from
the other classes, where the name corresponds to the topmost class, instead of
the bottommost one.  This is largely an artifact of the fact the socket is
created first, and then SSL (or a proxy connection) is layered on top of it.
SOCKETs may be reused between multiple requests, and a request may end up
getting a socket created for another request.

* HOST_RESOLVER_IMPL_JOB:  These correspond to HostResolverImpl::Job.  They
include information about how long the lookup was queued, each DNS request that
was attempted (with the platform or built-in resolver) and all the other sources
that are waiting on the job.

When one source depends on another, the code generally logs an event at both
sources with a `source_dependency` value pointing to the other source.  These
are clickable in the UI, adding the referred source to the list of selected
sources.

# Debugging

When you receive a report from the user, the first thing you'll generally want
to do find the URL_REQUEST[s] that are misbehaving.  If the user gives an ERR_*
code or the exact URL of the resource that won't load, you can just search for
it.  If it's an upload, you can search for "post", or if it's a redirect issue,
you can search for "redirect".  However, you often won't have much information
about the actual problem.  There are two filters in net-internals that can help
in a lot of cases:

* "type:URL_REQUEST is:error" will restrict the source list to URL_REQUEST
objects with an error of some sort.  Cache errors are often non-fatal, so you
should generally ignore those, and look for a more interesting one.

* "type:URL_REQUEST sort:duration" will show the longest-lived requests first.
This is often useful in finding hung or slow requests.

For a list of other filter commands, you can mouse over the question mark on
chrome://net-internals.

Once you locate the problematic request, the next is to figure out where the
problem is -- it's often one of the last events, though it could also be related
to response or request headers.  You can use `source_dependency` links to
navigate between related sources.  You can use the name of an event to search
for the code responsible for that event, and try to deduce what went wrong
before/after a particular event.

Some things to look for while debugging:

* CANCELLED events almost always come from outside the network stack.

* Changing networks and entering / exiting suspend mode can have all sorts of
fun and exciting effects on underway network activity.  Network changes log a
top level NETWORK_CHANGED event.  Suspend events are currently not logged.

* URL_REQUEST_DELEGATE_\* / NETWORK_DELEGATE_\* / DELEGATE_INFO events mean a
URL_REQUEST is blocked on a URLRequest::Delegate or the NetworkDelegate, which
are implemented outside the network stack.  A request will sometimes be CANCELED
here for reasons known only to the delegate.  Or the delegate may cause a hang.
In general, to debug issues related to delegates, one needs to figure out which
method of which object is causing the problem.  The object may be the a
NetworkDelegate, a ResourceThrottle, a ResourceHandler, the ResourceLoader
itself, or the ResourceDispatcherHost.

* Sockets are often reused between requests.  If a request is on a stale
(reused) socket, what was the previous request that used the socket, how long
ago was it made?  (Look at SOCKET_IN_USE events, and the HTTP_STREAM_JOBS they
point to via the `source_dependency` value.)

* SSL negotation is a process fraught with peril, particularly with broken
proxies.  These will generally stall or fail in the SSL_CONNECT phase at the
SOCKET layer.

* Range requests have magic to handle them at the cache layer, and are often
issued by the media and PDF code.

* Late binding:  HTTP_STREAM_JOBs are not associated with any CONNECT_JOB until
a CONNECT_JOB actually connects.  This is so the highest priority pending
HTTP_STREAM_JOB gets the first available socket (which may be a new socket, or
an old one that's freed up).  For this reason, it can be a little tricky to
relate hung HTTP_STREAM_JOBs to CONNECT_JOBs.

* Each CONNECT_JOB belongs to a "group", which has a limit of 6 connections.  If
all CONNECT_JOBs belonging to a group (the CONNECT_JOB's description field) are
stalled waiting on an available socket, the group probably has 6 sockets that
that are hung -- either hung trying to connect, or used by stalled requests and
thus outside the socket pool's control.

* There's a limit on number of DNS resolutions that can be started at once.  If
everything is stalled while resolving DNS addresses, you've probably hit this
limit, and the DNS lookups are also misbehaving in some fashion.

# Miscellany

These are just miscellaneous things you may notice when looking through the
logs.

* URLRequests that look to start twice for no obvious reason.  These are
typically main frame requests, and the first request is AppCache.  Can just
ignore it and move on with your life.

* Some HTTP requests are not handled by URLRequestHttpJobs.  These include
things like HSTS redirects (URLRequestRedirectJob), AppCache, ServiceWorker,
etc.  These generally don't log as much information, so it can be tricky to
figure out what's going on with these.

* Non-HTTP requests also appear in the log, and also generally don't log much
(blob URLs, chrome URLs, etc).

* Preconnects create a "HTTP_STREAM_JOB" event that may create multiple
CONNECT_JOBs (or none) and is then destroyed.  These can be identified by the
"SOCKET_POOL_CONNECTING_N_SOCKETS" events.
