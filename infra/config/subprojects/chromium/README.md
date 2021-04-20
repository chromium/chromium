Definitions of LUCI entities that test the chromium/src codebase.

* ci.star - builders that do post-submit testing against the main branch
* try.star, gpu.trystar, swangle.try.star - builders that do pre-submit testing
* fallback-cq.star - generator that sets up a do-nothing fallback CQ
  group so that CLs can be submitted to the CQ for canary branches or
  other unmanaged branches
