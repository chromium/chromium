# //components/webcrypto plan

This document outlines the plan for the code living in this directory. The
current code is more or less in maintenance mode.

## Code Changes

* Investigate whether CryptoThreadPool is necessary, whether it is necessary
  for all operations, and whether it can be use some existing more generic
  thread pool rather than requiring a dedicated worker thread
  (https://crbug.com/623700)
* Fix the semantic mismatches in JWK importing with usage masks
  (https://crbug.com/1136147)
* Make AES key scheduling more efficient by not recomputing keys all the
  time (https://crbug.com/1049916)
* Move the entire component into blink and get rid of a bunch of abstraction
  layers (https://crbug.com/614385)
* Remove the unnecessary "threadsafety caches" in WebCrypto keys
  (https://crbug.com/1180244)

## Behavior / Spec Changes (will require spec work)
* Either finish the X25519 / ed25519 implementation (which will probably require
  standardizing it) or remove it (https://crbug.com/1032821)
* Make most of the operations synchronous instead of asynchronous - they are
  fundamentally fast and do not need to be farmed out to worker threads, except
  for *maybe* key generation.
