// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.UnguessableToken;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.AdditionalNavigationParams;
import org.chromium.content_public.common.ChildProcessId;

/** Implementation of {@link AdditionalNavigationParams}. */
@JNINamespace("content")
@NullMarked
public class AdditionalNavigationParamsImpl implements AdditionalNavigationParams {
    private final UnguessableToken mInitiatorFrameToken;
    private final ChildProcessId mInitiatorProcessId;
    private final @Nullable UnguessableToken mAttributionSrcToken;
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private long mNativePtr;

    @CalledByNative
    private AdditionalNavigationParamsImpl(
            @JniType("base::UnguessableToken") UnguessableToken initiatorFrameToken,
            @JniType("content::ChildProcessId") ChildProcessId initiatorProcessId,
            @JniType("std::optional<base::UnguessableToken>")
                    @Nullable UnguessableToken attributionSrcToken,
            long nativePtr) {
        mInitiatorFrameToken = initiatorFrameToken;
        mInitiatorProcessId = initiatorProcessId;
        mAttributionSrcToken = attributionSrcToken;
        mNativePtr = nativePtr;
    }

    @CalledByNative
    private @JniType("std::optional<base::UnguessableToken>") UnguessableToken
            getInitiatorFrameToken() {
        return mInitiatorFrameToken;
    }

    @CalledByNative
    private @JniType("content::ChildProcessId") ChildProcessId getInitiatorProcessId() {
        return mInitiatorProcessId;
    }

    @CalledByNative
    private @JniType("std::optional<base::UnguessableToken>") @Nullable UnguessableToken
            getAttributionSrcToken() {
        return mAttributionSrcToken;
    }

    @CalledByNative
    private long takeNativeState() {
        long ptr = mNativePtr;
        mNativePtr = 0;
        return ptr;
    }

    @CalledByNative
    @Override
    public void destroy() {
        LifetimeAssert.destroy(mLifetimeAssert);
        if (mNativePtr != 0) {
            AdditionalNavigationParamsImplJni.get().destroy(mNativePtr);
            mNativePtr = 0;
        }
    }

    @NativeMethods
    interface Natives {
        void destroy(long statePtr);
    }
}
