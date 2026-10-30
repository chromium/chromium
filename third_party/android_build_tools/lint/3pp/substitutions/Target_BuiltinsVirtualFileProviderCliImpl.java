// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import com.intellij.openapi.vfs.VirtualFile;
import com.intellij.openapi.vfs.VirtualFileManager;
import com.oracle.svm.core.annotate.Substitute;
import com.oracle.svm.core.annotate.TargetClass;

import java.io.File;
import java.net.URL;

/**
 * Kotlin's analysis API reads builtins (kotlin/*.kotlin_builtins) through the jar:// VFS, but
 * native-image serves resources as resource: URLs. Read them from lint-builtins.jar next to the
 * executable (or $LINT_BUILTINS_JAR).
 */
@TargetClass(
        className =
                "org.jetbrains.kotlin.analysis.decompiler.psi.BuiltinsVirtualFileProviderCliImpl")
final class Target_BuiltinsVirtualFileProviderCliImpl {
    @Substitute
    private VirtualFile findVirtualFile(URL url) {
        String entry = url.getPath();
        int sep = entry.indexOf("!/");
        if (sep >= 0) {
            entry = entry.substring(sep + 2);
        }
        while (entry.startsWith("/")) {
            entry = entry.substring(1);
        }
        String jar = System.getenv("LINT_BUILTINS_JAR");
        if (jar == null) {
            String exe = ProcessHandle.current().info().command().orElse("");
            jar = new File(new File(exe).getParentFile(), "lint-builtins.jar").getPath();
        }
        return VirtualFileManager.getInstance()
                .getFileSystem("jar")
                .findFileByPath(jar + "!/" + entry);
    }
}
