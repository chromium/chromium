// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.blink.mojom.Authenticator;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebAuthenticationDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.services.service_manager.InterfaceFactory;

/**
 * Factory class registered to create Authenticators upon request.
 */
public class AuthenticatorFactory implements InterfaceFactory<Authenticator> {
    private final RenderFrameHost mRenderFrameHost;

    public AuthenticatorFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    @Override
    public Authenticator createImpl() {
        if (mRenderFrameHost == null) {
            return null;
        }
        WebContents webContents = WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
        if (webContents == null) {
            return null;
        }

        WebAuthenticationDelegate delegate = new WebAuthenticationDelegate();
        @WebAuthenticationDelegate.Support
        int supportLevel = delegate.getSupportLevel(webContents);
        if (supportLevel == WebAuthenticationDelegate.Support.NONE) {
            return null;
        }

        return new AuthenticatorImpl(
                delegate.getIntentSender(webContents), mRenderFrameHost, supportLevel);
    }
}
