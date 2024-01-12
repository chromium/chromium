// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.blink.mojom.Authenticator;
import org.chromium.components.webauthn.WebauthnModeProvider.WebauthnMode;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.Origin;

/** Factory class registered to create Authenticators upon request. */
public class AuthenticatorFactory implements InterfaceFactory<Authenticator> {
    private final RenderFrameHost mRenderFrameHost;
    private final CreateConfirmationUiDelegate.Factory mConfirmationFactory;
    private final @WebauthnMode int mMode;

    public AuthenticatorFactory(
            RenderFrameHost renderFrameHost,
            CreateConfirmationUiDelegate.Factory confirmationFactory,
            @WebauthnMode int mode) {
        mRenderFrameHost = renderFrameHost;
        mConfirmationFactory = confirmationFactory;
        mMode = mode;
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

        WindowAndroid window = webContents.getTopLevelNativeWindow();
        Context context = null;
        // In practice, `window` is sometimes null for unclear reasons (crbug.com/1459476).
        if (window != null) {
            context = window.getActivity().get();
        }
        if (context == null) {
            context = ContextUtils.getApplicationContext();
        }

        CreateConfirmationUiDelegate createConfirmationUiDelegate =
                mConfirmationFactory == null ? null : mConfirmationFactory.create(webContents);
        Origin topOrigin = webContents.getMainFrame().getLastCommittedOrigin();
        return new AuthenticatorImpl(
                context,
                new AuthenticatorImpl.WindowIntentSender(window),
                createConfirmationUiDelegate,
                mRenderFrameHost,
                topOrigin,
                mMode);
    }
}
