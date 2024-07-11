// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.content.browser.androidoverlay;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.view.Surface;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.androidoverlay.DialogOverlayImplTestRule.Client;

import java.util.concurrent.Callable;

/** Pixel tests for DialogOverlayImpl. These use UiAutomation, so they only run in JB or above. */
@RunWith(BaseJUnit4ClassRunner.class)
@DisabledTest(message = "https://crbug.com/1462304")
public class DialogOverlayImplPixelTest {
    // Color that we'll fill the overlay with.

    @Rule
    public DialogOverlayImplTestRule mActivityTestRule =
            new DialogOverlayImplTestRule(TEST_PAGE_DATA_URL);

    private static final int OVERLAY_FILL_COLOR = Color.BLUE;

    // CSS coordinates of a div that we'll try to cover with an overlay.
    private static final int DIV_X_CSS = 10;
    private static final int DIV_Y_CSS = 20;
    private static final int DIV_WIDTH_CSS = 300;
    private static final int DIV_HEIGHT_CSS = 200;

    // Provide a solid-color div that's positioned / sized by DIV_*_CSS.
    private static final String TEST_PAGE_STYLE =
            "<style>"
                    + "div {"
                    + "left: "
                    + DIV_X_CSS
                    + "px;"
                    + "top: "
                    + DIV_Y_CSS
                    + "px;"
                    + "width: "
                    + DIV_WIDTH_CSS
                    + "px;"
                    + "height: "
                    + DIV_HEIGHT_CSS
                    + "px;"
                    + "position: absolute;"
                    + "background: red;"
                    + "}"
                    + "</style>";
    private static final String TEST_PAGE_DATA_URL =
            UrlUtils.encodeHtmlDataUri(
                    "<html>" + TEST_PAGE_STYLE + "<body><div></div></body></html>");

    // Number of retries for various race-prone operations.
    private static final int NUM_RETRIES = 10;

    // Delay (msec) between retries.
    private static final int RETRY_DELAY = 50;

    // Number of rows and columns that we consider as optional due to rounding and blending diffs.
    private static final int FUZZY_PIXELS = 1;

    // DIV_*_CSS converted to screen pixels.
    int mDivXPx;
    int mDivYPx;
    int mDivWidthPx;
    int mDivHeightPx;

    // Target area boundaries.
    // We allow a range because of page / device scaling.  The size of the div and the size of the
    // area of overlap can be off by a pixel in either direction.  The div can be blended around the
    // edge, while the overlay position can round differently.
    int mTargetAreaMinPx;
    int mTargetAreaMaxPx;

    // Maximum status bar height that we'll work with.  This just lets us restrict the area of the
    // screenshot that we inspect, since it's slow.  This should also include the URL bar.
    private static final int mStatusBarMaxHeightPx = 300;

    // Area of interest that contains the div, since the whole image is big.
    Rect mAreaOfInterestPx;

    // Screenshot of the test page, before we do anything.
    Bitmap mInitialScreenshot;

    RenderCoordinatesImpl mCoordinates;

    @Before
    public void setUp() {
        takeScreenshotOfBackground();
        mCoordinates = mActivityTestRule.getRenderCoordinates();
    }

    // Take a screenshot via UiAutomation, which captures all overlays.
    Bitmap takeScreenshot() {
        return InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot();
    }

    // Fill |surface| with OVERLAY_FILL_COLOR and return a screenshot.  Note that we have no idea
    // how long it takes before the image posts, so the screenshot might not reflect it.  Be
    // prepared to retry.  Note that we always draw the same thing, so it's okay if a retry gets a
    // screenshot of a previous surface; they're identical.
    Bitmap fillSurface(Surface surface) {
        Canvas canvas = surface.lockCanvas(null);
        canvas.drawColor(OVERLAY_FILL_COLOR);
        surface.unlockCanvasAndPost(canvas);
        return takeScreenshot();
    }

    int convertCSSToScreenPixels(int css) {
        return (int)
                (css * mCoordinates.getPageScaleFactor() * mCoordinates.getDeviceScaleFactor());
    }

    // Since ContentShell makes our solid color div have some textured background, we have to be
    // somewhat lenient here.  Plus, sometimes the edges of the div are blended.
    boolean isApproximatelyRed(int color) {
        int r = Color.red(color);
        return r > 100 && Color.green(color) < r && Color.blue(color) < r;
    }

    // Take a screenshot, and wait until we get one that has the background div in it.
    void takeScreenshotOfBackground() {
        mAreaOfInterestPx = new Rect();
        for (int retries = 0; retries < NUM_RETRIES; retries++) {
            // Compute the div position in screen pixels.  We recompute these since they sometimes
            // take a while to settle, also.
            mDivXPx = convertCSSToScreenPixels(DIV_X_CSS);
            mDivYPx = convertCSSToScreenPixels(DIV_Y_CSS);
            mDivWidthPx = convertCSSToScreenPixels(DIV_WIDTH_CSS);
            mDivHeightPx = convertCSSToScreenPixels(DIV_HEIGHT_CSS);

            // Allow one edge on each side to be non-overlapping or misdetected.
            mTargetAreaMaxPx = mDivWidthPx * mDivHeightPx;
            mTargetAreaMinPx = (mDivWidthPx - FUZZY_PIXELS) * (mDivHeightPx - FUZZY_PIXELS);

            // Don't read the whole bitmap.  It's quite big.  Assume that the status bar is only at
            // the top, and that it's at most mStatusBarMaxHeightPx px tall.  We also allow a bit of
            // room on each side for rounding issues.  Setting these too large just slows down the
            // test, without affecting the result.
            mAreaOfInterestPx.left = mDivXPx - FUZZY_PIXELS;
            mAreaOfInterestPx.top = mDivYPx - FUZZY_PIXELS;
            mAreaOfInterestPx.right = mDivXPx + mDivWidthPx - 1 + FUZZY_PIXELS;
            mAreaOfInterestPx.bottom = mDivYPx + mDivHeightPx + mStatusBarMaxHeightPx;

            mInitialScreenshot = takeScreenshot();

            int area = 0;
            for (int ry = mAreaOfInterestPx.top; ry <= mAreaOfInterestPx.bottom; ry++) {
                for (int rx = mAreaOfInterestPx.left; rx <= mAreaOfInterestPx.right; rx++) {
                    if (isApproximatelyRed(mInitialScreenshot.getPixel(rx, ry))) area++;
                }
            }

            // It's okay if we have some randomly colored other pixels.
            if (area >= mTargetAreaMinPx) return;

            try {
                Thread.sleep(RETRY_DELAY);
            } catch (Exception e) {
            }
        }

        Assert.assertTrue(false);
    }

    // Count how many pixels in the div are covered by OVERLAY_FILL_COLOR in |overlayScreenshot|,
    // and return it.
    int countDivPixelsCoveredByOverlay(Bitmap overlayScreenshot) {
        // Find pixels that changed from the source color to the target color.  This should avoid
        // issues like changes in the status bar, unless we're really unlucky.  It assumes that the
        // div is actually the expected size; coloring the entire page red would fool this.
        int area = 0;
        for (int ry = mAreaOfInterestPx.top; ry <= mAreaOfInterestPx.bottom; ry++) {
            for (int rx = mAreaOfInterestPx.left; rx <= mAreaOfInterestPx.right; rx++) {
                if (isApproximatelyRed(mInitialScreenshot.getPixel(rx, ry))
                        && overlayScreenshot.getPixel(rx, ry) == OVERLAY_FILL_COLOR) {
                    area++;
                }
            }
        }

        return area;
    }

    // Assert that |surface| exactly covers the target div on the page.  Note that we assume that
    // you have not drawn anything to |surface| yet, so that we can still see the div.
    void assertDivIsExactlyCovered(Surface surface) {
        // Draw two colors, and count as the area the ones that change between screenshots.  This
        // lets us notice if the status bar is occluding something, even if it happens to be the
        // same color.
        int area = 0;
        int targetArea = mDivWidthPx * mDivHeightPx;
        for (int retries = 0; retries < NUM_RETRIES; retries++) {
            // We fill the overlay every time, in case a resize was pending.  Eventually, we should
            // reach a steady-state where the surface is resized, and this (or a previous) filled-in
            // surface is on the screen.
            Bitmap overlayScreenshot = fillSurface(surface);
            area = countDivPixelsCoveredByOverlay(overlayScreenshot);
            if (area >= mTargetAreaMinPx && area <= mTargetAreaMaxPx) return;

            // There are several reasons this can fail besides being broken.  We don't know how long
            // it takes for fillSurface()'s output to make it to the display.  We also don't know
            // how long scheduleLayout() takes.  Just try a few times, since the whole thing should
            // take only a frame or two to settle.
            try {
                Thread.sleep(RETRY_DELAY);
            } catch (Exception e) {
            }
        }

        // Assert so that we get a helpful message in the log.
        Assert.assertEquals(targetArea, area);
    }

    // Wait for |overlay| to become ready, get its surface, and return it.
    Surface waitForSurface(DialogOverlayImpl overlay) throws Exception {
        Assert.assertNotNull(overlay);
        final Client.Event event = mActivityTestRule.getClient().nextEvent();
        Assert.assertTrue(event.surfaceKey > 0);
        return ThreadUtils.runOnUiThreadBlocking(
                new Callable<Surface>() {
                    @Override
                    public Surface call() {
                        return DialogOverlayImplJni.get()
                                .lookupSurfaceForTesting((int) event.surfaceKey);
                    }
                });
    }

    @Test
    @MediumTest
    @Feature({"AndroidOverlay"})
    public void testInitialPosition() throws Exception {
        // Test that the initial position supplied for the overlay covers the <div> we created.
        final DialogOverlayImpl overlay =
                mActivityTestRule.createOverlay(mDivXPx, mDivYPx, mDivWidthPx, mDivHeightPx);
        Surface surface = waitForSurface(overlay);

        assertDivIsExactlyCovered(surface);
    }

    @Test
    @MediumTest
    @Feature({"AndroidOverlay"})
    public void testScheduleLayout() throws Exception {
        // Test that scheduleLayout() moves the overlay to cover the <div>.
        final DialogOverlayImpl overlay = mActivityTestRule.createOverlay(0, 0, 10, 10);
        Surface surface = waitForSurface(overlay);

        final org.chromium.gfx.mojom.Rect rect = new org.chromium.gfx.mojom.Rect();
        rect.x = mDivXPx;
        rect.y = mDivYPx;
        rect.width = mDivWidthPx;
        rect.height = mDivHeightPx;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    overlay.scheduleLayout(rect);
                });

        assertDivIsExactlyCovered(surface);
    }
}
