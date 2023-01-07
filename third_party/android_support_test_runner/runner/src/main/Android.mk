# Copyright (C) 2012 The Android Open Source Project
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
#

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, java)

LOCAL_MODULE := android-support-test-src

LOCAL_MODULE_TAGS := optional

LOCAL_SDK_VERSION := 8
LOCAL_STATIC_JAVA_LIBRARIES := junit4-target

include $(BUILD_STATIC_JAVA_LIBRARY)

# ------------------------------------------------
# build a version without bundled dependencies
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, java)

LOCAL_MODULE := android-support-test-nodeps

LOCAL_MODULE_TAGS := optional

LOCAL_SDK_VERSION := 8
LOCAL_JAVA_LIBRARIES := junit4-target

include $(BUILD_STATIC_JAVA_LIBRARY)

# -----------------------------------------------
# build a droiddoc package for integration in d.android.com
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, java) \
  $(call junit4_to_document, ../../../../../../external/junit/src/org) \
  $(call all-java-files-under, ../../../../../../external/hamcrest/src)

LOCAL_MODULE := android-support-test-docs
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_JAVA_LIBRARIES := android-support-test-src
LOCAL_SDK_VERSION := 8
LOCAL_IS_HOST_MODULE := false
LOCAL_DROIDDOC_CUSTOM_TEMPLATE_DIR := build/tools/droiddoc/templates-sdk

LOCAL_DROIDDOC_OPTIONS := \
        -hdf android.whichdoc online \
        -hdf template.showLanguageMenu true

include $(BUILD_DROIDDOC)

# ----------------------------------------------
# build a offline droiddoc package
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, java) \
  $(call all-java-files-under, ../../../../../../external/junit/src/org) \
  $(call all-java-files-under, ../../../../../../external/hamcrest/src)

LOCAL_MODULE := android-support-test-offline-docs
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_JAVA_LIBRARIES := android-support-test-src
LOCAL_SDK_VERSION := 8
LOCAL_IS_HOST_MODULE := false
LOCAL_DROIDDOC_CUSTOM_TEMPLATE_DIR := build/tools/droiddoc/templates-sdk

LOCAL_DROIDDOC_OPTIONS := \
        -offlinemode \
        -hdf android.whichdoc offline \
        -hdf template.showLanguageMenu true

include $(BUILD_DROIDDOC)

# ----------------------------------------------

# Use the following include to make our test apk.
include $(call all-makefiles-under,$(LOCAL_PATH))

