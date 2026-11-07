// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;

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
    @JniType("::content::InitiatorNavigationStateWrapper")
    interface NativeInitiatorNavigationStateWrapper extends JniTypeToken {}

    private final UnguessableToken mInitiatorFrameToken;
    private final ChildProcessId mInitiatorProcessId;
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private @Nullable JniUniquePtr<NativeInitiatorNavigationStateWrapper>
            mInitiatorNavigationStateWrapper;

    @CalledByNative
    AdditionalNavigationParamsImpl(
            @JniType("base::UnguessableToken") UnguessableToken initiatorFrameToken,
            @JniType("content::ChildProcessId") ChildProcessId initiatorProcessId,
            @Nullable JniUniquePtr<NativeInitiatorNavigationStateWrapper>
                    initiatorNavigationStateWrapper) {
        mInitiatorFrameToken = initiatorFrameToken;
        mInitiatorProcessId = initiatorProcessId;
        mInitiatorNavigationStateWrapper = initiatorNavigationStateWrapper;
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
    @Nullable JniPtr<NativeInitiatorNavigationStateWrapper> getNativeWrapper() {
        return mInitiatorNavigationStateWrapper;
    }

    @CalledByNative
    @Override
    public void destroy() {
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
        if (mInitiatorNavigationStateWrapper != null) {
            mInitiatorNavigationStateWrapper.destroy();
            mInitiatorNavigationStateWrapper = null;
        }
    }
}
