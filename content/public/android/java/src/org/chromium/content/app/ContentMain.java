// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.os.IBinder;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * This class is used to initialize all types of process. It corresponds to
 * content/public/app/content_main.h which is not used in Android as it has the different
 * initialization process.
 *
 * <p>TODO(michaelbai): Refactorying the BrowserProcessMain.java and the ChildProcessService.java to
 * start ContentMain, and run the process specific initialization code in
 * ContentMainRunner::Initialize.
 */
@JNINamespace("content")
public class ContentMain {
    /**
     * Sets a global set of IBinder refs in native code.
     *
     * @param binderBox A reference to a binder box which contains other binders packed by the
     *     parent process.
     */
    public static void setBindersFromParent(IBinder binderBox) {
        ContentMainJni.get().setBindersFromParent(binderBox);
    }

    /**
     * Start the ContentMainRunner in native side.
     *
     * @param startMinimalBrowser Whether to start only a minimal browser process environment.
     */
    public static int start(boolean startMinimalBrowser) {
        return ContentMainJni.get().start(startMinimalBrowser);
    }

    @NativeMethods
    interface Natives {
        void setBindersFromParent(IBinder binderBox);

        int start(boolean startMinimalBrowser);
    }
}
