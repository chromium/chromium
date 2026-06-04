// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.androidx;

import org.jni_zero.NativeMethods;

/** Helper to initialize native code for androidx.graphics-core. */
public class GraphicsCoreJniLoader {
    private static boolean sInitialized;

    public static void ensureInitialized() {
        synchronized (GraphicsCoreJniLoader.class) {
            if (!sInitialized) {
                sInitialized = true;
                GraphicsCoreJniLoaderJni.get().init();
            }
        }
    }

    @NativeMethods
    interface Natives {
        void init();
    }
}
