// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.androidoverlay;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.androidoverlay.DialogOverlayImplTestRule.Client;
import org.chromium.content_public.browser.Visibility;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.base.WindowAndroid;

/** Tests for DialogOverlayImpl. */
@RunWith(BaseJUnit4ClassRunner.class)
@DisabledTest(message = "https://crbug.com/1462304")
public class DialogOverlayImplTest {
    private static final String BLANK_URL = "about:blank";

    @Rule
    public DialogOverlayImplTestRule mActivityTestRule = new DialogOverlayImplTestRule(BLANK_URL);

    @Test
    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testCreateDestroyOverlay() {
        Assert.assertFalse(mActivityTestRule.getClient().hasReceivedOverlayModeChange());
        Assert.assertFalse(mActivityTestRule.getClient().isUsingOverlayMode());

        final DialogOverlayImpl overlay = mActivityTestRule.createOverlay(0, 0, 10, 10);

        // We should get a new overlay with a valid surface key.
        Client.Event event = mActivityTestRule.getClient().nextEvent();
        Assert.assertEquals(Client.SURFACE_READY, event.which);
        Assert.assertTrue(event.surfaceKey > 0);

        Assert.assertTrue(mActivityTestRule.getClient().hasReceivedOverlayModeChange());
        Assert.assertTrue(mActivityTestRule.getClient().isUsingOverlayMode());

        // Close the overlay, and make sure that the provider is notified.
        // Note that we should not get a 'destroyed' message when we close it.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    overlay.close();
                });
        Assert.assertEquals(Client.RELEASED, mActivityTestRule.getClient().nextEvent().which);
        Assert.assertFalse(mActivityTestRule.getClient().isUsingOverlayMode());
    }

    @Test
    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testCreateOverlayFailsIfUnknownRoutingToken() {
        // Try to create an overlay with a bad routing token.
        mActivityTestRule.incrementUnguessableTokenHigh();
        DialogOverlayImpl overlay = mActivityTestRule.createOverlay(0, 0, 10, 10);
        Assert.assertNotNull(overlay);

        // We should be notified that the overlay is destroyed.
        Client.Event event = mActivityTestRule.getClient().nextEvent();
        Assert.assertEquals(Client.DESTROYED, event.which);
    }

    @Test
    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testCreateOverlayFailsIfWebContentsHidden() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getWebContents()
                            .updateWebContentsVisibility(Visibility.HIDDEN);
                });

        DialogOverlayImpl overlay = mActivityTestRule.createOverlay(0, 0, 10, 10);
        Assert.assertNotNull(overlay);

        // We should be notified that the overlay is destroyed.
        Client.Event event = mActivityTestRule.getClient().nextEvent();
        Assert.assertEquals(Client.DESTROYED, event.which);
    }

    @Test
    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testHiddingWebContentsDestroysOverlay() {
        DialogOverlayImpl overlay = mActivityTestRule.createOverlay(0, 0, 10, 10);
        Assert.assertNotNull(overlay);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getWebContents()
                            .updateWebContentsVisibility(Visibility.HIDDEN);
                });

        // We should be notified that the overlay is destroyed.
        Client.Event event = mActivityTestRule.getClient().nextEvent();
        Assert.assertEquals(Client.DESTROYED, event.which);
    }

    @Test
    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testScheduleLayoutDoesntCrash() {
        // Make sure that we don't get any messages due to scheduleLayout, and we don't crash.
        final DialogOverlayImpl overlay = mActivityTestRule.createOverlay(0, 0, 10, 10);

        // Wait for the surface.
        Assert.assertEquals(Client.SURFACE_READY, mActivityTestRule.getClient().nextEvent().which);
        final org.chromium.gfx.mojom.Rect rect = new org.chromium.gfx.mojom.Rect();
        rect.x = 100;
        rect.y = 200;
        rect.width = 100;
        rect.height = 100;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    overlay.scheduleLayout(rect);
                });

        // No additional messages should have arrived.
        Assert.assertTrue(mActivityTestRule.getClient().isEmpty());
    }

    @Test
    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testCreateSecureSurface() {
        // Test that creating a secure overlay creates an overlay.  We can't really tell if it's
        // secure or not, until we can do a screen shot test.
        mActivityTestRule.setSecure(true);
        final DialogOverlayImpl overlay = mActivityTestRule.createOverlay(0, 0, 10, 10);
        Assert.assertNotNull(overlay);

        // We should get a new overlay with a valid surface key.
        Client.Event event = mActivityTestRule.getClient().nextEvent();
        Assert.assertEquals(Client.SURFACE_READY, event.which);
        Assert.assertTrue(event.surfaceKey > 0);
    }

    @Test
    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testCloseOnlyClosesOnce() {
        // Test that trying to close an overlay more than once doesn't actually do anything.
        final DialogOverlayImpl overlay = mActivityTestRule.createOverlay(0, 0, 10, 10);
        // The first should generate RELEASED
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    overlay.close();
                });
        Assert.assertEquals(Client.RELEASED, mActivityTestRule.getClient().nextEvent().which);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    overlay.close();
                    mActivityTestRule.getClient().injectMarkerEvent();
                });
        Assert.assertEquals(Client.TEST_MARKER, mActivityTestRule.getClient().nextEvent().which);
    }

    @Test
    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testEmptyWindowAndroidDoesntCrash() {
        // Test that receiving a WindowAndroid that doesn't have activity donesn't cause crash.
        ImmutableWeakReference<Context> nullContextWeakRef = new ImmutableWeakReference<>(null);
        WindowAndroid mockWindowAndroid = mock(WindowAndroid.class);
        when(mockWindowAndroid.getContext()).thenReturn(nullContextWeakRef);
        final DialogOverlayImpl overlay = mActivityTestRule.createOverlay(0, 0, 10, 10);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    overlay.onWindowAndroid(mockWindowAndroid);
                });
    }
}
