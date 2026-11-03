// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** Native test helpers for {@link MojoJniBridgeTest}. */
@JNINamespace("mojo::android")
public class MojoJniBridgeTestUtil {
    @NativeMethods
    interface Natives {
        long createStub();

        void destroyStub(long stub);

        long createBridge(long stub);

        void destroyBridge(long bridge);

        int getLastOneWayX(long stub);

        long createResultStub();

        void destroyResultStub(long stub);

        long createResultBridge(long stub);
    }
}
