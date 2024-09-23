// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;

public interface AuthenticationContextProvider {
    Context getContext();

    @Nullable
    RenderFrameHost getRenderFrameHost();

    FidoIntentSender getIntentSender();

    WebContents getWebContents();
}
