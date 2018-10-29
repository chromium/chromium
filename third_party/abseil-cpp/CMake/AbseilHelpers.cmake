#
# Copyright 2017 The Abseil Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

include(CMakeParseArguments)

# The IDE folder for Abseil that will be used if Abseil is included in a CMake
# project that sets
#    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# For example, Visual Studio supports folders.
set(ABSL_IDE_FOLDER Abseil)

#
# create a library in the absl namespace
#
# parameters
# SOURCES : sources files for the library
# PUBLIC_LIBRARIES: targets and flags for linking phase
# PRIVATE_COMPILE_FLAGS: compile flags for the library. Will not be exported.
# EXPORT_NAME: export name for the absl:: target export
# TARGET: target name
#
# create a target associated to <NAME>
# libraries are installed under CMAKE_INSTALL_FULL_LIBDIR by default
#
function(absl_library)
  cmake_parse_arguments(ABSL_LIB
    "DISABLE_INSTALL" # keep that in case we want to support installation one day
    "TARGET;EXPORT_NAME"
    "SOURCES;PUBLIC_LIBRARIES;PRIVATE_COMPILE_FLAGS"
    ${ARGN}
  )

  set(_NAME ${ABSL_LIB_TARGET})
  string(TOUPPER ${_NAME} _UPPER_NAME)

  add_library(${_NAME} STATIC ${ABSL_LIB_SOURCES})

  target_compile_options(${_NAME} PRIVATE ${ABSL_COMPILE_CXXFLAGS} ${ABSL_LIB_PRIVATE_COMPILE_FLAGS})
  target_link_libraries(${_NAME} PUBLIC ${ABSL_LIB_PUBLIC_LIBRARIES})
  target_include_directories(${_NAME}
    PUBLIC ${ABSL_COMMON_INCLUDE_DIRS} ${ABSL_LIB_PUBLIC_INCLUDE_DIRS}
    PRIVATE ${ABSL_LIB_PRIVATE_INCLUDE_DIRS}
  )
  # Add all Abseil targets to a a folder in the IDE for organization.
  set_property(TARGET ${_NAME} PROPERTY FOLDER ${ABSL_IDE_FOLDER})

  if(ABSL_LIB_EXPORT_NAME)
    add_library(absl::${ABSL_LIB_EXPORT_NAME} ALIAS ${_NAME})
  endif()
endfunction()



#
# header only virtual target creation
#
function(absl_header_library)
  cmake_parse_arguments(ABSL_HO_LIB
    "DISABLE_INSTALL"
    "EXPORT_NAME;TARGET"
    "PUBLIC_LIBRARIES;PRIVATE_COMPILE_FLAGS;PUBLIC_INCLUDE_DIRS;PRIVATE_INCLUDE_DIRS"
    ${ARGN}
  )

  set(_NAME ${ABSL_HO_LIB_TARGET})

  set(__dummy_header_only_lib_file "${CMAKE_CURRENT_BINARY_DIR}/${_NAME}_header_only_dummy.cc")

  if(NOT EXISTS ${__dummy_header_only_lib_file})
    file(WRITE ${__dummy_header_only_lib_file}
      "/* generated file for header-only cmake target */

      namespace absl {

       // single meaningless symbol
       void ${_NAME}__header_fakesym() {}
      }  // namespace absl
      "
    )
  endif()


  add_library(${_NAME} ${__dummy_header_only_lib_file})
  target_link_libraries(${_NAME} PUBLIC ${ABSL_HO_LIB_PUBLIC_LIBRARIES})
  target_include_directories(${_NAME}
    PUBLIC ${ABSL_COMMON_INCLUDE_DIRS} ${ABSL_HO_LIB_PUBLIC_INCLUDE_DIRS}
    PRIVATE ${ABSL_HO_LIB_PRIVATE_INCLUDE_DIRS}
  )

  # Add all Abseil targets to a a folder in the IDE for organization.
  set_property(TARGET ${_NAME} PROPERTY FOLDER ${ABSL_IDE_FOLDER})

  if(ABSL_HO_LIB_EXPORT_NAME)
    add_library(absl::${ABSL_HO_LIB_EXPORT_NAME} ALIAS ${_NAME})
  endif()

endfunction()


#
# create an abseil unit_test and add it to the executed test list
#
# parameters
# TARGET: target name prefix
# SOURCES: sources files for the tests
# PUBLIC_LIBRARIES: targets and flags for linking phase.
# PRIVATE_COMPILE_FLAGS: compile flags for the test. Will not be exported.
#
# create a target associated to <NAME>_bin
#
# all tests will be register for execution with add_test()
#
# test compilation and execution is disable when ABSL_RUN_TESTS=OFF
#
function(absl_test)

  cmake_parse_arguments(ABSL_TEST
    ""
    "TARGET"
    "SOURCES;PUBLIC_LIBRARIES;PRIVATE_COMPILE_FLAGS;PUBLIC_INCLUDE_DIRS"
    ${ARGN}
  )


  if(ABSL_RUN_TESTS)

    set(_NAME ${ABSL_TEST_TARGET})
    string(TOUPPER ${_NAME} _UPPER_NAME)

    add_executable(${_NAME}_bin ${ABSL_TEST_SOURCES})

    target_compile_options(${_NAME}_bin PRIVATE ${ABSL_COMPILE_CXXFLAGS} ${ABSL_TEST_PRIVATE_COMPILE_FLAGS})
    target_link_libraries(${_NAME}_bin PUBLIC ${ABSL_TEST_PUBLIC_LIBRARIES} ${ABSL_TEST_COMMON_LIBRARIES})
    target_include_directories(${_NAME}_bin
      PUBLIC ${ABSL_COMMON_INCLUDE_DIRS} ${ABSL_TEST_PUBLIC_INCLUDE_DIRS}
      PRIVATE ${GMOCK_INCLUDE_DIRS} ${GTEST_INCLUDE_DIRS}
    )

    # Add all Abseil targets to a a folder in the IDE for organization.
    set_property(TARGET ${_NAME}_bin PROPERTY FOLDER ${ABSL_IDE_FOLDER})

    add_test(${_NAME} ${_NAME}_bin)
  endif(ABSL_RUN_TESTS)

endfunction()




function(check_target my_target)

  if(NOT TARGET ${my_target})
    message(FATAL_ERROR " ABSL: compiling absl requires a ${my_target} CMake target in your project,
                   see CMake/README.md for more details")
  endif(NOT TARGET ${my_target})

endfunction()
