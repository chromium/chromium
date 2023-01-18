#
# Copyright 2019 The Abseil Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

workspace(name = "com_google_absl")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# GoogleTest/GoogleMock framework. Used by most unit-tests.
http_archive(
    name = "com_google_googletest",  # 2023-01-05T19:15:29Z
    sha256 = "1c805208d019aabb8be3cddbc6098be8815ee5cf0a7baf526102528fd624c422",
    strip_prefix = "googletest-934542165899c786cb5d8a710529c37184730183",
    # Keep this URL in sync with ABSL_GOOGLETEST_COMMIT in ci/cmake_common.sh.
    urls = ["https://github.com/google/googletest/archive/934542165899c786cb5d8a710529c37184730183.zip"],
)

# RE2 (the regular expression library used by GoogleTest)
# Note this must use a commit from the `abseil` branch of the RE2 project.
# https://github.com/google/re2/tree/abseil
http_archive(
    name = "com_googlesource_code_re2",
    sha256 = "0a890c2aa0bb05b2ce906a15efb520d0f5ad4c7d37b8db959c43772802991887",
    strip_prefix = "re2-a427f10b9fb4622dd6d8643032600aa1b50fbd12",
    urls = ["https://github.com/google/re2/archive/a427f10b9fb4622dd6d8643032600aa1b50fbd12.zip"],  # 2022-06-09
)

# Google benchmark.
http_archive(
    name = "com_github_google_benchmark",  # 2023-01-10T16:48:17Z
    sha256 = "ede6830512f21490eeea1f238f083702eb178890820c14451c1c3d69fd375b19",
    strip_prefix = "benchmark-a3235d7b69c84e8c9ff8722a22b8ac5e1bc716a6",
    urls = ["https://github.com/google/benchmark/archive/a3235d7b69c84e8c9ff8722a22b8ac5e1bc716a6.zip"],
)

# Bazel Skylib.
http_archive(
    name = "bazel_skylib",  # 2022-11-16T18:29:32Z
    sha256 = "a22290c26d29d3ecca286466f7f295ac6cbe32c0a9da3a91176a90e0725e3649",
    strip_prefix = "bazel-skylib-5bfcb1a684550626ce138fe0fe8f5f702b3764c3",
    urls = ["https://github.com/bazelbuild/bazel-skylib/archive/5bfcb1a684550626ce138fe0fe8f5f702b3764c3.zip"],
)

# Bazel platform rules.
http_archive(
    name = "platforms",  # 2022-11-09T19:18:22Z
    sha256 = "b4a3b45dc4202e2b3e34e3bc49d2b5b37295fc23ea58d88fb9e01f3642ad9b55",
    strip_prefix = "platforms-3fbc687756043fb58a407c2ea8c944bc2fe1d922",
    urls = ["https://github.com/bazelbuild/platforms/archive/3fbc687756043fb58a407c2ea8c944bc2fe1d922.zip"],
)
