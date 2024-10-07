// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.webauthn.cred_man;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;

/**
 * This class is responsible for providing the correct {@link CredManUiRecommender}. It can return a
 * null instance, for example for WebView.
 *
 * <p>Set the correct instance using
 *
 * <pre>{@code
 * CredManUiRecommenderProvider.getOrCreate()
 *     .setCredManUiRecommenderSupplier(() -> new CredManUiRecommender() { ... });
 * }</pre>
 */
public class CredManUiRecommenderProvider {
    private static CredManUiRecommenderProvider sInstance;

    private Supplier<CredManUiRecommender> mSupplier;

    @Nullable
    public static CredManUiRecommenderProvider getOrCreate() {
        if (sInstance == null) {
            sInstance = new CredManUiRecommenderProvider();
        }
        return sInstance;
    }

    /**
     * Sets the {@link CredManUiRecommender} supplier.
     *
     * <p>The supplier is used to get the instance of the {@link CredManUiRecommender} to be used.
     *
     * @param supplier the {@link CredManUiRecommender} supplier.
     */
    public void setCredManUiRecommenderSupplier(Supplier<CredManUiRecommender> supplier) {
        mSupplier = supplier;
    }

    @Nullable
    public CredManUiRecommender getCredManUiRecommender() {
        return mSupplier == null ? null : mSupplier.get();
    }

    static void resetInstanceForTesting() {
        sInstance = null;
    }

    private CredManUiRecommenderProvider() {}
}
