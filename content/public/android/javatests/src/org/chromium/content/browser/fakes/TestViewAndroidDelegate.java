// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.fakes;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.ui.base.ViewAndroidDelegate;

/**
 * Mimics the Chrome TestViewAndroidDelegate in chrome/browser/tab for use in tests,
 * driven by the {@code RenderWidgetHostViewAndroidTest}.
 */
@JNINamespace("content")
class TestViewAndroidDelegate extends ViewAndroidDelegate {
    /** Stores the Visual Viewport bottom inset when under test, just like the real one. */
    private int mApplicationViewportInsetBottomPx;

    /** Private constructor called by the create method from native. */
    private TestViewAndroidDelegate() {
        super(null);
    }

    /**
     * Creates an instance that's similar in behavior to the other various ViewAndroidDelegates.
     * @return A fake {@link TestViewAndroidDelegate} to be used in a native test.
     */
    @CalledByNative
    private static TestViewAndroidDelegate create() {
        return new TestViewAndroidDelegate();
    }

    /**
     * Insets the Visual Viewport bottom, just like the real {@code TestViewAndroidDelegate} does.
     * @param viewportInsetBottomPx Amount to inset.
     */
    @CalledByNative
    private void insetViewportBottom(int viewportInsetBottomPx) {
        mApplicationViewportInsetBottomPx = viewportInsetBottomPx;
    }

    @Override
    protected int getViewportInsetBottom() {
        return mApplicationViewportInsetBottomPx;
    }
}
