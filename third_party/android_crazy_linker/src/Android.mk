# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_PATH := $(call my-dir)

crazy_linker_sources := \
  src/crazy_linker_api.cpp \
  src/crazy_linker_ashmem.cpp \
  src/crazy_linker_debug.cpp \
  src/crazy_linker_elf_loader.cpp \
  src/crazy_linker_elf_relocations.cpp \
  src/crazy_linker_elf_relro.cpp \
  src/crazy_linker_elf_symbols.cpp \
  src/crazy_linker_elf_view.cpp \
  src/crazy_linker_error.cpp \
  src/crazy_linker_globals.cpp \
  src/crazy_linker_library_list.cpp \
  src/crazy_linker_library_view.cpp \
  src/crazy_linker_line_reader.cpp \
  src/crazy_linker_proc_maps.cpp \
  src/crazy_linker_rdebug.cpp \
  src/crazy_linker_search_path_list.cpp \
  src/crazy_linker_shared_library.cpp \
  src/crazy_linker_thread.cpp \
  src/crazy_linker_util.cpp \
  src/crazy_linker_wrappers.cpp \
  src/crazy_linker_system.cpp \
  src/linker_phdr.cpp \

# The crazy linker itself.
include $(CLEAR_VARS)
LOCAL_MODULE := crazy_linker
LOCAL_C_INCLUDES = $(LOCAL_PATH)/include $(LOCAL_PATH)/src
LOCAL_CFLAGS := -Os -fvisibility=hidden -Wall -Werror
LOCAL_SRC_FILES := $(crazy_linker_sources)
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_EXPORT_LDLIBS := -llog
include $(BUILD_STATIC_LIBRARY)

# The crazy linker unit tests.
include $(CLEAR_VARS)

LOCAL_MODULE := crazylinker_unittest
LOCAL_SRC_FILES := \
  $(crazy_linker_sources) \
  src/crazy_linker_ashmem_unittest.cpp \
  src/crazy_linker_error_unittest.cpp \
  src/crazy_linker_line_reader_unittest.cpp \
  src/crazy_linker_system_mock.cpp \
  src/crazy_linker_system_unittest.cpp \
  src/crazy_linker_globals_unittest.cpp \
  src/crazy_linker_proc_maps_unittest.cpp \
  src/crazy_linker_search_path_list_unittest.cpp \
  src/crazy_linker_util_unittest.cpp \
  src/crazy_linker_thread_unittest.cpp \
  minitest/minitest.cc \

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)/src
LOCAL_CFLAGS += -DUNIT_TESTS
LOCAL_LDLIBS := -llog

include $(BUILD_EXECUTABLE)

