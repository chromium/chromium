# WebRTC Telemetry Tests

[TOC]

## Overview

For its telemetry benchmarks, WebRTC uses the
[test pages](https://webrtc.github.io/test-pages/) and
[sample pages](https://webrtc.github.io/samples/) from the
[WebRTC GitHub project](https://github.com/webrtc).

These are downloaded by the
[`update_webrtc_cases`](../../../../tools/perf/page_sets/update_webrtc_cases)
script into the
[`webrtc_cases`](../../../../tools/perf/page_sets/webrtc_cases/)
directory, and then referenced in
[`webrtc_cases.py`](../../../../tools/perf/page_sets/webrtc_cases.py),
which controls the user interactions and duration of the test.

The [`webrtc.py`](../../../../tools/perf/benchmarks/webrtc.py)
benchmark specifies which metrics should be collected for the
test pages, and extra options that we pass to the test to fake the real camera
and skip assign permission to get access to the video and audio from the user.


## Running the Tests

To run the tests in the browser you should simply open the page, either in
GitHub or the `webrtc_cases` directory.

To collect traces and compute metrics you can run the following command
(assuming you are in `chromium/src`):
```
./tools/perf/run_benchmark webrtc --browser-executable=out/Release/chrome
```

You can filter the pages you want to test using the `--story-tag-filter` flag
with the tags specified in
[`webrtc_cases.py`](../../../../tools/perf/page_sets/webrtc_cases.py#127).

For example, to run only the *multiple-peerconnections* test page, you can use
the following command:
```
./tools/perf/run_benchmark webrtc --browser-executable=out/Release/chrome
--story-tag-filter=stress
```
Or you can run the single story directly:
```
./tools/perf/run_benchmark webrtc --story multiple-peerconnections --browser-executable=out/Release/chrome
```

## Adding Telemetry Tests for WebRTC

To add a new test page you should:

1. **Add a new test page to the
[test-pages](https://github.com/webrtc/test-pages) repository under the
[`src` directory](https://github.com/webrtc/test-pages/tree/gh-pages/src) in the
gh-pages branch.**

 The test page should be named `index.html`, and the logic for the test should
 be inside the `js` folder in a file named `main.js`. Don’t forget to reference
 it at the
 [index page](https://github.com/webrtc/test-pages/blob/gh-pages/index.html).

 See the *[multiple-peerconnections](https://github.com/webrtc/test-pages/tree/gh-pages/src/multiple-peerconnections)*
 test page for an example.

2. **Edit the [`update_webrtc_cases_script`](../../../../tools/perf/page_sets/update_webrtc_cases#21)
to reference the page you added, and run it.**

 This will download the `index.html` file and the `main.js` file into the
 `webrtc_cases` directory, and rename them with the name of the test.

 For example, the *multiple-peerconnections* test page will be downloaded as
 `multiple-peerconnections.html` and `multiple-peerconnections.js`.

3. **Add a new class to `webrtc_cases.py` to load the page and control the user
interactions.**

 See the
 [`MultiplePeerConnections`](../../../../tools/perf/page_sets/webrtc_cases.py#101)
 class for example.

4. **Add the story to the `WebrtcPageSet` class.**

 See [here](../../../../tools/perf/page_sets/webrtc_cases.py#127) for example.

5. **Submit your changes as a CL**
