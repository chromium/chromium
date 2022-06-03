# ES Modules in Service Workers

This document details the current state of ES module support in Service Workers.

## Introduction

ES modules have been a developer favorite for a while now — in addition to a
[number of other benefits](https://tc39.es/ecma262/#sec-modules), they offer the
promise of a universal module format where shared code can be released once and
run in browsers, as well as in alternative runtimes like Node.js. The spec can be
found [here](https://w3c.github.io/ServiceWorker/#service-worker-concept) (refer
to type in the section).

The ideal use case for ES modules inside of Service Workers is for loading in a
modern library or configuration code that's shared with other runtimes that
support ES modules.

A module script can be used by setting type to 'module' when registering the
Service Worker. The feature can be used in the following way.

```
// index.html
const registration = await navigator.serviceWorker.register(‘sw.js’, {type: ‘module’});

// sw.js (called as ‘main script’ or ‘top-level script’)
import * from ‘./imported_script.js’; // static import
```

## Current limitations
### Static imports only
ES modules can be imported in one of two ways: either statically, using the
import ... from '...' syntax, or dynamically, using an import('...') statement.
Inside of a Service Worker, only the static import is currently supported.
Dynamic import is currently blocked in Service Workers, but it will change in
the future. The status of supporting dynamic import inside a Service Worker
can be tracked [here](https://github.com/w3c/ServiceWorker/issues/1585).

This limitation is analogous to a similar restriction placed on importScripts()
usage. Dynamic calls to importScripts() do not work inside of a Service Worker,
and all importScripts() calls, which are inherently synchronous, must complete
before the Service Worker completes its install phase. This restriction ensures
that the browser knows about, and is able to store, all JavaScript
code needed for a Service Worker's implementation during installation.

### No support for import maps
Import maps allow runtime environments to rewrite module specifiers, to, for
example, prepend the URL of a preferred CDN from which the ES modules can be
loaded.

While Chrome version 89 and above supports import maps, they currently cannot
be used with Service Workers.
