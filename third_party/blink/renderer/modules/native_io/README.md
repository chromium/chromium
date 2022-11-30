# Storage Foundation API

[Storage Foundation API](https://github.com/fivedots/storage-foundation-api-explainer)
is a proposal for a low-level storage mechanism that will open a path for
developers to use their favorite database in their web applications. This API is
expected to be used heavily from WebAssembly, and the most popular ported
database is expected to be SQLite.

Most of the Blink implementation should be unsurprising, given the WebIDL for
the feature. This README goes over the parts that may be surprising to a storage
engineer.

Note: Storage Foundation API used to be called NativeIO and some references to
this name still remain in the code base.

## Synchronous storage APIs

The current prototype implements both synchronous and asynchronous versions of
Storage Foundation API. This allows us to collaborate with web developers to
understand the overhead of an asynchronous API, compared to the fastest
synchronous API we could possibly build.

The synchronous API is limited to dedicated workers.

## SharedArrayBuffer

The current Storage Foundation API design assumes that allocating and copying
memory buffers will result in significant overhead. To test this hypothesis, the
prototype includes zero-copy APIs, where the caller passes in a view into a
caller-owned I/O buffer.

This approach introduces an interesting design challenge for the asynchronous
API, because the web application can run code that interacts with the I/O buffer
while the operation is in progress, and observe side-effects. For example, the
application can read the buffer in a tight loop, and observe how the OS is
filling it with data from disk.

The current prototype requires that I/O buffers passed to the asynchronous API
are backed by SharedArrayBuffers, because these are intended to be modified by
multiple threads at the same time.

This aspect of the prototype is fairly unstable. Benchmarking may reveal that
we don't need zero-copy, and WebAssembly interfacing concerns may nudge us
towards a different design.

## Locking

The current prototype enforces that there is at most one open handle to a
Storage Foundation API file. In other words, opening a file only succeeds if it
can get an exclusive lock on the file, and closing the file releases the lock.

This approach is different from other storage APIs, which wait until they can
grab a lock. The main reason for the difference is this is the first API built
after
[Web Locks](https://developer.mozilla.org/en-US/docs/Web/API/Web_Locks_API), so
we can ask developers to take advantage of Web Locks instead of building a lock
manager with associated tests into each new storage API.

The main reason for enforcing locking early in the API design is to avoid
exposing the details of OS-level file descriptor sharing to the Web Platform.
This aspect of the prototype may also change, based on research and developer
feedback.

## Quota Management

Storage Foundation API forces applications to pre-allocate storage space before
writing to disk. This approach is different from other storage APIs, which ask
for sufficient storage space on every write operation. The main reason for the
difference is to allow the API to handle most aspects of quota management within
Blink. We hope that this results in smaller and more predictable overhead.

More precisely, the web application must explicitly ask for capacity before
storing any new data. This request will be granted according to Chrome’s quota
guidelines. The application can then use the capacity to execute write
operations. The application can also delete or overwrite data to make space for
new data without issuing a new request.

# Design documents

- [Quota Management](https://docs.google.com/document/d/1wUrtCOsyH3qGwKuqLhV9AJD-bDAjAzfPL5r1GT8H4IY)
- [Error handling](https://docs.google.com/document/d/1rvs615AU2s8kVsmUlukbmtQNvUWFny0yzAS_gsnYZEs)
- [Filename length](https://docs.google.com/document/u/1/d/1gjnHg9b0vuTul54MHAzRFfQ6T1XKdayYoGKs6Td8yuQ)
- [Replacing SharedArrayBuffers](https://docs.google.com/document/d/1yJv2b5yBhyIj4kOEExEu1s6oZELm2IjywbOrWwNdkuU)
