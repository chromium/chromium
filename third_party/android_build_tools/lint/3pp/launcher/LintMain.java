// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.lint;

import com.android.tools.lint.Main;
import com.android.tools.lint.annotations.ExtractAnnotationsDriver;

import java.util.Arrays;

/**
 * Main class of lint-graalvm, so that one image serves both tools.
 *
 * <p>With --extract-annotations as the first argument, runs lint-gradle's annotation extractor
 * (which writes annotations.zip for typedefs and other CLASS-retention annotations). Otherwise
 * runs lint.
 */
public class LintMain {
    public static void main(String[] args) throws Exception {
        if (args.length > 0 && args[0].equals("--extract-annotations")) {
            String[] rest = Arrays.copyOfRange(args, 1, args.length);
            System.exit(new ExtractAnnotationsDriver().run(rest));
        }
        Main.main(args);
    }
}
