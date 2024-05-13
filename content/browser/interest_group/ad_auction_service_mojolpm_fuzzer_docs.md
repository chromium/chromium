# Overview

[Fuzzing] is a technique that feeds randomly-generated inputs into production
code to search for crash bugs. [ad_auction_service_mojolpm_fuzzer.cc] implements
a fuzzer that invokes the API surface of the [`AdAuctionService`] Mojo API. Most
of the Protected Audience API implementation exists behind `AdAuctionService`
and related interfaces, with a thin portion of renderer-process code that
exposes the JavaScript APIs.

The ad_auction_service_mojolpm_fuzzer.cc fuzzer only fuzzes the Mojo APIs -- it
doesn't exercise any renderer-side code. However, it does run worklet code
(in-process), and reports crashes in worklet code.

(However, the fuzzer doesn't catch "bad message" worklet crashes where the
browser process detects that the worklet process sent an invalid Mojo message --
although it should be possible to write a fuzzer that invokes the worklet Mojo
interfaces directly and validates the results).

The fuzzer uses the [MojoLPM] framework, which extends the capabilities of
[libprotobuf-mutator] (LPM) to allow fuzzing Mojo interfaces. It maps [protocol
buffers] to Mojo struct values, which allows invoking Mojo interface methods
using values produced by LPM.

MojoLPM fuzzer testcases contain a series of "actions". These actions can
[invoke methods on Mojo interfaces], but they can also perform any arbitrary
actions that would normally be performed in a
[`RenderViewHostTestHarnessAdapter`]-based unit test such as [navigate main or
subframes], or [alter the response] of network responses using
[`URLLoaderInterceptor`]. So, most of what can be done in
[`AdAuctionServiceImplTest`] could be done in a fuzzer.

For every testcase, the [`SetUp()`] method gets called, then [`RunAction()`]
method is called for each action in the test case, and then finally,
[`TearDown()`] is called. This process is repeated for every test case in the
seed corpus, after which the fuzzer starts generating and executing new random
test cases, using the seed corpus as a guide.

See the [MojoLPM docs] for more info.

# Running the fuzzer

Run with the following build args:

```
# Without this flag, our fuzzer target won't exist.
enable_mojom_fuzzer = true
is_component_build = true
is_debug = false
optimize_for_fuzzing = true
use_libfuzzer = true
```

You can use `dcheck_always_on = true`, `use_remoteexec = true`, `use_siso =
true`, and `is_asan = true`. Note that as of writing, `CHECK()`s don't symbolize
if ASAN is turned on, even with tools/valgrind/asan/asan_symbolize.py
(https://crbug.com/40948553), and you'll need
`ASAN_OPTIONS=detect_odr_violation=0,handle_abort=1,handle_sigtrap=1,handle_sigill=1`
to go before `testing/xvfb.py` in the invocation below.

Replace `mojofuzz` with your out/ directory, and run:

`autoninja -C out/mojofuzz ad_auction_service_mojolpm_fuzzer && (rm -rf
/dev/shm/corpus;mkdir /dev/shm/corpus;unzip
out/mojofuzz/ad_auction_service_mojolpm_fuzzer_seed_corpus.zip -d
/dev/shm/corpus; testing/xvfb.py out/mojofuzz/ad_auction_service_mojolpm_fuzzer
-rss_limit_mb=2000000  /dev/shm/corpus`

`testing/xvfb.py` is needed when running on Linux over ssh to provide a fake X11
environment. You can omit it for other platforms.

You can replace `/dev/shm/corpus/` with any directory -- `/dev/shm` was chosen
for performance on Linux since it resides in RAM.

Note that the commands that populate `/dev/shm/corpus` are copying from the seed
corpus zip file from the out directory.

# Building a coverage report

The coverage run shows the coverage yielded by the testcases in your current
corpus directory (`/dev/shm/corpus` in this example). Coverage runs do not
attempt to create new testcases via fuzzing, unlike when running the fuzzer.

You can run coverage after running the fuzzer using the same corpus directory to
see the lines covered by the set of testcases discovered by the fuzzer. You can
also directly run against the unzipped seed corpus -- and the seed corpus zip
file also is produced by a coverage build.

Run with the following build args:

```
# Without this flag, our fuzzer target won't exist.
enable_mojom_fuzzer = true
is_component_build = false
is_debug = false
optimize_for_fuzzing = false
use_libfuzzer = true
use_clang_coverage = true
```

You can use `dcheck_always_on = true`, `use_remoteexec = true`, and
`use_siso = true`.

**Make sure the corpus directory `/dev/shm/corpus` is populated**, as described
above, then replace `mojofuzz-coverage` with your out/ directory, and run:

`vpython3 tools/code_coverage/coverage.py ad_auction_service_mojolpm_fuzzer -b
out/mojofuzz-coverage -o ManualReport -c "testing/xvfb.py
out/mojofuzz-coverage/ad_auction_service_mojolpm_fuzzer -ignore_timeouts=1
-timeout=4 -runs=0 /dev/shm/corpus" -f content`

`testing/xvfb.py` is needed when running on Linux over ssh to provide a fake X11
environment. You can omit it for other platforms.

You can replace `/dev/shm/corpus/` with any directory -- `/dev/shm` was chosen
for performance on Linux since it resides in RAM.

The resultant ManualReport/ directory will contain an HTML coverage report that
you can view in a browser.

[Fuzzing]: https://chromium.googlesource.com/chromium/src/+/main/testing/libfuzzer/README.md
[ad_auction_service_mojolpm_fuzzer.cc]: https://source.chromium.org/chromium/chromium/src/+/main:content/browser/interest_group/ad_auction_service_mojolpm_fuzzer.cc
[`AdAuctionService`]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/mojom/interest_group/ad_auction_service.mojom
[MojoLPM]: https://chromium.googlesource.com/chromium/src/+/main/mojo/docs/mojolpm.md
[libprotobuf-mutator]: https://chromium.googlesource.com/chromium/src/+/main/testing/libfuzzer/libprotobuf-mutator.md
[protocol buffers]: https://protobuf.dev/
[invoke methods on Mojo interfaces]: https://source.chromium.org/chromium/chromium/src/+/main:content/browser/interest_group/ad_auction_service_mojolpm_fuzzer.proto;l=49;drc=fc326be1f824cf5d3e20eb94b9e800457bf96d61
[`RenderViewHostTestHarnessAdapter`]: https://source.chromium.org/chromium/chromium/src/+/main:content/public/test/test_renderer_host.h?q=class:%5CbRenderViewHostTestHarness%5Cb
[navigate main or subframes]: https://source.chromium.org/chromium/chromium/src/+/main:content/browser/interest_group/ad_auction_service_mojolpm_fuzzer.cc;l=380;drc=fc326be1f824cf5d3e20eb94b9e800457bf96d61
[alter the response]: https://source.chromium.org/chromium/chromium/src/+/main:content/browser/interest_group/ad_auction_service_mojolpm_fuzzer.proto;l=48;drc=fc326be1f824cf5d3e20eb94b9e800457bf96d61
[`URLLoaderInterceptor`]: https://source.chromium.org/search?q=class:%5CbURLLoaderInterceptor%5Cb
[`AdAuctionServiceImplTest`]: https://source.chromium.org/search?q=class:%5CbAdAuctionServiceImplTest%5Cb
[`SetUp()`]: https://source.chromium.org/chromium/chromium/src/+/main:content/browser/interest_group/ad_auction_service_mojolpm_fuzzer.cc;l=368;drc=fc326be1f824cf5d3e20eb94b9e800457bf96d61
[`RunAction()`]: https://source.chromium.org/chromium/chromium/src/+/main:content/browser/interest_group/ad_auction_service_mojolpm_fuzzer.cc;l=311;drc=fc326be1f824cf5d3e20eb94b9e800457bf96d61
[`TearDown()`]: https://source.chromium.org/chromium/chromium/src/+/main:content/browser/interest_group/ad_auction_service_mojolpm_fuzzer.cc;l=398;drc=fc326be1f824cf5d3e20eb94b9e800457bf96d61
[MojoLPM docs]: https://chromium.googlesource.com/chromium/src/+/main/mojo/docs/mojolpm.md