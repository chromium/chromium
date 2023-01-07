// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.os.Build;

/**
 * Factory to construct a TestViewStructure only on "M" and higher,
 * otherwise AssistViewStructureTest fails to run because of the
 * reference to TestViewStructure.
 */
public class TestViewStructureFactory {
    static TestViewStructureInterface createTestViewStructure() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return new TestViewStructure();
        } else {
            return null;
        }
    }
}
