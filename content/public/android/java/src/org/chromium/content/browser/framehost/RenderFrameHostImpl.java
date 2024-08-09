// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.framehost;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.UnguessableToken;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.PermissionsPolicyFeature;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.mojo.bindings.Interface;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.impl.CoreImpl;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.List;

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
    private final GlobalRenderFrameHostId mRenderFrameHostId;

    private RenderFrameHostImpl(
            long nativeRenderFrameHostAndroid,
            RenderFrameHostDelegate delegate,
            boolean isIncognito,
            int renderProcessId,
            int renderFrameId) {
        mNativeRenderFrameHostAndroid = nativeRenderFrameHostAndroid;
        mDelegate = delegate;
        mIncognito = isIncognito;
        mRenderFrameHostId = new GlobalRenderFrameHostId(renderProcessId, renderFrameId);

        mDelegate.renderFrameCreated(this);
    }

    @CalledByNative
    private static RenderFrameHostImpl create(
            long nativeRenderFrameHostAndroid,
            RenderFrameHostDelegate delegate,
            boolean isIncognito,
            int renderProcessId,
            int renderFrameId) {
        return new RenderFrameHostImpl(
                nativeRenderFrameHostAndroid,
                delegate,
                isIncognito,
                renderProcessId,
                renderFrameId);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeRenderFrameHostAndroid = 0;
        mDelegate.renderFrameDeleted(this);
    }

    @CalledByNative
    private long getNativePointer() {
        return mNativeRenderFrameHostAndroid;
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
    @Nullable
    public GURL getLastCommittedURL() {
        if (mNativeRenderFrameHostAndroid == 0) return null;
        return RenderFrameHostImplJni.get().getLastCommittedURL(mNativeRenderFrameHostAndroid);
    }

    @Override
    @Nullable
    public Origin getLastCommittedOrigin() {
        if (mNativeRenderFrameHostAndroid == 0) return null;
        return RenderFrameHostImplJni.get().getLastCommittedOrigin(mNativeRenderFrameHostAndroid);
    }

    @Override
    @Nullable
    public RenderFrameHost getMainFrame() {
        if (mNativeRenderFrameHostAndroid == 0) return null;
        return RenderFrameHostImplJni.get().getMainFrame(mNativeRenderFrameHostAndroid);
    }

    @Override
    public void getCanonicalUrlForSharing(Callback<GURL> callback) {
        if (mNativeRenderFrameHostAndroid == 0) {
            callback.onResult(null);
            return;
        }
        RenderFrameHostImplJni.get()
                .getCanonicalUrlForSharing(mNativeRenderFrameHostAndroid, callback);
    }

    @Override
    public List<RenderFrameHost> getAllRenderFrameHosts() {
        if (mNativeRenderFrameHostAndroid == 0) return null;
        return RenderFrameHostImplJni.get().getAllRenderFrameHosts(mNativeRenderFrameHostAndroid);
    }

    @Override
    public boolean isFeatureEnabled(@PermissionsPolicyFeature int feature) {
        return mNativeRenderFrameHostAndroid != 0
                && RenderFrameHostImplJni.get()
                        .isFeatureEnabled(mNativeRenderFrameHostAndroid, feature);
    }

    /**
     * TODO(timloh): This function shouldn't really be on here. If we end up needing more logic from
     * the native BrowserContext, we should add a wrapper for that and move this function there.
     */
    @Override
    public boolean isIncognito() {
        return mIncognito;
    }

    @Override
    public void notifyUserActivation() {
        if (mNativeRenderFrameHostAndroid == 0) return;
        RenderFrameHostImplJni.get().notifyUserActivation(mNativeRenderFrameHostAndroid);
    }

    @Override
    public void notifyWebAuthnAssertionRequestSucceeded() {
        if (mNativeRenderFrameHostAndroid == 0) return;
        RenderFrameHostImplJni.get()
                .notifyWebAuthnAssertionRequestSucceeded(mNativeRenderFrameHostAndroid);
    }

    @Override
    public boolean isCloseWatcherActive() {
        return RenderFrameHostImplJni.get().isCloseWatcherActive(mNativeRenderFrameHostAndroid);
    }

    @Override
    public boolean signalCloseWatcherIfActive() {
        return RenderFrameHostImplJni.get()
                .signalCloseWatcherIfActive(mNativeRenderFrameHostAndroid);
    }

    @Override
    public boolean isRenderFrameLive() {
        if (mNativeRenderFrameHostAndroid == 0) return false;
        return RenderFrameHostImplJni.get().isRenderFrameLive(mNativeRenderFrameHostAndroid);
    }

    @Override
    public <I extends Interface, P extends Interface.Proxy> P getInterfaceToRendererFrame(
            Interface.Manager<I, P> manager) {
        if (mNativeRenderFrameHostAndroid == 0) return null;
        Pair<P, InterfaceRequest<I>> result = manager.getInterfaceRequest(CoreImpl.getInstance());
        RenderFrameHostImplJni.get()
                .getInterfaceToRendererFrame(
                        mNativeRenderFrameHostAndroid,
                        manager.getName(),
                        result.second.passHandle().releaseNativeHandle());
        return result.first;
    }

    @Override
    public void terminateRendererDueToBadMessage(int reason) {
        if (mNativeRenderFrameHostAndroid == 0) return;
        RenderFrameHostImplJni.get()
                .terminateRendererDueToBadMessage(mNativeRenderFrameHostAndroid, reason);
    }

    /** Return the AndroidOverlay routing token for this RenderFrameHostImpl. */
    @Nullable
    public UnguessableToken getAndroidOverlayRoutingToken() {
        if (mNativeRenderFrameHostAndroid == 0) return null;
        return RenderFrameHostImplJni.get()
                .getAndroidOverlayRoutingToken(mNativeRenderFrameHostAndroid);
    }

    @Override
    public boolean areInputEventsIgnored() {
        if (mNativeRenderFrameHostAndroid == 0) return false;
        return RenderFrameHostImplJni.get().isProcessBlocked(mNativeRenderFrameHostAndroid);
    }

    @Override
    public void performGetAssertionWebAuthSecurityChecks(
            String relyingPartyId,
            Origin effectiveOrigin,
            boolean isPaymentCredentialGetAssertion,
            Callback<RenderFrameHost.WebAuthSecurityChecksResults> callback) {
        if (mNativeRenderFrameHostAndroid == 0) {
            var result =
                    new WebAuthSecurityChecksResults(
                            AuthenticatorStatus.UNKNOWN_ERROR, /* isCrossOrigin= */ false);
            callback.onResult(result);
            return;
        }
        RenderFrameHostImplJni.get()
                .performGetAssertionWebAuthSecurityChecks(
                        mNativeRenderFrameHostAndroid,
                        relyingPartyId,
                        effectiveOrigin,
                        isPaymentCredentialGetAssertion,
                        callback);
    }

    @CalledByNative
    private static RenderFrameHost.WebAuthSecurityChecksResults createWebAuthSecurityChecksResults(
            @AuthenticatorStatus.EnumType int securityCheckResult, boolean isCrossOrigin) {
        return new WebAuthSecurityChecksResults(securityCheckResult, isCrossOrigin);
    }

    @CalledByNative
    private static void onEvaluateJavaScriptResult(String jsonResult, JavaScriptCallback callback) {
        callback.handleJavaScriptResult(jsonResult);
    }

    @Override
    public void performMakeCredentialWebAuthSecurityChecks(
            String relyingPartyId,
            Origin effectiveOrigin,
            boolean isPaymentCredentialCreation,
            Callback<RenderFrameHost.WebAuthSecurityChecksResults> callback) {
        if (mNativeRenderFrameHostAndroid == 0) {
            var result =
                    new WebAuthSecurityChecksResults(
                            AuthenticatorStatus.UNKNOWN_ERROR, /* isCrossOrigin= */ false);
            callback.onResult(result);
            return;
        }

        RenderFrameHostImplJni.get()
                .performMakeCredentialWebAuthSecurityChecks(
                        mNativeRenderFrameHostAndroid,
                        relyingPartyId,
                        effectiveOrigin,
                        isPaymentCredentialCreation,
                        callback);
    }

    @Override
    public GlobalRenderFrameHostId getGlobalRenderFrameHostId() {
        return mRenderFrameHostId;
    }

    @Override
    @LifecycleState
    public int getLifecycleState() {
        if (mNativeRenderFrameHostAndroid == 0) return LifecycleState.PENDING_DELETION;
        return RenderFrameHostImplJni.get().getLifecycleState(mNativeRenderFrameHostAndroid);
    }

    @Override
    public void insertVisualStateCallback(Callback<Boolean> callback) {
        if (mNativeRenderFrameHostAndroid == 0) {
            callback.onResult(false);
        }
        RenderFrameHostImplJni.get()
                .insertVisualStateCallback(mNativeRenderFrameHostAndroid, callback);
    }

    @Override
    public void executeJavaScriptInIsolatedWorld(
            String script, int worldId, @Nullable JavaScriptCallback callback) {
        RenderFrameHostImplJni.get()
                .executeJavaScriptInIsolatedWorld(
                        mNativeRenderFrameHostAndroid, script, worldId, callback);
    }

    @NativeMethods
    interface Natives {
        GURL getLastCommittedURL(long nativeRenderFrameHostAndroid);

        Origin getLastCommittedOrigin(long nativeRenderFrameHostAndroid);

        RenderFrameHost getMainFrame(long nativeRenderFrameHostAndroid);

        void getCanonicalUrlForSharing(long nativeRenderFrameHostAndroid, Callback<GURL> callback);

        @JniType("std::vector")
        List<RenderFrameHost> getAllRenderFrameHosts(long nativeRenderFrameHostAndroid);

        boolean isFeatureEnabled(
                long nativeRenderFrameHostAndroid, @PermissionsPolicyFeature int feature);

        @JniType("base::UnguessableToken")
        UnguessableToken getAndroidOverlayRoutingToken(long nativeRenderFrameHostAndroid);

        void notifyUserActivation(long nativeRenderFrameHostAndroid);

        void notifyWebAuthnAssertionRequestSucceeded(long nativeRenderFrameHostAndroid);

        boolean isCloseWatcherActive(long nativeRenderFrameHostAndroid);

        boolean signalCloseWatcherIfActive(long nativeRenderFrameHostAndroid);

        boolean isRenderFrameLive(long nativeRenderFrameHostAndroid);

        void getInterfaceToRendererFrame(
                long nativeRenderFrameHostAndroid, String interfacename, long messagePipeRawHandle);

        void terminateRendererDueToBadMessage(long nativeRenderFrameHostAndroid, int reason);

        boolean isProcessBlocked(long nativeRenderFrameHostAndroid);

        void performGetAssertionWebAuthSecurityChecks(
                long nativeRenderFrameHostAndroid,
                String relyingPartyId,
                Origin effectiveOrigin,
                boolean isPaymentCredentialGetAssertion,
                Callback<RenderFrameHost.WebAuthSecurityChecksResults> callback);

        void performMakeCredentialWebAuthSecurityChecks(
                long nativeRenderFrameHostAndroid,
                String relyingPartyId,
                Origin effectiveOrigin,
                boolean isPaymentCredentialCreation,
                Callback<RenderFrameHost.WebAuthSecurityChecksResults> callback);

        int getLifecycleState(long nativeRenderFrameHostAndroid);

        void insertVisualStateCallback(
                long nativeRenderFrameHostAndroid, Callback<Boolean> callback);

        void executeJavaScriptInIsolatedWorld(
                long nativeRenderFrameHostAndroid,
                String stript,
                int isolatedWorldId,
                JavaScriptCallback callback);
    }
}
