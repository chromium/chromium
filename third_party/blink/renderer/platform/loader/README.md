# platform/loader/

## Loading and Freezing

Loaders (i.e., blink::ResourceLoaders and related objects) run on unfreezable
task runners, which means even when the associated page is frozen a loader is
still running. This is because having frozen loaders for a long time
confuses the network service and the resource scheduler in it. On the other
hand, it would be difficult for blink code authors to handle the situation
where the loading module is working while everything else is frozen
correctly, given that loading experts are not necessarily freezing experts.
To mitigate the pain, we run only part of the loading module when the page
is frozen. Namely,

- blink::MojoURLLoaderClient
- blink::ResponseBodyLoader
- blink::ResourceRequestClient implementations
- blink::ThrottlingURLLoader implementations
- blink::BackForwardCacheLoaderHelper implementations

may run when the associated context is frozen, and other modules won't run
(except for the case where they are called from outside of the loading
module). To guarantee this, blink::ResourceFetcher::SetDefersLoading needs
to be called when the associated context gets frozen and unfrozen.

## Directory organization

### cors

Contains Cross-Origin Resource Sharing (CORS) related files. Some functions
in this directory will be removed once CORS support is moved to
//services/network. Please contact {kinuko,toyoshim}@chromium.org when you need
to depend on this directory from new code.

### fetch

Contains files for low-level loading APIs.  The `PLATFORM_EXPORT` macro is
needed to use them from `core/`.  Otherwise they can be used only in
`platform/`.

The directory conceptually implements https://fetch.spec.whatwg.org/#fetching
(with lower-level components such as the network service). See also: [slides
describing the relationship with the fetch spec](https://docs.google.com/presentation/d/1r9KHuYbNlgqQ6UABAMiWz0ONTpSTnMaDJ8UeYZGWjls/)

### testing

Contains helper files for testing that are available in both
`blink_platform_unittests` and `blink_unittests`.
These files are built as a part of the `platform:test_support` static library.
