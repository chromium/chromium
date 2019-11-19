# Viz Directory Structure

The Viz (Visuals) service is a collection of subservices: compositing, gl, hit
testing, and media. Viz bugs are tracked with the Internals>Viz component if
no more specific component (e.g. Internals>Compositing, Internals>GPU) would
serve.

Viz has two types of clients: a single privileged client and one or more
unprivileged clients.

The privileged client is responsible for starting and restarting Viz after a
crash and for facilitating connections to Viz from unprivileged clients. The
privileged client is trusted by all other clients, and is expected to be
long-lived and not prone to crashes.

Unprivileged clients request connections to Viz through the privileged client
such as the browser process or the window server. Furthermore, unprivileged
clients may be malicious or may crash at any time. Unprivileged clients are
expected to be mutually distrusting of one another. Thus, an unprivileged client
cannot be provided interfaces by which it can impact the operation of another
client.

For example, a channel to the GL service can only be dispensed by the privileged
client, but can be used by unprivileged clients. GL commands are exposed as a
stable public API to the command buffer by the client library whereas the
underlying IPC messages and their semantics are constantly changing and
meaningless without deep knowledge of implementation details.

We propose the following directory structure to accommodate Viz's needs.

```
//services/viz/public/interfaces/{compositing, gl, hit_test, media}
//services/viz/public/<language>/{compositing, gl, hit_test, media}
```

The interfaces directories contain mojoms that define the public, unprivileged
interface for the Viz subservices. Clients may directly use the mojo interfaces
in these directories or choose to use the client library in a public/<language>
directory if one exists for a given mojom. private and privileged interfaces
described below may depend on public interfaces.

```
//services/viz/public/<language>/{compositing, gl, hit_test, media}:data_types
```

Common data types such as CompositorFrame, and GpuMemoryBufferHandle can live in
`//services/viz/public/<language>/{compositing, gl, hit_test, media}` under the
data_types target.

Their associated mojoms can live in:

```
//services/viz/public/interfaces/{compositing, gl, hit_test, media}
```

Note:

`//services/viz/public/<language>/{compositing, gl, hit_test, media}:data_types`
holds C++ types only and does not depend on
`//services/viz/public/interfaces/{compositing, gl, hit_test, media}`. Instead,
there are StructTraits with the interfaces that produce/consume data_types for
mojo transport.

```
//services/viz/private/interfaces/{compositing, gl, hit_test, media}
```

These interfaces directories contain mojoms that may only be used by going
through a language-specific client library. They are meant for unprivileged use,
without direct access to the mojoms. As such, only the
//services/viz/public/<language> and //services/viz/privileged/<language>
directories may depend on private, while other directories including interface
directories must not. There is no private client library, as these are meant for
consumption by the public client library.

```
//services/viz/privileged/interfaces/{compositing, gl, hit_test, media}
//services/viz/privileged/<language>/{compositing, gl, hit_test, media}
```

The interfaces directories contains mojoms that may only be used by the
privileged client. Privileged interfaces are kept in separate directories to
facilitate security reviews. These interfaces may be used directly or through
the a privileged/<language> client library. The public and private interfaces
must not depend on privileged interfaces. Typically, the browser process or the
window server serves as the privileged client to Viz.

```
//services/viz/main
```

This is the glue code that implements the primordial VizMain interface (in
`//services/viz/privileged/interfaces/main`) that starts up the Viz process
through the service manager. VizMain is a factory interface that enables the
privileged client to instantiate the Viz subservices: compositing, gl, hit_test,
and media.

```
//services/viz/{compositing, gl, hit_test, media}/service
```

Service-side implementation code live in the various sub-service "service"
directories. Service code may depend on the `public/<language>/…:data_types`
target and interfaces subdirectories, but cannot depend on any of the
`//service/viz/public/<language>/...` client library.

## Short term: `//components/viz`,  `//gpu`, `//media`
At this time, the Viz public client library for the compositing and hit_test
subservices live in `//components/viz/client`, and the privileged client library
lives in `//components/viz/host`.

Command buffer code will continue to live in `//gpu` and media code will continue
to live in `//media`.

Once the content module has been removed (or no longer depends on
`components/viz/service`), the code in `//components/viz/client` will move to
appropriate destinations in `//services/viz/public/<language>/...`.
`//components/viz/service` will move to the appropriate service directories in
`//services/viz/...`. `//components/viz/host` will move to
`//services/viz/privileged/<language>/{compositing, gl, hit_test, media}`.

## Acceptable Dependencies
Note: `=>` means can depend on

Unprivileged client, can depend on
```
  services/viz/public/<language>/{compositing, gl, hit_test, media} =>
    services/viz/public/interfaces/{compositing, gl, hit_test, media} =>
      services/viz/public/<language>/{compositing, gl, hit_test, media}:data_types
    services/viz/private/interfaces/{compositing, gl, hit_test, media} =>
      services/viz/public/<language>/{compositing, gl, hit_test, media}:data_types
  services/viz/public/interfaces/{compositing, gl, hit_test, media}
```

The privileged client can depend on
```
  services/viz/privileged/<language>/{compositing, gl, hit_test, media} =>
    services/viz/privileged/interfaces/{compositing, gl, hit_test, media}
  services/viz/public/interfaces/{compositing, gl, hit_test, media}
  services/viz/public/<language>/{compositing, gl, hit_test, media}
```

Services can depend on:
```
  services/viz/public/interfaces/{compositing, gl, hit_test, media}
  services/viz/public/<language>/{compositing, gl, hit_test, media}:data_types
  services/viz/privileged/interfaces/{compositing, gl, hit_test, media}
  services/viz/private/interfaces/{compositing, gl, hit test, media}
```

