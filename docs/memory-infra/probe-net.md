# Network Stack Memory Tracing

This is an overview of the Net column in [MemoryInfra][memory-infra].

[TOC]

## Quick Start

To get an overview of total network stack memory usage, select the Browser
process' *net* category and look at *effective_size* or *size* column.

![net stack column][net-stack-column]

[memory-infra]:     README.md
[net-stack-column]: https://storage.googleapis.com/chromium-docs.appspot.com/net_category.png

## Detailed Information

The numbers are reported by the network stack’s MemoryDumpProvider, which is
implemented by URLRequestContext class. URLRequestContext calls into major
network stack objects (such as HttpNetworkSession, SSLClientSessionCache,
HttpCache) to aggregate memory usage numbers. The total number reported in
“net” is a lower bound of the memory used by the network stack. It is not
intended to be an accurate measurement. Please use
[heap profiler][heap-profiler] instead to see all allocations.

**URLRequestContext** (“url_request_context”)

This is a top-level network stack object used to create url requests. There are
several URLRequestContexts in Chrome. See
[Anatomy of the Network Stack][anatomy-of-network-stack] for what these
URLRequestContexts are created for. The number of URLRequestContexts increases
with the number of profiles.

For a “url_request_context” row, the “object_count” column indicates the number
of live URLRequests created by that context.

+ Sub rows

    - HttpCache (“http_cache”)

        This cache can be a disk cache (backed by either block file or simple
        cache backend) or an in-memory cache. An incognito profile, for example,
        has an in-memory HttpCache. You can tell this by whether
        *mem_backend_size* column is present for that particular
        URLRequestContext.


**HttpNetworkSession** (“http_network_session”)

This network stack object owns socket pools, the HTTP/2 and QUIC session pools. 
There is usually a 1:1 correspondence from a URLRequestContext to an
HttpNetworkSession, but there are exceptions. For example, the “main”
URLRequestContext shares the same HttpNetworkSession with “main_media”
URLRequestContext and “main_isolated_media” URLRequestContext.

+ Sub rows

  - HttpStreamFactory(“stream_factory”)

      This object is an entry to establish HTTP/1.1, HTTP/2 and QUIC
      connections.

  - SpdySessionPool (“spdy_session_pool”)

      This object owns HTTP/2 sessions.

  - QuicSessionPool (“quic_session_pool”)

      This object owns QUIC sessions and streams.

**SSLClientSessionCache** (“ssl_session_cache”)

This is a global singleton that caches SSL session objects which retain
references to refcounted SSL Certificates.

[heap-profiler]:            /docs/memory-infra/heap_profiler.md
[anatomy-of-network-stack]: /net/docs/life-of-a-url-request.md

