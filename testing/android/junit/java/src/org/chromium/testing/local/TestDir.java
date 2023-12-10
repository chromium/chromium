// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import java.io.File;

/** Tools to get the junit test path. */
public class TestDir {
    private static final String TEST_FOLDER = "/chrome/test/data/";

    /**
     * Construct the full path of a test data file.
     * @param path Pathname relative to chrome/test/data.
     */
    public static String getTestFilePath(String path) {
        String junitRoot = System.getProperty("dir.source.root");
        if (junitRoot == null) {
            junitRoot = "";
        }
        return new File(junitRoot + TEST_FOLDER + path).getPath();
    }
}
