// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebAuthenticationDelegate;
import org.chromium.url.Origin;

/**
 * Android implementation of the Authenticator service defined in
 * //third_party/blink/public/mojom/webauth/authenticator.mojom.
 */
public class Fido2ApiHandler {
    private static final String GMSCORE_PACKAGE_NAME = "com.google.android.gms";
    public static final int GMSCORE_MIN_VERSION = 16890000;

}
