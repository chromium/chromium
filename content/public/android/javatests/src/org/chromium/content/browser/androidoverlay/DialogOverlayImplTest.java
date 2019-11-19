// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.androidoverlay;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.androidoverlay.DialogOverlayImplTestRule.Client;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for DialogOverlayImpl.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class DialogOverlayImplTest {
    private static final String BLANK_URL = "about://blank";

    @Rule
    public DialogOverlayImplTestRule mActivityTestRule =
            new DialogOverlayImplTestRule(BLANK_URL);

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
        TestThreadUtils.runOnUiThreadBlocking(() -> { overlay.close(); });
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getWebContents().onHide(); });

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

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getWebContents().onHide(); });

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
        TestThreadUtils.runOnUiThreadBlocking(() -> { overlay.scheduleLayout(rect); });

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
        TestThreadUtils.runOnUiThreadBlocking(() -> { overlay.close(); });
        Assert.assertEquals(Client.RELEASED, mActivityTestRule.getClient().nextEvent().which);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            overlay.close();
            mActivityTestRule.getClient().injectMarkerEvent();
        });
        Assert.assertEquals(Client.TEST_MARKER, mActivityTestRule.getClient().nextEvent().which);
    }
}
