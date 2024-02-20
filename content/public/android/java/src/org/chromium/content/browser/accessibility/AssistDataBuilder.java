// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * Basic helper class to build ViewStructure objects for the WebContents in Chrome. The tree is
 * commonly referred to as the "AssistData" tree, hence the name, and to keep it separate from the
 * existing {@link ViewStructureBuilder.java} class which will be replaced by this class once the
 * unification of the code-paths is complete. This class can be used by the Native-side code {@link
 * web_contents_accessibility_android.cc} to construct objects for the virtual view hierarchy to
 * provide to the Android framework.
 */
@JNINamespace("content")
public class AssistDataBuilder {
    // Stubbed.
    @CalledByNative
    public void stubbed() {}
}
