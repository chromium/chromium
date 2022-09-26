# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libfoo
LOCAL_SRC_FILES := foo.cpp
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libfoo2
LOCAL_SRC_FILES := foo2.cpp
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libfoo_with_static_constructor
LOCAL_SRC_FILES := foo_with_static_constructor.cpp
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libfoo_with_relro
LOCAL_SRC_FILES := foo_with_relro.cpp
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libbar
LOCAL_SRC_FILES := bar.cpp
LOCAL_SHARED_LIBRARIES := libfoo
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libbar_with_relro
LOCAL_SRC_FILES := bar_with_relro.cpp
LOCAL_SHARED_LIBRARIES := libfoo_with_relro
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libzoo
LOCAL_SRC_FILES := zoo.cpp
LOCAL_LDLIBS := -ldl
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libjni_lib
LOCAL_SRC_FILES := jni_lib.cpp
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := test_load_library
LOCAL_SRC_FILES := test_load_library.cpp
LOCAL_STATIC_LIBRARIES := crazy_linker
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := test_load_library_depends
LOCAL_SRC_FILES := test_load_library_depends.cpp
LOCAL_STATIC_LIBRARIES := crazy_linker
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := test_load_library_callbacks
LOCAL_SRC_FILES := test_load_library_callbacks.cpp
LOCAL_STATIC_LIBRARIES := crazy_linker
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := test_dl_wrappers
LOCAL_SRC_FILES := test_dl_wrappers.cpp
LOCAL_STATIC_LIBRARIES := crazy_linker
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := test_constructors_destructors
LOCAL_SRC_FILES := test_constructors_destructors.cpp
LOCAL_STATIC_LIBRARIES := crazy_linker
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := test_shared_relro
LOCAL_SRC_FILES := test_shared_relro.cpp
LOCAL_STATIC_LIBRARIES := crazy_linker
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := test_relocated_shared_relro
LOCAL_SRC_FILES := test_relocated_shared_relro.cpp
LOCAL_STATIC_LIBRARIES := crazy_linker
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := test_two_shared_relros
LOCAL_SRC_FILES := test_two_shared_relros.cpp
LOCAL_STATIC_LIBRARIES := crazy_linker
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := test_search_path_list
LOCAL_SRC_FILES := test_search_path_list.cpp
LOCAL_STATIC_LIBRARIES := crazy_linker
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := test_jni_hooks
LOCAL_SRC_FILES := test_jni_hooks.cpp
LOCAL_STATIC_LIBRARIES := crazy_linker
include $(BUILD_EXECUTABLE)


ifneq (,$(strip $(CRAZY_BENCH)))
include $(CLEAR_VARS)
LOCAL_MODULE := bench_load_library
LOCAL_SRC_FILES := $(LOCAL_MODULE).cpp
LOCAL_STATIC_LIBRARIES := crazy_linker
include $(BUILD_EXECUTABLE)
endif

include $(LOCAL_PATH)/../Android.mk
