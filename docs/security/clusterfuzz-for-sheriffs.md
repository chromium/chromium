# Security Sheriff ClusterFuzz instructions

[TOC]

This page has instructions for [Security Sheriffs](sheriff.md) in how best to use
[ClusterFuzz](https://clusterfuzz.com) to reproduce and label bugs.

## Basics

[https://clusterfuzz.com/upload-testcase](https://clusterfuzz.com/upload-testcase)
allows you to upload files to reproduce crashes on various platforms and will
identify revision ranges when the regression was introduced. If a test case
requires multiple files, they can be uploaded together in a zip or tar
archive: the main file needs to contain the words `index`, `crash` or `test`.

Please *do* specify the crbug number when uploading the test case. This will allow
ClusterFuzz to keep the crbug updated with progress.

## Useful jobs

You should chose the right job type depending on the format of file you want to
test:

* repro.html [linux_asan_chrome_mp](https://clusterfuzz.com/upload-testcase?upload=true&job=linux_asan_chrome_mp)
  or [windows_asan_chrome](https://clusterfuzz.com/upload-testcase?upload=true&job=windows_asan_chrome)
* repro.js [linux_asan_d8](https://clusterfuzz.com/upload-testcase?upload=true&job=linux_asan_d8)
* repro.pdf [libfuzzer_pdfium_asan / pdfium_fuzzer](https://clusterfuzz.com/upload-testcase?upload=true&job=libfuzzer_pdfium_asan&target=pdfium_fuzzer)
  or [libfuzzer_pdfium_asan / pdfium_xfa_fuzzer](https://clusterfuzz.com/upload-testcase?upload=true&job=libfuzzer_pdfium_asan&target=pdfium_xfa_fuzzer)

## MojoJS

[MojoJS](../../mojo/public/js/README.md) is a means for a renderer process to use
Mojo IPCs directly from JavaScript. Although it's not enabled in normal production
Chrome builds, it's a great way to simulate how a compromised renderer can attack
other processes over IPC.

Because Mojo IPCs change with each version of Chrome, the test case needs to
use exactly the right MojoJS bindings. MojoJS bugs typically specify to use
`python ./copy_mojo_bindings.py` to put such bindings in place, but that does not
work for ClusterFuzz where it will need to bisect across many versions of Chrome
with many versions of Mojo.

Therefore, do this instead:

* In the PoC, replace all paths where it's loading MojoJS scripts to be prefixed
  with `file:///gen` instead. For example:
  ```
  <script src="file:///gen/mojo/public/js/mojo_bindings_lite.js">
  ```
  This works because most of the ClusterFuzz Chrome binaries are [now built with](https://chromium-review.googlesource.com/c/chromium/src/+/1119727) `enable_ipc_fuzzer=true`.
* If you believe the bug will reproduce on Linux, use the [linux_asan_chrome_mojo](https://clusterfuzz.com/upload-testcase?upload=true&job=linux_asan_chrome_mojo) job type.
* If you believe the bug will only reproduce on Android, [ClusterFuzz can't help right now](https://crbug.com/1067103).
* Otherwise, use any job type but specify extra command-line flags `--enable-blink-features=MojoJS`. In this case, ClusterFuzz might declare that a browser process crash is Critical severity, whereas because of the precondition of a compromised renderer [you may wish to adjust it down to High](severity-guidelines.md).

[Example bug where these instructions have worked](https://crbug.com/1072983).
