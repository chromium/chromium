#!/bin/bash
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x

this_dir=$(dirname $0)
cpp_dest="$this_dir/cpp"

# Create directory if it doesn't exist
mkdir -p "$cpp_dest"

# Clean up only downloaded files, preserving local initializers
find "$cpp_dest" -type f -delete

# Create a temp directory
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

# 1. Download and extract C++ files
curl -sL "https://android.googlesource.com/platform/frameworks/support/+archive/androidx-main/graphics/graphics-core/src/main/cpp.tar.gz" > "$tmp_dir/cpp.tar.gz"
mkdir -p "$tmp_dir/cpp"
tar -xzf "$tmp_dir/cpp.tar.gz" -C "$tmp_dir/cpp"
# Copy only .cpp and .h files to cpp_dest, excluding test utils
find "$tmp_dir/cpp" \( -name "*.cpp" -o -name "*.h" \) ! -name "sc_test_utils.cpp" | while read file; do
    cp "$file" "$cpp_dest/"
done

# Apply patch to rename JNI_OnLoad and change signature
patch -p0 -d "$this_dir" <<'EOF'
--- cpp/graphics-core.cpp
+++ cpp/graphics-core.cpp
@@ -678,12 +678,7 @@
 };
 
 extern "C"
-JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
-    JNIEnv *env;
-    ALOGE("GraphicsCore JNI_OnLoad start");
-    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
-        return JNI_ERR;
-    }
+jint GraphicsCore_JNI_OnLoad(JNIEnv* env) {
 
     jclass clazz = env->FindClass("androidx/graphics/surface/JniBindings");
     if(clazz == nullptr) {
EOF
# Make EGL function pointers static in egl_utils.cpp to avoid linker conflicts
sed -i 's/^PFN/static PFN/' "$cpp_dest/egl_utils.cpp"
