// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.on_device_model.mojom.GenerateOptions;

@JNINamespace("on_device_model")
@NullMarked
class GenerateOptionsHelper {
    @CalledByNative
    private static GenerateOptions create(int maxOutputTokens) {
        GenerateOptions generateOptions = new GenerateOptions();
        generateOptions.maxOutputTokens = maxOutputTokens;
        return generateOptions;
    }
}
