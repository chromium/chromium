// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.services.service_manager.InterfaceProvider;
import org.chromium.url.Origin;

/**
 * The RenderFrameHost Java wrapper to allow communicating with the native RenderFrameHost object.
 */
public interface RenderFrameHost {
    /**
     * Get the last committed URL of the frame.
     *
     * @return The last committed URL of the frame or null when being destroyed.
     */
    @Nullable
    String getLastCommittedURL();

    /**
     * Get the last committed Origin of the frame. This is not always the same as scheme/host/port
     * of getLastCommittedURL(), since it can be an "opaque" origin in such cases as, for example,
     * sandboxed frame.
     *
     * @return The last committed Origin of the frame or null when being destroyed.
     */
    @Nullable
    Origin getLastCommittedOrigin();

    /**
     * Fetch the canonical URL associated with the fame.
     *
     * @param callback The callback to be notified once the canonical URL has been fetched.
     */
    void getCanonicalUrlForSharing(Callback<String> callback);

    /**
     * Returns whether the feature policy allows the feature in this frame.
     *
     * @param feature A feature controlled by feature policy.
     *
     * @return Whether the feature policy allows the feature in this frame.
     */
    boolean isFeatureEnabled(@FeaturePolicyFeature int feature);

    /**
     * Returns an InterfaceProvider that provides access to interface implementations provided by
     * the corresponding RenderFrame. This provides access to interfaces implemented in the renderer
     * to Java code in the browser process.
     *
     * @return The InterfaceProvider for the frame.
     */
    InterfaceProvider getRemoteInterfaces();

    /**
     * Notifies the native RenderFrameHost about a user activation from the browser side.
     */
    void notifyUserActivation();

    /**
     * Returns whether we're in incognito mode.
     *
     * @return {@code true} if we're in incognito mode.
     */
    boolean isIncognito();

    /**
     * See native RenderFrameHost::IsRenderFrameCreated().
     *
     * @return {@code true} if render frame is created.
     */
    boolean isRenderFrameCreated();

    /**
     * @return Whether input events from the renderer are ignored on the browser side.
     */
    boolean areInputEventsIgnored();

    /**
     * Runs security checks associated with a Web Authentication GetAssertion request for the
     * the given relying party ID and an effective origin. If the request originated from a render
     * process, then the effective origin is the same as the last committed origin. However, if the
     * request originated from an internal request from the browser process (e.g. Payments
     * Autofill), then the relying party ID would not match the renderer's origin, and will
     * therefore have to provide its own effective origin. The return value is a code corresponding
     * to the AuthenticatorStatus mojo enum.
     *
     * @return Status code indicating the result of the GetAssertion request security checks.
     */
    int performGetAssertionWebAuthSecurityChecks(String relyingPartyId, Origin effectiveOrigin);

    /**
     * Runs security checks associated with a Web Authentication MakeCredential request for the
     * the given relying party ID and an effective origin. See
     * performGetAssertionWebAuthSecurityChecks for more on |effectiveOrigin|. The return value is a
     * code corresponding to the AuthenticatorStatus mojo enum.
     *
     * @return Status code indicating the result of the MakeCredential request security checks.
     */
    int performMakeCredentialWebAuthSecurityChecks(String relyingPartyId, Origin effectiveOrigin);
}
