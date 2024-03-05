// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {})
public class CredManUiRecommenderProviderRobolectricTest {
    @Mock Supplier<CredManUiRecommender> mSupplier;
    @Mock CredManUiRecommender mRecommender;

    CredManUiRecommenderProvider mProvider;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        when(mSupplier.get()).thenReturn(mRecommender);
        mProvider = CredManUiRecommenderProvider.getOrCreate();
    }

    @After
    public void tearDown() {
        CredManUiRecommenderProvider.resetInstanceForTesting();
    }

    @Test
    @SmallTest
    public void testGetOrCreate() {
        CredManUiRecommenderProvider provider2 = CredManUiRecommenderProvider.getOrCreate();

        assertThat(provider2).isEqualTo(mProvider);
    }

    @Test
    @SmallTest
    public void testNullSupplier() {
        assertThat(mProvider.getCredManUiRecommender()).isNull();
    }

    @Test
    @SmallTest
    public void testGetCredManUiRecommender() {
        mProvider.setCredManUiRecommenderSupplier(mSupplier);

        assertThat(mProvider.getCredManUiRecommender()).isEqualTo(mRecommender);
    }
}
