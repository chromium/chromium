// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromium_linker_test_apk;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.Linker;

/**
 * A class that is only used in linker test APK to perform runtime checks
 * in the current process.
 */
@JNINamespace("content")
public class LinkerTests implements Linker.TestRunner {
    private static final String TAG = "LinkerTest";

    public LinkerTests() {}

    @Override
    public boolean runChecks(boolean isBrowserProcess) {
        return LinkerTestsJni.get().checkForSharedRelros(isBrowserProcess);
    }

    @NativeMethods
    interface Natives {
        // Check that there are shared RELRO sections in the current process,
        // and that they are properly mapped read-only. Returns true on success.
        boolean checkForSharedRelros(boolean isBrowserProcess);
    }
}
