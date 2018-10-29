#
# Copyright 2017 The Abseil Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# We require 3.0 for modern, target-based CMake.  We require 3.1 for the use of
# CXX_STANDARD in our targets.
cmake_minimum_required(VERSION 3.1)
project(absl)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake)

include(GNUInstallDirs)
include(AbseilHelpers)


# config options
if (MSVC)
  # /wd4005  macro-redefinition
  # /wd4068  unknown pragma
  # /wd4244  conversion from 'type1' to 'type2'
  # /wd4267  conversion from 'size_t' to 'type2'
  # /wd4800  force value to bool 'true' or 'false' (performance warning)
  add_compile_options(/W3 /wd4005 /wd4068 /wd4244 /wd4267 /wd4800)
  add_definitions(/DNOMINMAX /DWIN32_LEAN_AND_MEAN=1 /D_CRT_SECURE_NO_WARNINGS /D_SCL_SECURE_NO_WARNINGS)
else()
  set(ABSL_STD_CXX_FLAG "-std=c++11" CACHE STRING "c++ std flag (default: c++11)")
endif()



##
## Using absl targets
##
## all public absl targets are
## exported with the absl:: prefix
##
## e.g absl::base absl::synchronization absl::strings ....
##
## DO NOT rely on the internal targets outside of the prefix


# include current path
list(APPEND ABSL_COMMON_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

# -std=X
set(CMAKE_CXX_FLAGS "${ABSL_STD_CXX_FLAG} ${CMAKE_CXX_FLAGS}")

# -fexceptions
set(ABSL_EXCEPTIONS_FLAG "${CMAKE_CXX_EXCEPTIONS}")

# find dependencies
## pthread
find_package(Threads REQUIRED)

option(ABSL_USE_GOOGLETEST_HEAD
  "If ON, abseil will download HEAD from googletest at config time." OFF)

option(ABSL_RUN_TESTS "If ON, Abseil tests will be run." OFF)

if(${ABSL_RUN_TESTS})
  # enable CTest.  This will set BUILD_TESTING to ON unless otherwise specified
  # on the command line
  include(CTest)
  enable_testing()
endif()

## check targets
if(BUILD_TESTING)

  if(${ABSL_USE_GOOGLETEST_HEAD})
    include(CMake/DownloadGTest.cmake)
  endif()

  check_target(gtest)
  check_target(gtest_main)
  check_target(gmock)

  list(APPEND ABSL_TEST_COMMON_LIBRARIES
    gtest_main
    gtest
    gmock
    ${CMAKE_THREAD_LIBS_INIT}
  )
endif()

add_subdirectory(absl)
