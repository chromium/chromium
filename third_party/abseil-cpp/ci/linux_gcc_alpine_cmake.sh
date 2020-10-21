#!/bin/bash
#
# Copyright 2019 The Abseil Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# TODO(absl-team): This script isn't fully hermetic because
# -DABSL_USE_GOOGLETEST_HEAD=ON means that this script isn't pinned to a fixed
# version of GoogleTest. This means that an upstream change to GoogleTest could
# break this test. Fix this by allowing this script to pin to a known-good
# version of GoogleTest.

set -euox pipefail

if [[ -z ${ABSEIL_ROOT:-} ]]; then
  ABSEIL_ROOT="$(realpath $(dirname ${0})/..)"
fi

if [[ -z ${ABSL_CMAKE_CXX_STANDARDS:-} ]]; then
  ABSL_CMAKE_CXX_STANDARDS="11 14 17"
fi

if [[ -z ${ABSL_CMAKE_BUILD_TYPES:-} ]]; then
  ABSL_CMAKE_BUILD_TYPES="Debug Release"
fi

if [[ -z ${ABSL_CMAKE_BUILD_SHARED:-} ]]; then
  ABSL_CMAKE_BUILD_SHARED="OFF ON"
fi

source "${ABSEIL_ROOT}/ci/linux_docker_containers.sh"
readonly DOCKER_CONTAINER=${LINUX_ALPINE_CONTAINER}

for std in ${ABSL_CMAKE_CXX_STANDARDS}; do
  for compilation_mode in ${ABSL_CMAKE_BUILD_TYPES}; do
    for build_shared in ${ABSL_CMAKE_BUILD_SHARED}; do
      time docker run \
        --mount type=bind,source="${ABSEIL_ROOT}",target=/abseil-cpp,readonly \
        --workdir=/abseil-cpp \
        --tmpfs=/buildfs:exec \
        --cap-add=SYS_PTRACE \
        --rm \
        -e CFLAGS="-Werror" \
        -e CXXFLAGS="-Werror" \
        "${DOCKER_CONTAINER}" \
        /bin/sh -c "
          cd /buildfs && \
          cmake /abseil-cpp \
            -DABSL_USE_GOOGLETEST_HEAD=ON \
            -DABSL_RUN_TESTS=ON \
            -DCMAKE_BUILD_TYPE=${compilation_mode} \
            -DCMAKE_CXX_STANDARD=${std} \
            -DCMAKE_MODULE_LINKER_FLAGS=\"-Wl,--no-undefined\" && \
          make -j$(nproc) && \
          ctest -j$(nproc) --output-on-failure"
    done
  done
done
