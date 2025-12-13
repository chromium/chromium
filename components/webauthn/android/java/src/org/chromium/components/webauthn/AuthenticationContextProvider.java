// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;

@NullMarked
public interface AuthenticationContextProvider {
    @Nullable Context getContext();

    @Nullable RenderFrameHost getRenderFrameHost();

    FidoIntentSender getIntentSender();

    @Nullable WebContents getWebContents();

    /**
     * Returns the request callback for the current WebAuthn operation.
     *
     * <p>This is used to access the request callback for the current WebAuthn operation. It is not
     * meant to be cached since the Authenticator interface can be reset and hence the callback will
     * be invalid.
     */
    @Nullable WebauthnRequestCallback getRequestCallback();
}
