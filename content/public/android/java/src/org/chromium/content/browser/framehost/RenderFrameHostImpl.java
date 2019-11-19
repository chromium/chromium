// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.framehost;

import org.chromium.base.Callback;
import org.chromium.base.UnguessableToken;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.mojo.system.impl.CoreImpl;
import org.chromium.services.service_manager.InterfaceProvider;
import org.chromium.url.Origin;

/**
 * The RenderFrameHostImpl Java wrapper to allow communicating with the native RenderFrameHost
 * object.
 */
@JNINamespace("content")
public class RenderFrameHostImpl implements RenderFrameHost {
    private long mNativeRenderFrameHostAndroid;
    // mDelegate can be null.
    private final RenderFrameHostDelegate mDelegate;
    private final boolean mIncognito;
    private final InterfaceProvider mInterfaceProvider;

    private RenderFrameHostImpl(long nativeRenderFrameHostAndroid, RenderFrameHostDelegate delegate,
            boolean isIncognito, int nativeInterfaceProviderHandle) {
        mNativeRenderFrameHostAndroid = nativeRenderFrameHostAndroid;
        mDelegate = delegate;
        mIncognito = isIncognito;
        mInterfaceProvider =
                new InterfaceProvider(CoreImpl.getInstance()
                                              .acquireNativeHandle(nativeInterfaceProviderHandle)
                                              .toMessagePipeHandle());

        mDelegate.renderFrameCreated(this);
    }

    @CalledByNative
    private static RenderFrameHostImpl create(long nativeRenderFrameHostAndroid,
            RenderFrameHostDelegate delegate, boolean isIncognito,
            int nativeInterfaceProviderHandle) {
        return new RenderFrameHostImpl(
                nativeRenderFrameHostAndroid, delegate, isIncognito, nativeInterfaceProviderHandle);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeRenderFrameHostAndroid = 0;
        mDelegate.renderFrameDeleted(this);
    }

    /**
     * Get the delegate associated with this RenderFrameHost.
     *
     * @return The delegate associated with this RenderFrameHost.
     */
    public RenderFrameHostDelegate getRenderFrameHostDelegate() {
        return mDelegate;
    }

    public long getNativePtr() {
        return mNativeRenderFrameHostAndroid;
    }

    @Override
    public String getLastCommittedURL() {
        if (mNativeRenderFrameHostAndroid == 0) return null;
        return RenderFrameHostImplJni.get().getLastCommittedURL(
                mNativeRenderFrameHostAndroid, RenderFrameHostImpl.this);
    }

    @Override
    public Origin getLastCommittedOrigin() {
        if (mNativeRenderFrameHostAndroid == 0) return null;
        return RenderFrameHostImplJni.get().getLastCommittedOrigin(
                mNativeRenderFrameHostAndroid, RenderFrameHostImpl.this);
    }

    @Override
    public void getCanonicalUrlForSharing(Callback<String> callback) {
        if (mNativeRenderFrameHostAndroid == 0) {
            callback.onResult(null);
            return;
        }
        RenderFrameHostImplJni.get().getCanonicalUrlForSharing(
                mNativeRenderFrameHostAndroid, RenderFrameHostImpl.this, callback);
    }

    @Override
    public InterfaceProvider getRemoteInterfaces() {
        return mInterfaceProvider;
    }

    /**
     * TODO(timloh): This function shouldn't really be on here. If we end up
     * needing more logic from the native BrowserContext, we should add a
     * wrapper for that and move this function there.
     */
    @Override
    public boolean isIncognito() {
        return mIncognito;
    }

    @Override
    public void notifyUserActivation() {
        RenderFrameHostImplJni.get().notifyUserActivation(
                mNativeRenderFrameHostAndroid, RenderFrameHostImpl.this);
    }

    @Override
    public boolean isRenderFrameCreated() {
        return RenderFrameHostImplJni.get().isRenderFrameCreated(
                mNativeRenderFrameHostAndroid, RenderFrameHostImpl.this);
    }

    /**
     * Return the AndroidOverlay routing token for this RenderFrameHostImpl.
     */
    public UnguessableToken getAndroidOverlayRoutingToken() {
        if (mNativeRenderFrameHostAndroid == 0) return null;
        return RenderFrameHostImplJni.get().getAndroidOverlayRoutingToken(
                mNativeRenderFrameHostAndroid, RenderFrameHostImpl.this);
    }

    @Override
    public boolean areInputEventsIgnored() {
        if (mNativeRenderFrameHostAndroid == 0) return false;
        return RenderFrameHostImplJni.get().isProcessBlocked(
                mNativeRenderFrameHostAndroid, RenderFrameHostImpl.this);
    }

    @NativeMethods
    interface Natives {
        String getLastCommittedURL(long nativeRenderFrameHostAndroid, RenderFrameHostImpl caller);
        Origin getLastCommittedOrigin(
                long nativeRenderFrameHostAndroid, RenderFrameHostImpl caller);
        void getCanonicalUrlForSharing(long nativeRenderFrameHostAndroid,
                RenderFrameHostImpl caller, Callback<String> callback);
        UnguessableToken getAndroidOverlayRoutingToken(
                long nativeRenderFrameHostAndroid, RenderFrameHostImpl caller);
        void notifyUserActivation(long nativeRenderFrameHostAndroid, RenderFrameHostImpl caller);
        boolean isRenderFrameCreated(long nativeRenderFrameHostAndroid, RenderFrameHostImpl caller);
        boolean isProcessBlocked(long nativeRenderFrameHostAndroid, RenderFrameHostImpl caller);
    }
}
