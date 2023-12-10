# Web Assembly Security in Chromium

TL;DR
From Chrome's threat model perspective we generally consider WASM and JavaScript
to be in the same risk category - potentially actively malicious. We will run
arbitrary WASM from the web without any indication of trust and rely on V8's
security model as well as our renderer sandbox to contain potentially malicious
behavior. Any outputs from such WASM and JavaScript need to be regarded as
untrusted by the rest of the browser code.

## Background

As the web continues to evolve, the greater ecosystem brings with it new and
exciting capabilities. From ajax to shadow DOM, Blink, Chrome, and others
continue to push the web forward.

One such capability is to run native code. The latest standard to do so on the
web is via [web assembly](https://webassembly.org/).

When it comes to security, web assembly prompts a series of concerns centered
around our understanding of the insecurity of native code. Some of these
specific issues are addressed below. Native code suffers from a
variety of memory vulnerabilities which can result in remote code execution
(RCE). By some measures, 60-70% of security bugs fall into this category.

Web assembly baked in memory safety measures to prevent RCE. In particular,
wasm’s memory model is to give the page a single, continuous block of
memory. This memory is isolated from the program’s binary instructions. [Learn
more](https://webassembly.org/docs/security/).

## CPU bugs, side channel attacks

WASM is not materially different from JavaScript in this regard and both may be
vulnerable.

## Malicious Input

WASM is not materially different from JavaScript in this regard and both may be
vulnerable.
