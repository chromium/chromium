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
    name = "com_google_googletest",
    # Keep this URL in sync with ABSL_GOOGLETEST_COMMIT in ci/cmake_common.sh.
    urls = ["https://github.com/google/googletest/archive/f5e592d8ee5ffb1d9af5be7f715ce3576b8bf9c4.zip"],  # 2021-04-29T14:40:44Z
    strip_prefix = "googletest-f5e592d8ee5ffb1d9af5be7f715ce3576b8bf9c4",
    sha256 = "e61e3889bd5cc3e6bc1084d2108ecda2f110c0387ba88b394ffd16043a1d5709",
)

# Google benchmark.
http_archive(
    name = "com_github_google_benchmark",
    urls = ["https://github.com/google/benchmark/archive/7d0d9061d83b663ce05d9de5da3d5865a3845b79.zip"],  # 2021-05-11T11:56:00Z
    strip_prefix = "benchmark-7d0d9061d83b663ce05d9de5da3d5865a3845b79",
    sha256 = "a07789754963e3ea3a1e13fed3a4d48fac0c5f7f749c5065f6c30cd2c1529661",
)

# C++ rules for Bazel.
http_archive(
    name = "rules_cc",
    urls = ["https://github.com/bazelbuild/rules_cc/archive/ab5395627c80e025e824bd005d41f96b20618b9d.zip"],  # 2021-05-10T15:35:04Z
    strip_prefix = "rules_cc-ab5395627c80e025e824bd005d41f96b20618b9d",
    sha256 = "f3908cb40a6577ab0d1ef9e00052739bf69b4313fa7d20b4d61d4521ea67abf3",
)
