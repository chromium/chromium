Blink Public API
================

This directory contains the public API for Blink. The API consists of a number
of C++ header files, scripts, and GN build files. We consider all other files
in Blink to be implementation details, which are subject to change at any time
without notice.

The primary consumer of this API is Chromium's Content layer. If you are
interested in using Blink, please consider interfacing with Blink via the
Content layer rather than interfacing directly with this API.

Compatibility
-------------

The API does not support binary compatibility. Instead, the API is intended to
insulate the rest of the Chromium project from internal changes to Blink.  Over
time, the API is likely to evolve in source-incompatible ways as Chromium's and
Blink's needs change.

Organization
------------

The API is organized into three parts:

  - public/common
  - public/platform
  - public/web

The public/common directory contains public headers for the Web Platform stuff
that can be referenced both from renderer-side and browser-side code, also
from outside the Blink directory (e.g. from `//content` and `//chrome`).
Anything in this directory should **NOT** depend on other Blink headers.
Corresponding .cc code normally lives in `blink/common`, and public `.mojom`
files live in `blink/public/mojom`.  See `DEPS` and `blink/common/README.md`
for more details.

The public/platform directory defines an abstract platform upon which Blink
runs. Rather than communicating directly with the underlying operating system,
Blink is designed to run in a sandbox and interacts with the operating system
via the platform API. The central interface in this part of the API is
Platform, which is a pure virtual interface from which Blink obtains many other
interfaces. public/platform/ is implemented by blink/renderer/platform/exported.

The public/web directory defines an interface to Blink's implementation of the
web platform, including the Document Object Model (DOM). The central interface
in this part of the API is WebView, which is a good starting point for
exploring the API. public/web/ is implemented by
blink/renderer/{core,modules}/exported/.

Note that public/platform should not depend on public/web.

Notes
-----

As mentioned above, conceptually public/platform/ and public/web/ should be
used as follows:

  - public/platform/ is implemented by the underlying functionalities and used by Blink
  - public/web/ is implemented by Blink and used by Chromium

In reality, however, they are sometimes abused. Due to the dependency constraint
(public/web/ => controller/ => modules/ => core/ => platform/ => public/platform/),
people sometimes define classes in public/platform/ or public/web/ just because
that place is more convenient. This is already happening in many places but keep
the concept in mind.

Basic Types
-----------

The API does not use STL types, except for a small number of STL types that are
used internally by Blink (e.g., std::pair). Instead, we use WTF containers to
implement the API.

The API uses some internal types (e.g., blink::Node). Typically, these types
are forward declared and are opaque to consumers of the API. In other cases,
the full definitions are available behind the INSIDE_BLINK preprocessor macro.
In both cases, we continue to regard these internal types as implementation
details of Blink, and consumers of the API should not rely upon these types.

Similarly, the API uses STL types outside of the INSIDE_BLINK preprocessor
macro, which is for the convenience of the consumer.

Naming Conventions
------------------

The public/common directory doesn't need to use 'Web' prefix for their classes,
structs, and enums. But, public/platform/ and public/web/ directories keep using
'Web' prefix for the internal consistency.

Contact Information
-------------------

The public API also contains an OWNERS file, which lists a number of people who
are knowledgeable about the API. If you have questions or comments about the
API that might be of general interest to the Blink community at large, please
consider directing your inquiry to blink-dev@chromium.org rather than to the
OWNERS specifically.
