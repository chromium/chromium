// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import com.oracle.svm.core.annotate.Substitute;
import com.oracle.svm.core.annotate.TargetClass;

/**
 * EDT.isCurrentThreadEdt() asks java.awt.EventQueue, which initializes the AWT toolkit and fails in
 * the native image. Lint never runs an AWT event queue, so on the JVM the answer is always false
 * too.
 */
@TargetClass(className = "com.intellij.util.ui.EDT")
final class Target_EDT {
    @Substitute
    public static boolean isCurrentThreadEdt() {
        return false;
    }
}
