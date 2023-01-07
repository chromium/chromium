# Chromium Design Docs

This directory contains chromium project documentation in
[Gitiles-flavored Markdown](https://gerrit.googlesource.com/gitiles/+/master/Documentation/markdown.md).
It is automatically
[rendered by Gitiles](https://chromium.googlesource.com/chromium/src/+/main/docs/).

Documents here have been imported
from [the Project site](https://www.chromium.org/developers/design-documents).
As of this writing, the vast majority of docs have not been imported yet.

* [Sandboxing](sandbox.md) - The Sandboxing architecture, and Windows
  implementation of sandboxing.
* [Sandboxing FAQ](sandbox_faq.md) - Frequently asked questions about Chromium
  sandboxing.
* [Startup](startup.md) - How browser processes starts up, on different
  platforms.
* [Threading](threading.md) - Preferred ways to use threading, and library
  support for concurrency.
* [GPU Synchronization](gpu_synchronization.md) - Mechanisms for sequencing
  GPU drawing operations across contexts or processes.
