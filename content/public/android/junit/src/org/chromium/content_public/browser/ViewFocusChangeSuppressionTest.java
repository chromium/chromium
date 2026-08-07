// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link ViewFocusChangeSuppression}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ViewFocusChangeSuppressionTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContents mWebContents;
    private ViewFocusChangeSuppression mSuppression;

    @Before
    public void setUp() {
        Mockito.when(mWebContents.getOrSetUserData(Mockito.any(), Mockito.any()))
                .thenAnswer(
                        invocation -> {
                            if (mSuppression == null) {
                                WebContents.UserDataFactory<ViewFocusChangeSuppression> factory =
                                        invocation.getArgument(1);
                                mSuppression = factory.create(mWebContents);
                            }
                            return mSuppression;
                        });
    }

    @Test
    @SmallTest
    public void testFromReturnsNonNull() {
        ViewFocusChangeSuppression suppression = ViewFocusChangeSuppression.from(mWebContents);
        assertNotNull(suppression);
    }

    @Test
    @SmallTest
    public void testSameInstanceReturned() {
        ViewFocusChangeSuppression suppression1 = ViewFocusChangeSuppression.from(mWebContents);
        ViewFocusChangeSuppression suppression2 = ViewFocusChangeSuppression.from(mWebContents);
        assertEquals(suppression1, suppression2);
    }

    @Test
    @SmallTest
    public void testSetAndGetSuppressed() {
        ViewFocusChangeSuppression suppression = ViewFocusChangeSuppression.from(mWebContents);
        assertFalse(suppression.isSuppressed());

        suppression.setSuppressed(true);
        assertTrue(suppression.isSuppressed());

        suppression.setSuppressed(false);
        assertFalse(suppression.isSuppressed());
    }
}
