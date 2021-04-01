// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.blink.mojom.Authenticator;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.RenderFrameHost;
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
        if (!ContentFeatureList.isEnabled(ContentFeatureList.WEB_AUTH)) {
            return null;
        }

        if (mRenderFrameHost == null) return null;
        return new AuthenticatorImpl(mRenderFrameHost);
    }
}
