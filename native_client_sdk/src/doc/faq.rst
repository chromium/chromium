.. include:: /migration/deprecation.inc

##########################
Frequently Asked Questions
##########################

.. contents::
  :local:
  :backlinks: none
  :depth: 2

This document answers some frequently asked questions about Native
Client (NaCl) and Portable Native Client (PNaCl, pronounced
"pinnacle"). For a high-level overview of Native Client, see the
:doc:`Technical Overview <overview>`.

If you have questions that aren't covered in this FAQ:

* Scan through the :doc:`Release Notes <sdk/release-notes>`.
* Search through or ask on the :doc:`Native Client Forums <help>`.


What is Native Client Good For?
===============================

Why did Google build Native Client?
-----------------------------------

* **Performance:** Native Client modules run nearly as fast as native
  compiled code.
* **Security:** Native Client lets users run native compiled code in the
  browser with the same level of security and privacy as traditional web
  applications.
* **Convenience:**

  * Developers can leverage existing code, written in C/C++ or other
    languages, in their applications without forcing users to install a
    plugin.
  * This code can interact with the embedding web page as part of an
    HTML and JavaScript web application, or it can be a self-contained
    and immersive experience.

* **Portability:** Native Client and Portable Native Client applications
  can execute on:

  * The Windows, macOS, Linux or Chrome OS operating systems.
  * Processors with the x86-32, x86-64, or ARM instruction set
    architectures. Native Client also has experimental support for MIPS.

Portable Native client further enhances the above:

* **Performance:** Each PNaCl release brings with it more performance
  enhancements. Already-released applications get faster over time,
  conserving user's battery.
* **Security:** Users are kept secure with an ever-improving sandbox
  model which adapts to novel attacks, without affecting
  already-released applications.
* **Convenience:** Developers only need to ship a single ``.pexe`` file,
  not one ``.nexe`` file per supported architecture.
* **Portability:** Developers and users don't need to worry about
  already-released applications not working on new hardware: PNaCl
  already supports all architectures NaCl does, and as PNaCl evolves it
  gains support for new processors and fully uses their capabilities.

.. TODO Expand on the PNaCl performance section in another document, and
.. link to it here. How does one profile PNaCl code? What are common
.. causes of slowness? How can code be made faster? What's the best way
.. to use Pepper's asynchronous APIs? What do I need to know about
.. threads and inter-thread communications? Can I use SIMD or other
.. processor-specific instructions? What about the GPU?

For more details, refer to the :doc:`history behind and comparison of
NaCl and PNaCl <nacl-and-pnacl>`.

When should I use Portable Native Client instead of Native Client?
------------------------------------------------------------------

See :doc:`NaCl and PNaCl <nacl-and-pnacl>`. In short: PNaCl works on the Open
Web platform delivered by Chrome whereas NaCl only works on the Chrome Web
Store.

When should I use Portable Native Client / Native Client?
---------------------------------------------------------

The following are some typical use cases. For details, see the
:doc:`Technical Overview <overview>`.

* Porting existing applications or software components, written in C/C++ or
  virtual machines written in C/C++, for use in a web application.
* Using compute-intensive applications, including threads and SIMD, such as:

  * Scientific computing.
  * Handling multimedia for a web application.
  * Various aspects of web-based games, including physics engines and AI.

* Running untrusted code on a server or within an application (such as a plugin
  system for a game).

Portable Native Client and Native Client are versatile technologies which are
used in many other contexts outside of Chrome.

How fast does code run in Portable Native Client?
-------------------------------------------------

Fast! The SPEC2k benchmarks (C, C++ and floating-point benchmarks) give
the following overhead for optimized PNaCl compared to regular optimized
LLVM:

+--------+-----+
| x86-32 | 15% |
+--------+-----+
| x86-64 | 25% |
+--------+-----+
|  ARM   | 10% |
+--------+-----+

Note that benchmark performance is sometimes bimodal, so different use
cases are likely to achieve better or worse performance than the above
averages. For example floating-point heavy code usually exhibits much
lower overheads whereas very branch-heavy code often performs worse.

Note that PNaCl supports performance features that are often used in
native code such as :ref:`threading <language_support_threading>` and
:ref:`Portable SIMD Vectors <portable_simd_vectors>`.

For details, see:

* `PNaCl SIMD: Speed on the Web`_.
* `Adapting Software Fault Isolation to Contemporary CPU Architectures`_ (PDF).
* `Native Client: A Sandbox for Portable, Untrusted x86 Code`_ (PDF).

If your code isn't performing as close to native speed as you'd expect,
:doc:`let us know <help>`!

.. TODO Link to the non-existent performance page! (see above todo).

Why use Portable Native Client instead of *<technology X>*?
-----------------------------------------------------------

Many other technologies can be compared to Portable Native Client:
Flash, Java, Silverlight, ActiveX, .NET, asm.js, etc...

Different technologies have different strengths and weaknesses. In
appropriate contexts, Portable Native Client can be faster, more secure,
and/or more compatible across operating systems and architectures than
other technologies.

Portable Native Client complement other technologies by giving web
developers a new capability: the ability to run fast, secure native code
from a web browser in an architecture-independent way.

If I want direct access to the OS, should I use Native Client?
--------------------------------------------------------------

No---Native Client does not provide direct access to the OS or devices,
or otherwise bypass the JavaScript security model. For more information,
see later sections of this FAQ.


Development Environments and Tools
==================================

What development environment and development operating system do you recommend?
-------------------------------------------------------------------------------

You can develop on Windows, macOS, or Linux, and the resulting Native Client or
Portable Native Client application will run inside the Google Chrome browser on
all those platforms as well as Chrome OS. You can also develop on Chrome OS with
Crouton_ or our `experimental development environment which runs within NaCl`_,
and we're working on self-hosting a full development environment on Portable
Native Client.

Any editor+shell combination should work as well as IDEs like Eclipse,
Visual Studio with the :doc:`Native Client Add-In
<devguide/devcycle/vs-addin>` on Windows, or Xcode on macOS.

I'm not familiar with native development tools, can I still use the Native Client SDK?
--------------------------------------------------------------------------------------

You may find our :doc:`Tutorial <devguide/tutorial/index>` and :doc:`Building
instructions <devguide/devcycle/building>` useful, and you can look at
the code and Makefiles for the SDK examples to understand how the
examples are built and run.

You'll need to learn how to use some tools (like GCC, LLVM, make, Eclipse,
Visual Studio, or Xcode) before you can get very far with the SDK. Try seaching
for an `introduction to GCC`_.


Openness, and Supported Architectures and Languages
===================================================

Is Native Client open? Is it a standard?
----------------------------------------

Native Client is completely open: the executable format is open and the
`source code is open <https://code.google.com/p/nativeclient/>`_. Right
now the Native Client project is in its early stages, so it's premature
to consider Native Client for standardization.

We consistenly try to document our design and implementation and hope to
standardize Portable Native Client when it gains more traction. A good
example is our :doc:`PNaCl bitcode reference manual
<reference/pnacl-bitcode-abi>`.

How can I contribute to Native Client?
--------------------------------------

Read about :doc:`contributor ideas <reference/ideas>`.

What are the supported instruction set architectures?
-----------------------------------------------------

Portable Native Client uses an architecture-independent format (the
``.pexe``) which can currently be translated to execute on processors
with the x86-32, x86-64, and ARM instruction set architectures, as well
as experimental support for MIPS. As new architectures come along and
become popular we expect Portable Native Client to support them without
developers having to recompile their code.

Native Client can currently execute on the same architectures as
Portable Native Client but is only supported on the Chrome Web
Store. Native Client's ``.nexe`` files are architecture-dependent and
cannot adapt to new architectures without recompilation, we therefore
deem them better suited to a web store than to the open web.

With Portable Native Client we deliver a system that has comparable
portability to JavaScript and can adapt to new instruction set
architectures without requiring recompilation. The web is better when
it's platform-independent, and we'd like it to stay that way.

.. _other_languages:

Do I have to use C or C++? I'd really like to use another language.
-------------------------------------------------------------------

Right now only C and C++ are supported directly by the toolchain in the SDK. C#
and other languages in the .NET family are supported via the `Mono port`_ for
Native Client. Moreover, there are several ongoing projects to support
additional language runtimes (e.g. `webports includes Lua, Python and Ruby`_)
as well as to compile more languages to LLVM's intermediate representation
(e.g. support Halide_, Haskell with GHC_ or support Fortran with flang_), or
transpile languages to C/C++ (source-to-source compilation). Even JavaScript is
supported by compiling V8_ to target PNaCl.

The PNaCl toolchain is built on LLVM and can therefore generate code from
languages such as Rust_, Go_, or Objective-C, but there may still be a few rough
edges.

If you're interested in getting other languages working, please contact the
Native Client team by way of the native-client-discuss_ mailing list, and read
through :doc:`contributor ideas <reference/ideas>`.

Do you only support Chrome? What about other browsers?
------------------------------------------------------

We aim to support multiple browsers. However, a number of features that
we consider requirements for a production-quality system that keeps the
user safe are difficult to implement without help from the
browser. Specific examples are an out-of-process plugin architecture and
appropriate interfaces for integrated 3D graphics. We have worked
closely with Chromium developers to deliver these features and we are
eager to collaborate with developers from other browsers.

What's the difference between NPAPI and Pepper?
-----------------------------------------------

:doc:`Pepper <pepper_stable/index>` (also known as PPAPI) is a new API that
lets Native Client modules communicate with the browser. Pepper supports
various features that don't have robust support in NPAPI, such as event
handling, out-of-process plugins, and asynchronous interfaces. Native
Client has transitioned from using NPAPI to using Pepper.

Is NPAPI part of the Native Client SDK?
---------------------------------------

NPAPI is not supported by the Native Client SDK, and is `deprecated in Chrome`_.

Does Native Client support SIMD vector instructions?
----------------------------------------------------

Portable Native Client supports portable SIMD vectors, as detailed in
:ref:`Portable SIMD Vectors <portable_simd_vectors>`.

Native Client supports SSE, AVX1, FMA3 and AVX2 (except for `VGATHER`) on x86
and NEON on ARM.

Can I use Native Client for 3D graphics?
----------------------------------------

Yes. Native Client supports `OpenGL ES 2.0`_.

To alert the user regarding their hardware platform's 3D feature set
before loading a large NaCl application, see :doc:`Vetting the driver in
Javascript <devguide/coding/3D-graphics>`.

Some GL extensions are exposed to Native Client applications, see the `GLES2
file`_.  This file is part of the GL wrapper supplied by the library
``ppapi_gles2`` which you'll want to include in your project.  In most cases
extensions map to extensions available on other platforms, or differ very
slightly (if they differ, the extension is usually CHROMIUM or ANGLE instead of
EXT).

.. TODO Improve documentation for GL extensions.

Does Native Client support concurrency/parallelism?
---------------------------------------------------

Native Client and Portable Native Client both support pthreads,
C11/C++11 threads, and low-level synchronization primitives (mutex,
barriers, atomic read/modify/write, compare-and-exchange, etc...), thus
allowing your Native Client application to utilize several CPU cores.
Note that this allows you to modify datastructures concurrently without
needing to copy them, which is often a limitation of shared-nothing
systems. For more information see :ref:`memory model and atomics
<memory_model_and_atomics>` and :ref:`threading
<language_support_threading>`.

Native Client doesn't support HTML5 Web Workers directly but can
interact with JavaScript code which does.


Coming Soon
===========

Do Native Client modules have access to external devices?
---------------------------------------------------------

At this time Native Client modules do not have access to serial ports,
camera devices, or microphones: Native Client can only use native
resources that today's browsers can access. However, we intend to
recommend such features to the standards bodies and piggyback on their
efforts to make these resources available inside the browser.

You can generally think of Pepper as the C/C++ bindings to the
capabilities of HTML5. The goal is for Pepper and JavaScript to evolve
together and stay on par with each other with respect to features and
capabilities.


Security and Privacy
====================

What happens to my data when I use Native Client?
-------------------------------------------------

Users can opt-in to sending usage statistics and crash information in
Chrome, which includes usage statistics and crash information about
Native Client. Crashes in your code won't otherwise send your
information to Google: Google counts the number of such crashes, but
does so anonymously without sending your application's data or its debug
information.

For additional information about privacy and Chrome, see the `Google Chrome
privacy policy`_ and the `Google Chrome Terms of Service`_.

How does Native Client prevent sandboxed code from doing Bad Things?
--------------------------------------------------------------------

Native Client's sandbox works by validating the untrusted code (the
compiled Native Client module) before running it. The validator checks
the following:

* **Data integrity:** No loads or stores are permitted outside of the
  data sandbox. In particular this means that once loaded into memory,
  the binary is not writable. This is enforced by operating system
  protection mechanisms. While new instructions can be inserted at
  runtime to support things like JIT compilers, such instructions will
  be subject to runtime verification according to the following
  constraints before they are executed.
* **No unsafe instructions:** The validator ensures that the Native
  Client application does not contain any unsafe instructions. Examples
  of unsafe instructions are ``syscall``, ``int``, and ``lds``.
* **Control flow integrity:** The validator ensures that all direct and
  indirect branches target a safe instruction.

The beauty of the Native Client sandbox is in reducing "safe" code to a
few simple rules that can be verified by a small trusted validator: the
compiler isn't trusted. The same applies to Portable Native Client where
even the ``.pexe`` to ``.nexe`` translator, a simplified compiler
backend, isn't trusted: it is validated before executing, and so is its
output.

In addition to static analysis of untrusted code, the Native Client runtime also
includes an outer sandbox that mediates system calls. For more details about
both sandboxes, see `Native Client: A Sandbox for Portable, Untrusted x86 Code`_
(PDF).

How does Google know that the safety measures in Native Client are sufficient?
------------------------------------------------------------------------------

Google has taken several steps to ensure that Native Client's security works,
including:

* Open source, peer-reviewed papers describing the design.
* A :doc:`security contest <community/security-contest/index>`.
* Multiple internal and external security reviews.
* The ongoing vigilance of our engineering and developer community.

Google is committed to making Native Client safer than JavaScript and other
popular browser technologies. If you have suggestions for security improvements,
let the team know, by way of the native-client-discuss_ mailing list.

Development
===========

How do I debug?
---------------

Instructions on :ref:`debugging the SDK examples
<debugging_the_sdk_examples>` using GDB are available. You can also
debug Native Client modules with some :doc:`alternative approaches
<devguide/devcycle/debugging>`.

How do I build x86-32, x86-64 or ARM ``.nexes``?
------------------------------------------------

By default, the applications in the ``/examples`` folder create
architecture-independent ``.pexe`` for Portable Native Client. To
generate a ``.nexe`` targeting one specific architecture using the
Native Client or Portable Native Client toolchains, see the
:doc:`Building instructions <devguide/devcycle/building>`.

How can my web application determine which ``.nexe`` to load?
-------------------------------------------------------------

Your application does not need to make the decision of loading an
x86-32, x86-64 or ARM ``.nexe`` explicitly---the Native Client runtime
examines a manifest file (``.nmf``) to pick the right ``.nexe`` file for
a given user. You can generate a manifest file using a Python script
that's included in the SDK (see the ``Makefile`` in any of the SDK
examples for an illustration of how to do so). Your HTML file specifies
the manifest filename in the ``src`` attribute of the ``<embed>``
tag. You can see the way the pieces fit together by examining the
examples included in the SDK.

Is it possible to build a Native Client module with just plain C (not C++)?
---------------------------------------------------------------------------

Yes. See the ``"Hello, World!"`` in C example in the SDK under
``examples/tutorial/using_ppapi_simple/``, or the Game of Life example
under ``examples/demo/life/life.c``.

What UNIX system calls can I make through Native Client?
--------------------------------------------------------

Native Client doesn't directly expose any system calls from the host OS
because of the inherent security risks and because the resulting
application would not be portable across operating systems. Instead,
Native Client provides portable cross-OS abstractions wrapping or
proxying OS functionality or emulating UNIX system calls. For example,
Native Client provides an ``mmap()`` system call that behaves much like
the standard UNIX ``mmap()`` system call.

Is my favorite third-party library available for Native Client?
---------------------------------------------------------------

Google has ported several third-party libraries to Native Client; such libraries
are available in the webports_ project. We encourage you to contribute
libraries to webports, and/or to host your own ported libraries, and to let the
team know about it on native-client-discuss_ when you do. You can also read
through :doc:`contributor ideas <reference/ideas>` to find ideas of new projects
to port.

Do all the files in an application need to be served from the same domain?
--------------------------------------------------------------------------

The ``.nmf``, and ``.nexe`` or ``.pexe`` files must either be served from the
same origin as the embedding page or an origin that has been configured
correctly using CORS_.

For applications installed from the Chrome Web Store the Web Store manifest
must include the correct, verified domain of the embedding page.

Portability
===========

Do I have to do anything special to make my application run on different operating systems?
-------------------------------------------------------------------------------------------

No. Native Client and Portable Native Client applications run without
modification on all supported operating systems.

However, to run on different instruction set architectures (such as
x86-32, x86-64 or ARM), you currently have to either:

* Use Portable Native Client.
* Build and supply a separate ``.nexe`` file for each architecture, and
  make them available on the Chrome Web Store. See :doc:`target
  architectures <devguide/devcycle/building>` for details about which
  ``.nexe`` files will run on which architectures.

How easy is it to port my existing native code to Native Client?
----------------------------------------------------------------

In most cases you won't have to rewrite much, if any, code. The Native
Client-specific tools, such as ``pnacl-clang++`` or ``x86_64-nacl-g++``,
take care of most of the necessary changes. You may need to make some
changes to your operating system calls and interactions with external
devices to work with the web. Porting existing Linux libraries is
generally straightforward, with large libraries often requiring no
source change.

The following kinds of code may be more challenging to port:

* Code that does direct `TCP <pepper_stable/cpp/classpp_1_1_t_c_p_socket>`_ or
  `UDP <pepper_stable/cpp/classpp_1_1_u_d_p_socket>`_ networking. For security
  reasons these APIs are only available to `Chrome apps </apps>`_ after asking
  for the appropriate permissions, not on the open web. Native Client is
  otherwise restricted to the networking APIs available in the browser. You may
  want to use to `nacl_io library <nacl_io>`_ to use POSIX-like sockets.
* Code that creates processes, including UNIX ``fork``, won't function
  as-is. However, threads are supported. You can nonetheless create new
  ``<embed>`` tags in your HTML page to launch new PNaCl processes. You can even
  use new ``.pexe`` files that your existing ``.pexe`` saved in a local
  filesystem. This is somewhat akin to ``execve``, but the process management
  has to go through ``postMessage`` to JavaScript in order to create the new
  ``<embed>``.
* Code that needs to do local file I/O. Native Client is restricted to accessing
  URLs and to local storage in the browser (the Pepper :doc:`File IO API
  <devguide/coding/file-io>` has access to the same per-application storage that
  JavaScript has via Local Storage). HTML5 File System can be used, among
  others. For POSIX compatabiliy the Native Client SDK includes a library called
  nacl_io which allows the application to interact with all these types of files
  via standard POSIX I/O functions (e.g. ``open`` / ``fopen`` / ``read`` /
  ``write`` / ...). See :doc:`Using NaCl I/O <devguide/coding/nacl_io>` for more
  details.

.. _faq_troubleshooting:

Troubleshooting
===============

My ``.pexe`` isn't loading, help!
---------------------------------

* You must use Google Chrome version 31 or greater for Portable Native
  Client. Find your version of chrome by opening ``about:chrome``, and `update
  Chrome <http://www.google.com/chrome/>`_ if you are on an older version. If
  you're already using a recent version, open ``about:components`` and "Check
  for update" for PNaCl. Note that on Chrome OS PNaCl is always up to date,
  whereas on other operating systems it updates shortly after Chrome updates.
* A PNaCl ``.pexe`` must be compiled with pepper_31 SDK or higher. :ref:`Update
  your bundles <updating-bundles>` and make sure you're using a version of
  Chrome that matches the SDK version. 
* Your application can verify that Portable Native Client is supported
  in JavaScript with ``navigator.mimeTypes['application/x-pnacl'] !==
  undefined``. This is preferred over checking the Chrome version.

My ``.nexe`` files never finish loading. What gives?
----------------------------------------------------

Here are ways to resolve some common problems that can prevent loading:

* You must use Google Chrome version 14 or greater for Native Client.
* If you haven't already done so, enable the Native Client flag in
  Google Chrome. Type ``about:flags`` in the Chrome address bar, scroll
  down to "Native Client", click the "Enable" link, scroll down to the
  bottom of the page, and click the "Relaunch Now" button (all browser
  windows will restart).
* Verify that the Native Client plugin is enabled in Google Chrome. Type
  ``about:plugins`` in the Chrome address bar, scroll down to "Native
  Client", and click the "Enable" link. (You do not need to relaunch
  Chrome after you enable the Native Client plugin).
* Make sure that the ``.nexe`` files are being served from a web
  server. Native Client uses the same-origin security policy, which
  means that modules will not load in pages opened with the ``file://``
  protocol. In particular, you can't run the examples in the SDK by
  simply dragging the HTML files from the desktop into the browser. See
  :doc:`Running Native Client Applications <devguide/devcycle/running>`
  for instructions on how to run the httpd.py mini-server included in
  the SDK.
* The ``.nexe`` files must have been compiled using SDK version 0.5 or
  greater.
* You must load the correct ``.nexe`` file for your machine's specific
  instruction set architecture (x86-32, x86-64 or ARM). You can ensure
  you're loading the correct ``.nexe`` file by building a separate
  ``.nexe`` for each architecture, and using a ``.nmf`` manifest file to
  let the browser select the correct ``.nexe`` file. Note: the need to
  select a processor-specific ``.nexe`` goes away with Portable Native
  Client.
* If things still aren't working, :doc:`ask for help <help>`!


.. _`PNaCl SIMD: Speed on the Web`: https://www.youtube.com/watch?v=675znN6tntw&list=PLOU2XLYxmsIIwGK7v7jg3gQvIAWJzdat_
.. _Adapting Software Fault Isolation to Contemporary CPU Architectures: https://nativeclient.googlecode.com/svn/data/site/NaCl_SFI.pdf
.. _`Native Client: A Sandbox for Portable, Untrusted x86 Code`: http://research.google.com/pubs/pub34913.html
.. _Crouton: https://github.com/dnschneid/crouton
.. _experimental development environment which runs within NaCl: https://www.youtube.com/watch?v=OzNuzBDEWzk&list=PLOU2XLYxmsIIwGK7v7jg3gQvIAWJzdat_
.. _introduction to GCC: https://www.google.com/search?q=gcc+introduction
.. _Mono port: https://github.com/elijahtaylor/mono
.. _webports includes Lua, Python and Ruby: https://chromium.googlesource.com/webports
.. _Halide: http://halide-lang.org/
.. _GHC: http://www.haskell.org/ghc/docs/latest/html/users_guide/code-generators.html
.. _flang: https://flang-gsoc.blogspot.ie/2013/09/end-of-gsoc-report.html
.. _V8: https://code.google.com/p/v8/
.. _Rust: http://www.rust-lang.org/
.. _Go: https://golang.org
.. _native-client-discuss: https://groups.google.com/group/native-client-discuss
.. _deprecated in Chrome: http://blog.chromium.org/2013/09/saying-goodbye-to-our-old-friend-npapi.html
.. _OpenGL ES 2.0: https://www.khronos.org/opengles/
.. _GLES2 file: https://source.chromium.org/chromium/chromium/src/+/HEAD:ppapi/lib/gl/gles2/gles2.c
.. _Google Chrome privacy policy: https://www.google.com/chrome/intl/en/privacy.html
.. _Google Chrome Terms of Service: https://www.google.com/chrome/intl/en/eula_text.html
.. _webports: https://chromium.googlesource.com/webports
.. _CORS: http://en.wikipedia.org/wiki/Cross-origin_resource_sharing
