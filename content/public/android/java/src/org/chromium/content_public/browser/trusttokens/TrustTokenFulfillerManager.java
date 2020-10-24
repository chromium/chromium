// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.content_public.browser.trusttokens;

import org.chromium.content.mojom.LocalTrustTokenFulfiller;

/**
 * TrustTokenFulfillerManager is a static utility class that allows embedders to plug in
 * implementations of LocalTrustTokenFulfiller to accommodate the fact that they might
 * use different mechanisms for executing Trust Tokens operations locally.
 */
public class TrustTokenFulfillerManager {
    /**
     * Clients provide their implementations of LocalTrustTokenFulfiller by subclassing Factory.
     */
    public interface Factory {
        LocalTrustTokenFulfiller create();
    }

    /**
     * Returns a Trust Tokens operation fulfiller, or null if:
     * <ul>
     *   <li> the embedder hasn't provided a way of constructing a fulfiller, or
     *   <li> the embedder indicated some kind of failure when constructing the fulfiller.
     */
    public static LocalTrustTokenFulfiller create() {
        if (sFactory == null) return null;
        return sFactory.create();
    }

    private static Factory sFactory;

    public static void setFactory(Factory factory) {
        sFactory = factory;
    }
}
