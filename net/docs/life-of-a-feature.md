# Life of a Feature

In the years since the Chromium browser was first open-sourced, the `//net`
directory has expanded from being the basis of loading web content in the
Chromium browser to accommodating a wide variety of networking needs,
both in the Chromium browser and in other Google and third-party products
and projects.

This brings with it many new opportunities, such as the ability to
introduce new protocols rapidly or push Web security forward, as well as
new challenges, such as how to balance the needs of various `//net`
consumers effectively.

To make it easier to contribute new features or to change existing
behaviors in `//net`, this document tries to capture the life of a
feature in `//net`, from initial design to the eventual possibility of
deprecation and removal.

## Supported Projects

When considering the introduction of a new `//net` feature or changing
a `//net` behavior, it's first necessary to understand where `//net`
is used, how it is used, and what the various constraints and limits are.

To understand a more comprehensive matrix of the supported platforms and
constraints, see [Supported Projects](supported-projects.md). When
examining a new feature request, or a change in behavior, it's necessary
to consider dimensions such as:

  * Does this feature apply to all supported projects, or is this something
    like a Browser-only feature?
  * Does this feature apply to all supported platforms, or is this something
    specific to a particular subset?
  * Is the feature a basic networking library feature, or is it specific to
    something in the Web Platform?
  * Will some projects wish to strip the feature in order to meet targets
    such as memory usage (RAM) or binary size?
  * Does it depend on Google services or Google-specific behaviors or
    features?
  * How will this feature be tested / experimented with? For example,
    __Field Trials (Finch)__ and __User Metrics (UMA)__ may not be available
    on a number of platforms.
  * How risky is the feature towards compatibility/stability? How will it
    be undone if there is a bug?
  * Are the power/memory/CPU/bandwidth requirements appropriate for the
    targeted projects and/or platforms? 

## Design and Layering

Once the supported platforms and constraints are identified, it's necessary
to determine how to actually design the feature to meet those constraints,
in hopefully the easiest way possible both for implementation and consumption.

### Designing for multiple platforms

In general, `//net` features try to support all platforms with a common
interface, and generally eschew OS-specific interfaces from being exposed as
part of `//net`.

Cross-platform code is generally done via declaring an interface named
`foo.h`, which is common for all platforms, and then using the build-system to
do compile-time switching between implementations in `foo_win.cc`, `foo_mac.cc`,
`foo_android.cc`, etc.

The goal is to ensure that consumers generally don't have to think about
OS-specific considerations, and can instead code to the interface.

### Designing for multiple products

While premature abstraction can significantly harm readability, if it is
anticipated that different products will have different implementation needs,
or may wish to selectively disable the feature, it's often necessary to
abstract that interface sufficiently in `//net` to allow for dependency
injection.

This is true whether discussing concrete classes and interfaces or something
as simple a boolean configuration flag that different consumers wish to set
differently.

The two most common approaches in `//net` are injection and delegation.

#### Injection

Injection refers to the pattern of defining the interface or concrete
configuration parameter (such as a boolean), along with the concrete
implementation, but requiring the `//net` embedder to supply an instance
of the interface or the configuration parameters (perhaps optionally).

Examples of this pattern include things such as the `ProxyConfigService`,
which has concrete implementations in `//net` for a variety of platforms'
configuration of proxies, but which requires it be supplied as part of the
`URLRequestContextGetter` building, if proxies are going to be supported.

An example of injecting configuration flags can be seen in the
`HttpNetworkSession::Params` structure, which is used to provide much of
the initialization parameters for the HTTP layer.

The ideal form of injection is to pass ownership of the injected object,
such as via a `std::unique_ptr<Foo>`. While this is not consistently
applied in `//net`, as there are a number of places in which ownership is
either shared or left to the embedder, with the injected object passed
around as a naked/raw pointer, this is generally seen as an anti-pattern
and not to be mirrored for new features.

#### Delegation

Delegation refers to forcing the `//net` embedder to respond to specific
delegated calls via a Delegate interface that it implements. In general,
when using the delegate pattern, ownership of the delegate should be
transferred, so that the lifetime and threading semantics are clear and
unambiguous.

That is, for a given class `Foo`, which has a `Foo::Delegate` interface
defined to allow embedders to alter behavior, prefer a constructor that
is
```
explicit Foo(std::unique_ptr<Delegate> delegate);
```
so that it is clear that the lifetime of `delegate` is determined by
`Foo`.

While this may appear similar to Injection, the general difference
between the two approaches is determining where the bulk of the
implementation lies. With Injection, the interface describes a
behavior contract that concrete implementations must adhere to; this
allows for much more flexibility with behavior, but with the downside
of significantly more work to implement or extend. Delegation attempts
to keep the bulk of the implementation in `//net`, and the decision as
to which implementation to use in `//net`, but allows `//net` to
provide specific ways in which embedders can alter behaviors.

The most notable example of the delegate pattern is `URLRequest::Delegate`,
which keeps the vast majority of the loading logic within `URLRequest`,
but allows the `URLRequest::Delegate` to participate during specific times
in the request lifetime and alter specific behaviors as necessary. (Note:
`URLRequest::Delegate`, like much of the original `//net` code, doesn't
adhere to the recommended lifetime patterns of passing ownership of the
Delegate. It is from the experience debugging and supporting these APIs
that the `//net` team strongly encourages all new code pass explicit
ownership, to reduce the complexity and risk of lifetime issues).

While the use of a `base::Callback` can also be considered a form of
delegation, the `//net` layer tries to eschew any callbacks that can be
called more than once, and instead favors defining class interfaces
with concrete behavioral requirements in order to ensure the correct
lifetimes of objects and to adjust over time. When `//net` takes a
callback (e.g. `net::CompletionCallback`), the intended pattern is to
signal the asynchronous completion of a single method, invoking that
callback at most once before deallocating it. For more discussion
of these patterns, see [Code Patterns](code-patterns.md).

### Understanding the Layering

A significant challenge many feature proposals face is understanding the
layering in `//net` and what different portions of `//net` are allowed to
know. 

#### Socket Pools

The most common challenge feature proposals encounter is the awareness
that the act of associating an actual request to make with a socket is
done lazily, referred to as "late-binding".

With late-bound sockets, a given `URLRequest` will not be assigned an actual
transport connection until the request is ready to be sent. This allows for
reprioritizing requests as they come in, to ensure that higher priority requests
get preferential treatment, but it also means that features or data associated
with a `URLRequest` generally don't participate in socket establishment or
maintenance.

For example, a feature that wants to associate the low-level network socket
with a `URLRequest` during connection establishment is not something that the
`//net` design supports, since the `URLRequest` is kept unaware of how sockets
are established by virtue of the socket pools and late binding. This allows for
more flexibility when working to improve performance, such as the ability to
coalesce multiple logical 'sockets' over a single HTTP/2 or QUIC stream, which
may only have a single physical network socket involved.

#### Making Additional Requests

From time to time, `//net` feature proposals will involve needing to load
a secondary resource as part of processing. For example, feature proposals have
involved fetching a `/.well-known/` URI or reporting errors to a particular URL.

This is particularly challenging, because often, these features are implemented
deeper in the network stack, such as [`//net/cert`](../cert), [`//net/http`](../http),
or [`//net/filter`](../filter), which [`//net/url_request`](../url_request) depends
on. Because `//net/url_request` depends on these low-level directories, it would
be a circular dependency to have these directories depend on `//net/url_request`,
and circular dependencies are forbidden.

The recommended solution to address this is to adopt the delegation or injection
patterns. The lower-level directory will define some interface that represents the
"I need this URL" request, and then elsewhere, in a directory allowed to depend
on `//net/url_request`, an implementation of that interface/delegate that uses
`//net/url_request` is implemented.

### Understanding the Lifetimes

Understanding the object lifetime and dependency graph can be one of the largest
challenges to contributing and maintaining `//net`. As a consequence, features
which require introducing more complexity to the lifetimes of objects generally
have a greater challenge to acceptance.

The `//net` stack is designed heavily around a sync-or-async pattern, as
documented in [Code Patterns](code-patterns.md), while also having a strong
requirement that it be possible to cleanly shutdown the network stack. As a
consequence, features should have precise, well-defined lifetime semantics
and support graceful cleanup. Further, because much of the network stack can
have web-observable side-effects, it is often required for tasks to have
defined sequencing that cannot be reordered. To be ensure these requirements
are met, features should attempt to model object lifetimes as a hierarchical
DAG, using explicit ownership and avoiding the use of reference counting or
weak pointers as part of any of the exposed API contracts (even for features
only consumed in `//net`). Features that pay close attention to the lifetime
semantics are more likely to be reviewed and accepted than those that leave
it ambiguous.

In addition to preferring explicit lifetimes, such as through judicious use of
`std::unique_ptr<>` to indicate ownership transfer of dependencies, many
features in `//net` also expect that if a `base::Callback` is involved (which
includes `net::CompletionCallback`), then it's possible that invoking the
callback may result in the deletion of the current (calling) object. As
further expanded upon in [Code Patterns](code-patterns.md), features and
changes should be designed such that any callback invocation is the last
bit of code executed, and that the callback is accessed via the stack (such
as through the use of `std::move(callback_).Run()`.

### Specs: What Are They Good For

As `//net` is used as the basis for a number of browsers, it's an important part
of the design philosophy to ensure behaviors are well-specified, and that the
implementation conforms to those specifications. This may be seen as burdensome
when it's unclear whether or not a feature will 'take off,' but it's equally
critical to ensure that the Chromium projects do not fork the Web Platform.

#### Incubation Is Required

`//net` respects Chromium's overall position of [incubation first](https://groups.google.com/a/chromium.org/d/msg/blink-dev/PJ_E04kcFb8/baiLN3DTBgAJ) standards development.

With an incubation first approach, before introducing any new features that
might be exposed over the wire to servers, whether they are explicit behaviors,
such as adding new headers, or implicit behaviors such as
[Happy Eyeballs](https://tools.ietf.org/html/rfc6555), should have some form
of specification written. That specification should at least be on an
incubation track, and the expectation is that the specification should have a
direct path to an appropriate standards track. Features which don't adhere to
this pattern, or which are not making progress towards a standards track, will
require high-level approvals, to ensure that the Platform doesn't fragment.

#### Introducing New Headers

A common form of feature request is the introduction of new headers, either via
the `//net` implementation directly, or through consuming `//net` interfaces
and modifying headers on the fly.

The introduction of any additional headers SHOULD have an incubated spec attached,
ideally with cross-vendor interest. Particularly, headers which only apply to
Google or Google services are very likely to be rejected outright.

#### Making Additional Requests

While it's necessary to provide abstraction around `//net/url_request` for
any lower-level components that may need to make additional requests, for most
features, that's not all that is necessary. Because `//net/url_request` only
provides a basic HTTP fetching mechanism, it's insufficient for any Web Platform
feature, because it doesn't consider the broader platform concerns such as
interactions with CORS, Service Workers, cookie and authentication policies, or
even basic interactions with optional features like Extensions or SafeBrowsing.

To account for all of these things, any resource fetching that is to support
a feature of the Web Platform, whether because the resource will be directly
exposed to web content (for example, an image load or prefetch) or because it
is in response to invoking a Web Platform API (for example, invoking the
credential management API), the feature's resource fetching should be
explainable in terms of the  [Fetch Living Standard](https://fetch.spec.whatwg.org/).
The Fetch standard defines a JavaScript API for fetching resources, but more
importantly, defines a common set of infrastructure and terminology that
tries to define how all resource loads in the Web Platform happen - whether
it be through the JavaScript API, through `XMLHttpRequest`, or the `src`
attribute in HTML tags, for example.

This also includes any resource fetching that wishes to use the same socket
pools or caches as the Web Platform, to ensure that every resource that is
web exposed (directly or indirectly) is fetched in a consistent and
well-documented way, thus minimizing platform fragmentation and security
issues.

There are exceptions to this, however, but they're generally few and far
between. In general, any feature that needs to define an abstraction to
allow it to "fetch resources," likely needs to also be "explained in terms
of Fetch".

## Implementation

In general, prior to implementing, try to get a review on net-dev@chromium.org
for the general feedback and design review.

In addition to the net-dev@chromium.org early review, `//net` requires that any
browser-exposed behavior should also adhere to the
[Blink Process](https://www.chromium.org/blink#new-features), which includes an
"Intent to Implement" message to blink-dev@chromium.org

For features that are unclear about their future, such as experiments or trials,
it's also expected that the design planning will also account for how features
will be removed cleanly. For features that radically affect the architecture of
`//net`, expect a high bar of justification, since reversing those changes if
they fail to pan out can cause significant disruption to productivity and
stability.

## Deprecation

Plan for obsolence, hope for success. Similar to implementation, features that
are to be removed should also go through the
[Blink Process](https://www.chromium.org/blink#TOC-Web-Platform-Changes:-Process)
for removing features.

Note that due to the diversity of [Supported Projects](supported-projects.md),
removing a feature while minimizing disruption can be just as complex as adding
a feature. For example, relying solely on __User Metrics (UMA)__ to signal the
safety of removing a feature may not consider all projects, and relying on
__Field Trials (Finch)__ to assess risk or restore the 'legacy' behavior may not
work on all projects either.

It's precisely because of these challenges that there's such a high bar for
adding features, because they may represent multi-year commitments to support,
even when the feature itself is deprecated or targeted for removal.
