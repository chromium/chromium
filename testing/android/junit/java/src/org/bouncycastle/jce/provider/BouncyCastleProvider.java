// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.bouncycastle.jce.provider;

import org.chromium.build.annotations.NullMarked;

import java.security.Provider;

/**
 * A lightweight stub of BouncyCastleProvider for Robolectric tests.
 *
 * <p>Upstream Robolectric's AndroidTestEnvironment eagerly instantiates
 * org.bouncycastle.jce.provider.BouncyCastleProvider and registers it into java.security.Security.
 * The real BouncyCastleProvider registers thousands of cryptographic algorithms and service
 * mappings, adding ~250ms of CPU overhead to test runner startup. Chromium unit tests do not rely
 * on Bouncy Castle algorithms. Providing this stub avoids that initialization overhead.
 */
@NullMarked
public final class BouncyCastleProvider extends Provider {
    public static final String PROVIDER_NAME = "BC";
    private static final String INFO = "BouncyCastle Security Provider (Chromium Stub)";

    @SuppressWarnings("deprecation")
    public BouncyCastleProvider() {
        super(PROVIDER_NAME, /* version= */ 1.80d, INFO);
    }
}
