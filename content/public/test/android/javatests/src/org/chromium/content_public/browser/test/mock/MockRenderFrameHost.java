// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.mock;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.PermissionsPolicyFeature;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.mojo.bindings.Interface;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.Collections;
import java.util.List;

/** Mock class for {@link RenderFrameHost}. */
public class MockRenderFrameHost implements RenderFrameHost {
    @Override
    public GURL getLastCommittedURL() {
        return null;
    }

    @Override
    public Origin getLastCommittedOrigin() {
        return null;
    }

    @Override
    public RenderFrameHost getMainFrame() {
        return null;
    }

    @Override
    public void getCanonicalUrlForSharing(Callback<GURL> callback) {}

    @Override
    public List<RenderFrameHost> getAllRenderFrameHosts() {
        return Collections.emptyList();
    }

    @Override
    public boolean isFeatureEnabled(@PermissionsPolicyFeature int feature) {
        return false;
    }

    @Override
    public <I extends Interface, P extends Interface.Proxy> P getInterfaceToRendererFrame(
            Interface.Manager<I, P> manager) {
        return null;
    }

    @Override
    public void terminateRendererDueToBadMessage(int reason) {}

    @Override
    public void notifyUserActivation() {}

    @Override
    public void notifyWebAuthnAssertionRequestSucceeded() {}

    @Override
    public boolean isIncognito() {
        return false;
    }

    @Override
    public boolean isCloseWatcherActive() {
        return false;
    }

    @Override
    public boolean signalCloseWatcherIfActive() {
        return false;
    }

    @Override
    public boolean isRenderFrameLive() {
        return false;
    }

    @Override
    public boolean areInputEventsIgnored() {
        return false;
    }

    @Override
    public void performGetAssertionWebAuthSecurityChecks(
            String relyingPartyId,
            Origin effectiveOrigin,
            boolean isPaymentCredentialGetAssertion,
            Callback<WebAuthSecurityChecksResults> callback) {
        callback.onResult(new WebAuthSecurityChecksResults(AuthenticatorStatus.SUCCESS, false));
    }

    @Override
    public void performMakeCredentialWebAuthSecurityChecks(
            String relyingPartyId,
            Origin effectiveOrigin,
            boolean isPaymentCredentialCreation,
            Callback<WebAuthSecurityChecksResults> callback) {
        callback.onResult(new WebAuthSecurityChecksResults(AuthenticatorStatus.SUCCESS, false));
    }

    @Override
    public GlobalRenderFrameHostId getGlobalRenderFrameHostId() {
        return new GlobalRenderFrameHostId(-1, -1);
    }

    @Override
    public int getLifecycleState() {
        return LifecycleState.ACTIVE;
    }

    @Override
    public void insertVisualStateCallback(Callback<Boolean> callback) {}

    @Override
    public void executeJavaScriptInIsolatedWorld(
            String script, int worldId, @Nullable JavaScriptCallback callback) {}
}
