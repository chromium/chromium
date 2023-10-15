// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

@JNINamespace("mojo::android")
public class NullableValueTypesTestUtil {
    @NativeMethods
    interface Natives {
        void bindTestInterface(long rawMessagePipeHandle);
    }
}
