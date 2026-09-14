// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.build.annotations.NullMarked;

/**
 * Lists //components/webauthn features that can be accessed through {@link WebauthnFeatureMap}.
 *
 * <p>Note: Features must be added to the array |kFeaturesExposedToJava| in
 * //components/webauthn/android/webauthn_feature_map.cc.
 */
@NullMarked
public abstract class WebauthnFeatures {
    public static final String WEBAUTHN_ANDROID_CRED_MAN_FOR_DEV = "WebAuthnAndroidCredManForDev";
    // This is a temporary workaround for crbug.com/555599813.
    public static final String WEBAUTHN_FILTER_SMART_CARD_TRANSPORT =
            "WebAuthnFilterSmartCardTransport";
}
