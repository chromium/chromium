// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

/** JNI bridge with content::Page */
@JNINamespace("content")
@NullMarked
public class Page {
    private boolean mIsPrerendering;

    public static Page createForTesting() {
        return new Page(/* isPrerendering= */ false);
    }

    @CalledByNative
    private Page(boolean isPrerendering) {
        mIsPrerendering = isPrerendering;
    }

    /** The C++ page is about to be deleted. */
    @CalledByNative
    private void willDeletePage(boolean isPrerendering) {
        mIsPrerendering = isPrerendering;
        // TODO(crbug.com/359826084): Make AwContentsObserver a listener to this release.
    }

    public boolean isPrerendering() {
        return mIsPrerendering;
    }
}
