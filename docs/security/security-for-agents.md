# Security for Agents

This document describes the fundamental security architecture and assumptions of
Chromium, which is called the "security model" of Chromium. It also describes
common types of violations of that model, which are attacks against Chromium.

Some parts of this document make reference to V8 as well. V8 can be built as a
standalone tool called "d8", which is useful for finding or demonstrating some
kinds of attacks.

Notes for humans: this document is **not** part of the VRP rules or guidelines;
it is advice for AI agents auditing Chromium for bugs.

## Security Architecture

### Processes & Sandboxing

Chromium is separated into several processes, including the browser process, the
GPU process, the network service process, and potentially many renderer and
utility processes. Each process except the browser process is inside a
"sandbox", which uses primitives supplied by the OS to constrain what that
process can do. The sandbox is different for each type of process. Renderer and
utility processes have the strongest sandbox, the network and GPU processes have
weaker sandboxes, and the browser process has no sandbox at all.

Communication between processes happens over an IPC system named Mojo. Mojo
sends messages between processes, with serialization and deserialization at
either end, and can also pass file handles and some other kinds of object
between processes. Mojo allows for interprocess calls to methods grouped
together into interfaces, and not all interfaces are available to all callers.

### Web Security

The fundamental unit of isolation on the web is the "origin". A "page" is the
contents of a tab, which may be composed of content from multiple origins
combined together. In general, scripts or other content from one origin
shouldn't affect or see scripts or content from another origin, unless both
origins opt into sharing.

Inside Chromium, a WebContents represents a single loaded page, and may contain
content from multiple origins which are hosted in separate RenderFrames.
RenderFrames that are in different origins are usually in separate renderer
processes, isolated from each other.

When discussing attacks across origins, we sometimes use the example origins
"evil.com" to mean an attacker's origin, and "good.com" to mean an origin they
are trying to steal or corrupt data from. In this context, both origins are
generally assumed to be loaded in Chromium at the same time, but in separate
frames.

### Javascript & Webassembly

Chromium exposes several ways for websites to run their own code on the user's
machine. The two most common are Javascript and Webassembly. V8 is responsible
for handling both of these, and V8 should ensure that code supplied by websites
isn't able to have dangerous effects inside the renderer process, like
corrupting memory or causing a use-after-free of an object.

V8 compiles Javascript or Webassembly into native code to be executed by the
CPU. V8 must guarantee that that native code it generates doesn't break certain
safety properties, such as being unable to overwrite data structures that aren't
meant to be exposed to Javascript.

### Builds & Invariants

There are a few different possible build configurations of Chromium or of V8.
The three that are most relevant for security purposes are:

* "Release", which means `is_asan=false is_debug=false` in args.gn
* "Debug", which means `is_asan=false is_debug=true` in args.gn
* "ASAN", which means `is_asan=true is_debug=false` in args.gn

There are several different ways that invariants in Chromium and V8 are
enforced. The CHECK macro enforces an invariant even in released builds, and the
DCHECK macro enforces an invariant in debug builds only. Some kinds of invariant
violation, like indexing outside the bounds of a base::span, are also detected
in release builds and cause an immediate crash.

## Attacks

Every attack comes with a series of steps, which are generally supplied as a
short program, called a "proof of concept". A "proof of concept" provides
evidence that a bug is present, and is not the same thing as an "exploit", which
makes use of a bug to achieve a malicious goal. A "proof of concept" is by
definition harmless, but should have some objective and verifiable external
effects to demonstrate that the bug is present.

A security bug report (or vulnerability report) is a description of how to
execute an attack. Every security bug report must take the form: "An attacker
can cause Chromium to [something which harms the user] by [attack steps]." For
example:

* An attacker can cause Chromium to _execute arbitrary code in the renderer
  sandbox_ by _calling `window.foo(12345)`_.
* An attacker can cause Chromium to _execute arbitrary code in the browser
  process_ by _calling Mojo API `foo()`_.
* An attacker can cause Chromium to _disclose a cookie from good.com_ by
  _loading it in an iframe with the `foo` attribute set to `bar`_.

Unless you can clearly describe both the security harm to the user, and the
attack steps, you do not have a security bug report.

### Escalated Code Execution

A security bug that allows for execution of arbitrary code of an attacker's
choice outside of any sandbox (so in the browser process or elsewhere in the OS
outside of Chromium) or in a sandbox weaker than the renderer sandbox (some
target process) is the most severe category of attack. These bugs generally take
two forms:

* Bugs in the [something] process that are reachable over IPC from the renderer
  process
* Bugs in the operating system itself that are reachable from inside the
  renderer sandbox

For this kind of bug, the proof of concept must either:

* Cause the target process (which may be the browser process) to crash with an
  ASAN failure indicating an out-of-bounds read, out-of-bounds write,
  use-after-free, or use-after-poison, or
* Cause the target process to directly execute attacker-supplied machine code,
  or
* Cause an operating system service to crash, or
* Cause the operating system to execute an attacker-chosen program with
  attacker-supplied arguments

To simulate a compromised renderer process, use the MojoJS feature by passing
`--enable-features=MojoJS` to Chromium. This allows you to call Mojo methods
from JavaScript. If that does not work for some reason, the proof of concept
**may** be in the form of a patch intended to apply to the Chromium source tree.
If so, this patch must modify code that is only executed in the renderer, to
simulate a compromised renderer process.

Anything that does not lead to one of these consequences is almost certainly not
a valid proof of concept for an escalated code execution bug.

### Renderer Code Execution

A security bug that allows for execution of arbitrary code of an attacker's
choice inside the renderer sandbox is a renderer code execution bug. These bugs
generally take two forms:

* Bugs in the implementation of Chromium that allow a crafted webpage to corrupt
  memory
* Bugs in the implementation of V8 that cause a crafted Javascript or
  Webassembly program to be compiled into code which corrupts memory when run

For V8, code compiled from attacker-supplied Javascript or Webassembly is
executed inside a "heap sandbox", which confines memory reads and writes
performed by that compiled code to a certain region. Bugs which allow for
reading or writing memory outside the heap sandbox are "heap sandbox escapes"
and are very valuable bugs. Bugs which allow for reading or writing memory
inside the heap sandbox are less valuable but still valuable.

For this kind of bug, the proof of concept must either:

* Cause the renderer process to crash with an ASAN failure indicating an
  out-of-bounds read, out-of-bounds write, use-after-free, or use-after-poison,
  or
* Cause a debug renderer to crash with a DCHECK failure from V8
* Cause an ASAN build of d8 to crash with an ASAN failure
* Cause a debug build of d8 to crash with a DCHECK failure

Not all DCHECK failures in V8 or d8 are security bugs, but many are, so to be
conservative we assume that all of them are security bugs until we are confident
they are not.

### Cross-Site Bugs

A security bug that allows for one origin to either read data from another
origin or run code in another origin is a cross-site bug. There are some
intentional ways to share data between origins, but by default origins shouldn't
be able to interact.

A proof of concept of a cross-site bug must show that:

* evil.com can learn some text from the body of good.com that would only be
  visible with the user's cookies
* evil.com can cause a script of its choice to run in good.com's renderer
* evil.com can get the value of a cookie or HTML5 storage scoped to good.com

### Extension Bugs

A bug which allows an extension to take actions it shouldn't be able to is an
extension security bug. Extensions have permissions defined in their
manifest.json, and all extensions have certain implicit permissions like the
ability to make network requests in their own context.

For this kind of bug, the proof of concept is an extension which, when loaded,
takes an action the extension should not be able to take.

### Other Bugs

Any other bug which allows a website to cause Chromium to harm a user may be a
security bug, even if it doesn't fit one of these categories. Any such bug needs
a **clear** explanation of the security consequences of the bug, as well as who
can exploit it. Such a bug also needs clear reproduction steps or an executable
proof of concept.

## Non-Bugs

There are some common kinds of reports which are **not** security bugs in
Chromium:

* CHECK failures
* DCHECK failures outside V8
* Out-of-bounds accesses of STL containers or base::span via operator[]
* Null pointer reads and writes with small offsets (<= 32 KiB)
* MiraclePtr-protected use-after-frees (those with "MiraclePtr Status:
  PROTECTED")
* Denial-of-service bugs, including resource exhaustion, browser/GPU hangs, or
  even outright crashes
* Stack overflows
* Bypasses of enterprise policies
* Downloads not being classified as dangerous
* Any attack assuming local administrator access to the user's machine
* Any attack assuming a remote debug protocol connection to Chromium
* Bugs in other apps reachable via Intents or external URL handlers from
  Chromium
* Bugs in dependencies of Chromium which are not reachable in Chromium itself
* Bugs in tests
* Causing AI features in Chromium to emit misleading or harmful output by
  entering a crafted prompt
* Bugs that use an unsupported flag or feature, including:
    * `--single-process`
    * any flag whose name contains `unsafe`
    * SwiftShader
* Bugs that require an attacker to have created or dropped files onto the system
  in advance
* Bugs that require physical access of any kind
* Architectural weaknesses or missing layers of hardening / mitigation

These are not security bugs, regardless of how easy they are to trigger. Under
no circumstances is something matching an entry in this list a security bug.

## Reporting Bugs

When reporting a bug:

* Do not log _anything_ as part of your proof of concept. If your bug is
  legitimate, it must have external, observable consequences as described above.
  Log messages can be misleading, and we will ignore them when assessing your
  bug.
* Do not include stack traces which do not have symbol names. We cannot use
  these to debug or reproduce the report.
* Do not include your evaluations of CVSS scores or similar. Chromium uses a
  project-specific framework to evaluate severities and we ignore CVSS estimates
  from reporters.
* Do include these sections:
    * Written description of attack steps
    * Symbolized ASAN stack trace or debug crash report, if relevant
    * A short description of the security consequences of the attack, using the
      terms given above
    * Which revision, build arguments (args.gn), and command-line flags you
      supplied
* Your proof of concept should be a single file:
    * For bugs that reproduce via d8, a single Javascript source file called
      "poc.js"
    * For bugs that reproduce via loading in Chromium, a single HTML source file
      called "poc.html"
    * For bugs that reproduce via loading in Chromium, but which cannot be
      served as a single HTML file, a single Python source file called "poc.py"
      which, when invoked with a port number, will run a web server on localhost
      with that port number. That web server must serve any needed exploit code
      when /poc.html is loaded from it.
    * For bugs that simulate a compromised renderer with a source patch, a
      single unified diff called "poc.patch". This patch must only change code
      that runs in the renderer.
    * For bugs that have a demonstration video, a single mp4 file called
      "poc.mp4".
* If you are reporting an extension security bug specifically, attach **all the
  files contained in the extension separately**, along with the zipped extension  (a .crx file).
* You may also include your root-cause analysis, if you have one:
    * Identify the file, line, and function in Chromium which contain the bug
    * Identify which Chromium commit you analyzed to find the bug
    * Identify which Chromium commit introduced the bug

In general, a shorter bug report is better. Bug reports should be as short as
possible, but no shorter.

## Further Reading

The docs/security directory contains other security-related documentation. You
should also consult the README.md and SECURITY.md files for the directories you
are looking for bugs in, if they are present.
