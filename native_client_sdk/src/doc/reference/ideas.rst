.. _ideas:

.. include:: /migration/deprecation.inc

#################
Contributor Ideas
#################

.. contents::
   :local:
   :backlinks: none
   :depth: 3

Contributing? Me‽
=================

NaCl and PNaCl are very big projects: they expose an entire operating system to
developers, interact with all of the Web platform, and deal with compilers
extensively to allow code written in essentially any programming language to
execute on a variety of CPU architectures. This can be daunting when trying to
figure out how to contribute to the open-source project! This page tries to make
contributing easier by listing project ideas by broad area of interest, and
detailing the required experience and expectations for each idea.

This isn't meant to constrain contributions! If you have ideas that aren't on
this page please contact the native-client-discuss_ mailing list.

If you like an idea on this page and would like to get started, contact the
native-client-discuss_ mailing list so that we can help you find a mentor.

.. _native-client-discuss: https://groups.google.com/group/native-client-discuss

Google Summer of Code
=====================

PNaCl participates in the `2015 Google Summer of Code`_ (see the `PNaCl GSoC
page`_). `Student applications`_ are open March 16–27. Discuss project ideas no
native-client-discuss_, and submit your proposal on the GSoC page by the
deadline.

.. _PNaCl GSoC page: https://www.google-melange.com/gsoc/org2/google/gsoc2015/pnacl
.. _2015 Google Summer of Code: https://www.google-melange.com/gsoc/homepage/google/gsoc2015
.. _Student applications: https://www.google-melange.com/gsoc/document/show/gsoc_program/google/gsoc2015/help_page#4._How_does_a_student_apply

Ideas
=====

We've separated contributor ideas into broad areas of interest:

* **Ports** encompass all the code that *uses* the PNaCl platform. Put simply,
  the point of ports is to make existing open-source code work.
* **Programming languages** sometimes involves compiler work, and sometimes
  requires getting an interpreter and its APIs to work well within the Web
  platform.
* **LLVM and PNaCl** requires compiler work: PNaCl is based on the LLVM
  toolchain, and most of the work in this area would occur in the upstream LLVM
  repository.
* **NaCl** mostly deals with low-level systems work and security.


..
  Adding a proposal to this document should follow this format:
    Project:                *project title*
    Brief explanation:      *brief description*
    Expected results:       *how do we evaluate the project's success?*
    Knowledge Prerequisite: *programming languages, CS topics, ...*
    Mentor:                 *one or multiple, their roles in this project*
  The above list is inspired by the Google Summer of Code guidelines, and the
  KDE project list.

Ports
-----

New Filesystems
^^^^^^^^^^^^^^^

* **Project:** Expose new filesystems to :doc:`nacl_io
  </devguide/coding/nacl_io>`.
* **Brief explanation:** nacl_io exposes filesystems like html5fs and RAM disk,
  which can be mounted and then accessed through regular POSIX APIs. New types
  of filesystems could be exposed in a similar way, allowing developers to build
  apps that "just work" on the Web platform while using Web APIs. A few ideas
  include connecting to: Google Drive, Github, Dropbox.
* **Expected results:** A new filesystem is mountable using nacl_io, is well
  tested, and used in a demo application.
* **Knowledge Prerequisite:** C++.
* **Mentor:** Sam Clegg.

Open Source Porting
^^^^^^^^^^^^^^^^^^^

* **Project:** Port substantial open source projects to work in webports.
* **Brief explanation:** webports contains a large collection of open source
  projects that properly compile and run on the PNaCl platform. This project
  involves adding new useful projects to webports, and upstreaming any patches
  to the original project: running on PNaCl effective involves porting to a new
  architecture and operating system. Project ideas include: Gimp, Inkscape, Gtk.
* **Expected results:** New open source projects are usable from webports.
* **Knowledge Prerequisite:** C/C++.
* **Mentor:** Brad Nelson.


Languages
---------

PNaCl already has support for C and C++, and virtual machines such as
JavaScript, Lua, Python and Ruby. We'd like to support more languages, either by
having these languages target LLVM bitcode or by making sure that the language
virtual machine's APIs work well on the Web platform.

Rust
^^^^

* **Project:** Support the Rust programming languages.
* **Brief explanation:** The Rust_ programming language uses LLVM. The aim of
  this project is to allow it to deliver PNaCl ``.pexe`` files.
* **Expected results:** The Rust test suite passes within the browser. How to
  use Rust to target PNaCl is well documented and easy to do.
* **Knowledge Prerequisite:** Compilers, LLVM.
* **Mentor:** Ben Smith.

.. _Rust: http://www.rust-lang.org

Haskell
^^^^^^^

* **Project:** Support the Haskell programming language.
* **Brief explanation:** GHC_ targets LLVM. The aim of this project is to allow
  it to deliver PNaCl ``.pexe`` files. One interesting difficulty will be to
  ensure that tail call optimization occurs properly in all targets.
* **Expected results:** The Haskell test suite passes within the browser. How to
  use Haskell to target PNaCl is well documented and easy to do.
* **Knowledge Prerequisite:** Compilers, LLVM.
* **Mentor:** Ben Smith.

.. _GHC:
   http://www.haskell.org/ghc/docs/latest/html/users_guide/code-generators.html

Julia
^^^^^

* **Project:** Support the Julia programming language.
* **Brief explanation:** Julia_ targets LLVM, but it does so through LLVM's
  Just-in-Time compiler which PNaCl doesn't support. The aim of this project is
  to allow it to deliver PNaCl ``.pexe`` files.
* **Expected results:** The Julia test suite passes within the browser. How to
  use Julia to target PNaCl is well documented and easy to do.
* **Knowledge Prerequisite:** Compilers, LLVM.
* **Mentor:** Ben Smith.

.. _Julia: http://julialang.org

Scala
^^^^^

* **Project:** Support the Scala programming language.
* **Brief explanation:** The aim of this project is to allow Scala_ to deliver
  PNaCl ``.pexe`` files.
* **Expected results:** The Scala test suite passes within the browser. How to
  use Scala to target PNaCl is well documented and easy to do.
* **Knowledge Prerequisite:** Compilers.
* **Mentor:** Ben Smith.

.. _Scala: http://www.scala-lang.org

Elm
^^^

* **Project:** Support the Elm programming language.
* **Brief explanation:** The aim of this project is to allow Elm_ to deliver
  PNaCl ``.pexe`` files.
* **Expected results:** The Elm test suite passes within the browser. How to use
  Elm to target PNaCl is well documented and easy to do.
* **Knowledge Prerequisite:** Compilers.
* **Mentor:** Jan Voung.

.. _Elm: http://elm-lang.org

Mono
^^^^

* **Project:** Support C# running inside Mono.
* **Brief explanation:** C# is traditionally a Just-in-Time compiled language,
  the aim of this project is to be able to run C# code within Mono_ while
  compiling ahead-of-time.
* **Expected results:** The Mono test suite passes within the browser. How to
  use Mono to target PNaCl is well documented and easy to do.
* **Knowledge Prerequisite:** Compilers.
* **Mentor:** Derek Schuff.

.. _Mono: http://www.mono-project.com

Perl
^^^^

* **Project:** Support Perl.
* **Brief explanation:** Port the Perl programming language and its packages to
  the PNaCl platform.
* **Expected results:** The Perl test suite passes within the browser. How to
  use Perl to target PNaCl is well documented and easy to do.
* **Knowledge Prerequisite:** C.
* **Mentor:** Brad Nelson.

TCC
---

* **Project:** Port Fabrice Ballard's Tiny C Compiler _TCC to NaCl and PNaCl.
* **Brief explanation:** Port TCC to NaCl and enhance to follow `NaCl sandboxing
  rules`_, as well as emitting `PNaCl bitcode`_. The same could be done with
  `Pico C`_.
* **Expected results:** Compiler ported and code generator working. Can run a
  small benchmark of your choice.
* **Knowledge Prerequisite:** C, assembly, compilers.
* **Mentor:** JF Bastien.

.. _`NaCl sandboxing rules`: https://developer.chrome.com/native-client/reference/sandbox_internals/index
.. _`PNaCl bitcode`: https://developer.chrome.com/native-client/reference/pnacl-bitcode-manual
.. _TCC: http://bellard.org/tcc/
.. _`Pico C`: https://code.google.com/p/picoc


LLVM and PNaCl
--------------

PNaCl relies heavily on LLVM in two key areas:

* On the developer's machine, LLVM is used as a regular toolchain to parse code,
  optimize it, and create a portable executable.
* On user devices, LLVM is installed as part of Chrome to translate a portable
  executable into a machine-specific sandboxed executable.

Most of the contribution ideas around LLVM would occur in the upstream LLVM
repository, and would improve LLVM for more than just PNaCl's sake (though PNaCl
is of course benefiting from these improvements!). Some of these ideas would
also apply to Subzero_, a small and fast translator from portable executable to
machine-specific code.

.. _Subzero: https://chromium.googlesource.com/native_client/pnacl-subzero/+/main/README.rst

Sandboxing Optimizations
^^^^^^^^^^^^^^^^^^^^^^^^

* **Project:** Improved sandboxed code generation.
* **Brief explanation:** PNaCl generates code that targets the NaCl sandbox, but
  this code generation isn't always optimal and sometimes results in a
  performance lost of 10% to 25% compared to unsandboxed code. This project
  would require looking at the x86-32, x86-64, ARM and MIPS code being generated
  by LLVM or Subzero and figuring out how it can be improved to execute
  faster. As an example, one could write a compiler pass to figure out when
  doing a zero-extending ``lea`` on NaCl x86-64 would be useful (increment and
  sandbox), or see if ``%rbp`` can be used more for loads/stores unrelated to
  the call frame.
* **Expected results:** Sandboxed code runs measurably faster, and gets much
  closer to unsandboxed code performance. PNaCl has a fairly extensive
  performance test suite to measure these improvements.
* **Knowledge Prerequisite:** Compilers, assembly.
* **Mentor:** Jan Voung.

Binary Size Reduction
^^^^^^^^^^^^^^^^^^^^^

* **Project:** Reduce the size of binaries generated by LLVM.
* **Brief explanation:** This is generally useful for the LLVM project, but is
  especially important for PNaCl and Emscripten because we deliver code on the
  Web (transfer size and compile time matter!). This stands to drastically
  improve transfer time, and load time. Reduces the size of the PNaCl translator
  as well as user code, makes the generated portable executables smaller and
  translation size faster. Improve LLVM’s ``mergefuncs`` pass to reduce
  redundancy of code. Detect functions and data that aren’t used. Improve
  partial evaluation: can e.g. LLVM’s command-line parsing be mostly removed
  from the PNaCl translator?  Potentially add a pass where a developer manually
  marks functions as unused, and have LLVM replace them with ``abort`` (this
  should propagate and mark other code as dead). This list could be created by
  using code coverage information.
* **Expected results:** Portable executables in the PNaCl repository are
  measurably smaller and translate faster.
* **Knowledge Prerequisite:** LLVM bitcode.
* **Mentor:** JF Bastien.

Vector Support
^^^^^^^^^^^^^^

* **Project:** Improve PNaCl SIMD support.
* **Brief explanation:** PNaCl offers speed on the Web, and generating good SIMD
  code allows developers to use the full capabilities of the device (better user
  experience, longer battery life). The goal of this project is to allow
  developers to use more hardware features in a portable manner by exposing
  portable SIMD primitives and using auto-vectorization. This could also mean
  making the architecture-specific intrinsics “just work” within PNaCl (lower
  them to equivalent architecture-independent intrinsics).
* **Expected results:** Sample code and existing applications run measurably
  faster by using portable SIMD and/or by auto-vectorizing.
* **Knowledge Prerequisite:** Compilers, high-performance code tuning.
* **Mentor:** JF Bastien.

Atomics
^^^^^^^

* **Project:** Improve the performance of C++11 atomics.
* **Brief explanation:** C++11 atomics allow programmers to shed inline assembly
  and use language-level features to express high-performance code. This is
  great for portability, but atomics currently aren't as fast as they could be
  on all platforms. We had an intern work on this in the summer of 2014, see his
  LLVM developer conference presentation `Blowing up the atomic barrier`_. This
  project would be a continuation of this work: improve LLVM's code generation
  for atomics.
* **Expected results:** Code using C++11 atomics runs measurably faster on
  different architectures.
* **Knowledge Prerequisite:** Compilers, memory models.
* **Mentor:** JF Bastien.

.. _`Blowing up the atomic barrier`: http://llvm.org/devmtg/2014-10/#talk10

Security-enhanced PNaCl
^^^^^^^^^^^^^^^^^^^^^^^

* **Project:** Security in-depth for PNaCl.
* **Brief explanation:** PNaCl brings native code to the Web, and we want to
  improve the security of the platform as well as explore novel mitigations.
  This allows PNaCl to take better advantage of the hardware and operating
  system it's running on and makes the platform even faster while keeping users
  safe. It’s also useful for non-browser uses of PNaCl such as running untrusted
  code in the Cloud. A few areas to explore are: code randomization for LLVM and
  Subzero, fuzzing of the translator, code hiding at compilation time, and code
  tuning to the hardware and operating system the untrusted code is running on.
* **Expected results:** The security design and implementation successfully pass
  a review with the Chrome security team.
* **Knowledge Prerequisite:** Security.
* **Mentor:** JF Bastien.

Sanitizer Support
^^^^^^^^^^^^^^^^^

* **Project:** Sanitizer support for untrusted code.
* **Brief explanation:** LLVM supports many sanitizers_ for C/C++ using the
  ``-fsanitize=<name>``. Some of these sanitizers currently work, and some don't
  because they use clever tricks to perform their work, such as using ``mmap``
  to allocate a special shadow memory region with a specific address. This
  project requires adding full support to all of LLVM's sanitizers for untrusted
  user code within PNaCl.
* **Expected results:** The sanitizer tests successfully run as untrusted code
  within PNaCl.
* **Knowledge Prerequisite:** Compilers.
* **Mentor:** JF Bastien.

.. _sanitizers: http://clang.llvm.org/docs/UsersManual.html#controlling-code-generation

NaCl
----

Auto-Sandboxing
^^^^^^^^^^^^^^^

* **Project:** Auto-sandboxing assembler.
* **Brief explanation:** NaCl has a toolchain which can sandbox native
  code. This toolchain can consume C/C++ as well as pre-sandboxed assembly, or
  assembly which uses special sandboxing macros. The goal of this project is to
  follow NaCl's sandboxing requirements automatically which compiling assembly
  files.
* **Expected results:** Existing assembly code can be compiled to a native
  executable that follows NaCl's sandboxing rules.
* **Knowledge Prerequisite:** Assemblers.
* **Mentor:** Derek Schuff, Roland McGrath.

New Sandbox
^^^^^^^^^^^

* **Project:** Create a new software-fault isolation sandbox.
* **Brief explanation:** NaCl pioneered production-quality sandboxes based on
  software-fault isolation, and currently supports x86-32, x86-64, ARMv7's ARM,
  and MIPS. This project involves designing and implementing new sandboxes. Of
  particular interest are ARMv8's aarch64 and Power8. This also requires
  implementing sandboxing in the compiler.
* **Expected results:** The new sandbox's design and implementation successfully
  pass a review with the Chrome security team. Existing NaCl code successfully
  runs in the new sandbox.
* **Knowledge Prerequisite:** Security, low-level assembly, compilers, LLVM.
* **Mentor:** David Sehr.

64-bit Sandbox
^^^^^^^^^^^^^^

* **Project:** Create a 64-bit sandbox.
* **Brief explanation:** NaCl currently supports sandboxes where pointers are
  32-bits. Some applications, both in-browser and not in-browser, would benefit
  from a larger address space. This project involves designing and implementing
  a model for 64-bit sandboxes on all architecture NaCl currently supports. This
  also requires supporting 64-bit pointers in PNaCl using the ``le64`` platform,
  and updating the code generation for each platform.
* **Expected results:** The new sandbox's design and implementation successfully
  pass a review with the Chrome security team. Existing NaCl code successfully
  runs in the new sandbox.
* **Knowledge Prerequisite:** Security, low-level assembly, compilers, LLVM.
* **Mentor:** David Sehr.
