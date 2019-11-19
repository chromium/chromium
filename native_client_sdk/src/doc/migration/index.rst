.. _migration:

WebAssembly Migration Guide
===========================

(P)NaCl Deprecation Announcements
---------------------------------

Given the momentum of cross-browser WebAssembly support, we plan to focus our
native code efforts on WebAssembly going forward and plan to remove support for
PNaCl in Q4 2019 (except for Chrome Apps). We believe that the vibrant
ecosystem around `WebAssembly <http://webassembly.org>`_
makes it a better fit for new and existing high-performance
web apps and that usage of PNaCl is sufficiently low to warrant deprecation.

As of Chrome 76, PNaCl on the open web has been moved behind an
`Origin Trial
<https://github.com/GoogleChrome/OriginTrials/blob/gh-pages/developer-guide.md>`_,
which is a mechanism for web developers to register and get access to a feature that isn't on by default.
This is usually a new proposed feature but in this case it's a feature being deprecated.
A developer can register on the `Origin Trial Console
<https://developers.chrome.com/origintrials/#/view_trial/3553340105995321345>`_
and receive a token, which can be embedded into a page and will enable the feature without the user needing to use a flag.
(For more details see the linked guide). The trial is scheduled to last through Chrome 78, approximately until December 2019.
This change is not intended to affect NaCl or PNaCl in Chrome Apps or extensions, and the "enable-nacl"
flag in chrome://flags can also be used to enable PNaCl locally for testing
(this flag also retains its current function of enabling non-PNaCl "native" NaCl on any page).

We also recently announced the deprecation Q1 2018 of
`Chrome Apps
<https://blog.chromium.org/2016/08/from-chrome-apps-to-web.html>`_
outside of Chrome OS.


Toolchain Migration
-------------------

For the majority of (P)NaCl uses cases we recommend transitioning
from the NaCl SDK to `Emscripten
<http://webassembly.org/getting-started/developers-guide/>`_.
Migration is likely to be reasonably straightforward
if your application is portable to Linux, uses
`SDL <https://www.libsdl.org/>`_, or POSIX APIs.
While direct support for NaCl / Pepper APIs in not available,
we've attempted to list Web API equivalents.
For more challenging porting cases, please reach out on
native-client-discuss@googlegroups.com


API Migration
-------------

We've outlined here the status of Web Platform substitutes for each
of the APIs exposed to (P)NaCl.
Additionally, the table lists the library or option in Emscripten
that offers the closest substitute.

We expect to add shared memory threads support to WebAssembly in 2017,
as threads are crucial to matching (P)NaCl's most interesting use
cases. Migration items which assume forthcoming threads support
are marked below. If your application's flow control relies heavily on blocking
APIs, you may also find threads support is required for convenient porting.

While we've tried to be accurate in this table,
there are no doubt errors or omissions.
If you encounter one, please reach out to us on
native-client-discuss@googlegroups.com

.. contents::
  :local:
  :backlinks: none
  :depth: 2

PPAPI
-----
.. raw:: html
  :file: public.html

IRT
---
.. raw:: html
  :file: public.html

PPAPI (Apps)
------------
.. raw:: html
  :file: apps.html
