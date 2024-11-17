// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.androidoverlay;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.app.Dialog;
import android.os.Binder;
import android.os.IBinder;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.WindowManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowDialog;
import org.robolectric.shadows.ShadowPhoneWindow;
import org.robolectric.shadows.ShadowSurfaceView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.gfx.mojom.Rect;
import org.chromium.media.mojom.AndroidOverlayConfig;

/** Tests for DialogOverlayCore. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DialogOverlayCoreTest {
    private Activity mActivity;

    AndroidOverlayConfig mConfig = new AndroidOverlayConfig();

    // Should we request a panel?
    boolean mAsPanel;

    // DialogCore under test.
    DialogOverlayCore mCore;

    // The dialog that we've provided to |mCore|.
    Dialog mDialog;

    // Fake window token that we'll send to |mCore|.
    IBinder mWindowToken = new Binder();

    // Surface that will be provided by |mDialog|.
    Surface mSurface = new Surface();

    // SurfaceHolder that will be provided by |mDialog|.
    SurfaceHolder mHolder = new MyFakeSurfaceHolder(mSurface);

    /** Robolectric shadow for PhoneWindow. This one keeps track of takeSurface() calls. */
    @Implements(className = "com.android.internal.policy.PhoneWindow", isInAndroidSdk = false)
    public static class MyPhoneWindowShadow extends ShadowPhoneWindow {
        public MyPhoneWindowShadow() {}

        private SurfaceHolder.Callback2 mCallback;
        private WindowManager.LayoutParams mLayoutParams;
        public boolean mDidUpdateParams;

        @Implementation
        public void takeSurface(SurfaceHolder.Callback2 callback) {
            mCallback = callback;
        }

        @Implementation
        public void setAttributes(WindowManager.LayoutParams layoutParams) {
            mLayoutParams = layoutParams;
            mDidUpdateParams = true;
        }
    }

    /** The default fake surface holder doesn't let us provide a surface. */
    public static class MyFakeSurfaceHolder extends ShadowSurfaceView.FakeSurfaceHolder {
        private Surface mSurface;

        // @param surface The Surface that we'll provide via getSurface.
        public MyFakeSurfaceHolder(Surface surface) {
            mSurface = surface;
        }

        @Override
        public Surface getSurface() {
            return mSurface;
        }
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();

        mConfig = new AndroidOverlayConfig();
        mConfig.rect = new Rect();
        mConfig.rect.x = 0;
        mConfig.rect.y = 1;
        mConfig.rect.width = 2;
        mConfig.rect.height = 3;
    }

    public void createOverlay() {
        mCore = new DialogOverlayCore();
        mCore.initialize(mActivity, mConfig, mHost, mAsPanel);
        mDialog = mCore.getDialog();

        // Nothing should be called yet.
        checkOverlayDidntCall();

        // The dialog should not be shown yet.
        checkDialogIsNotShown();
    }

    // Make sure that the overlay didn't provide us with a surface, or notify us that it was
    // destroyed, or wait for cleanup.
    void checkOverlayDidntCall() {
        assertEquals(null, mHost.surface());
        assertEquals(0, mHost.destroyedCount());
    }

    // Return the SurfaceHolder callback that was provided to takeSurface(), if any.
    SurfaceHolder.Callback2 holderCallback() {
        return ((MyPhoneWindowShadow) Shadows.shadowOf(mDialog.getWindow())).mCallback;
    }

    // Return the LayoutPararms that was most recently provided to the dialog.
    WindowManager.LayoutParams layoutParams() {
        return ((MyPhoneWindowShadow) Shadows.shadowOf(mDialog.getWindow())).mLayoutParams;
    }

    MyPhoneWindowShadow getShadowWindow() {
        return ((MyPhoneWindowShadow) Shadows.shadowOf(mDialog.getWindow()));
    }

    /** Host impl that counts calls to it. */
    static class HostMock implements DialogOverlayCore.Host {
        private Surface mSurface;
        private int mDestroyedCount;

        @Override
        public void onSurfaceReady(Surface surface) {
            mSurface = surface;
        }

        @Override
        public void onOverlayDestroyed() {
            mDestroyedCount++;
        }

        public Surface surface() {
            return mSurface;
        }

        public int destroyedCount() {
            return mDestroyedCount;
        }
    }
    ;

    HostMock mHost = new HostMock();

    // Send a window token and provide the surface, so that the overlay is ready for use.
    void sendTokenAndSurface() {
        mCore.onWindowToken(mWindowToken);
        // Make sure that somebody called takeSurface.
        assertNotNull(holderCallback());

        checkDialogIsShown();

        // Provide the Android Surface.
        holderCallback().surfaceCreated(mHolder);

        // The host should have been told about the surface.
        assertEquals(mSurface, mHost.surface());
    }

    // Verify that the dialog has been shown.
    void checkDialogIsShown() {
        assertEquals(mDialog, ShadowDialog.getShownDialogs().get(0));
    }

    // Verify that the dialog is not currently shown.  Note that dismiss() doesn't remove it from
    // the shown dialog list in Robolectric, so we check for "was never shown or was dismissed".
    void checkDialogIsNotShown() {
        assertTrue(
                ShadowDialog.getShownDialogs().size() == 0
                        || Shadows.shadowOf(mDialog).hasBeenDismissed());
    }

    // Verify that |mCore| signaled that the overlay was lost to|mHost|.
    void checkOverlayWasDestroyed() {
        // |mCore| should have notified the host that it has been destroyed, and also waited for
        // the host to signal that the client released it.
        assertEquals(1, mHost.destroyedCount());
        checkDialogIsNotShown();
    }

    // Check that releasing an overlay before getting a window token works.
    @Test
    @Config(shadows = {MyPhoneWindowShadow.class})
    public void testReleaseImmediately() {
        // Release the overlay.  |mCore| shouldn't notify us, since we released it.
        createOverlay();
        mCore.release();
        checkOverlayDidntCall();
        checkDialogIsNotShown();
    }

    // Create a dialog, then send it a token.  Verify that it's shown.
    @Test
    @Config(shadows = {MyPhoneWindowShadow.class})
    public void testTokenThenRelease() {
        createOverlay();
        mCore.onWindowToken(mWindowToken);
        checkDialogIsShown();

        // Release the surface.  |mHost| shouldn't be notified, nor should it wait for cleanup.
        // Note: it might be okay if it checks for cleanup, since cleanup would be complete after
        // we call release().  However, it's not needed, so we enforce that it isn't.
        mCore.release();
        checkOverlayDidntCall();
        checkDialogIsNotShown();
    }

    // Create a dialog, send a token, send a surface, then release it.
    @Test
    @Config(shadows = {MyPhoneWindowShadow.class})
    public void testSurfaceThenRelease() {
        createOverlay();
        sendTokenAndSurface();

        mCore.release();
        assertEquals(0, mHost.destroyedCount());
        checkDialogIsNotShown();
    }

    // Create a dialog, send a surface, then destroy the surface.
    @Test
    @Config(shadows = {MyPhoneWindowShadow.class})
    public void testSurfaceThenDestroy() {
        createOverlay();
        sendTokenAndSurface();

        // Destroy the surface.
        holderCallback().surfaceDestroyed(mHolder);
        // |mCore| should have waited for cleanup during surfaceDestroyed.
        mCore.release();

        checkOverlayWasDestroyed();
    }

    // Test that we're notified when the window token changes.
    @Test
    @Config(shadows = {MyPhoneWindowShadow.class})
    public void testChangeWindowToken() {
        createOverlay();
        sendTokenAndSurface();

        // Change the window token.
        mCore.onWindowToken(new Binder());

        checkOverlayWasDestroyed();
    }

    // Test that we're notified when the window token is lost.
    @Test
    @Config(shadows = {MyPhoneWindowShadow.class})
    public void testLoseWindowToken() {
        createOverlay();
        sendTokenAndSurface();

        // Remove the window token.
        mCore.onWindowToken(null);

        checkOverlayWasDestroyed();
    }

    // Test that the layout params reflect TYPE_APPLICATION_MEDIA, and that it its geometry matches
    // what we requested.
    @Test
    @Config(shadows = {MyPhoneWindowShadow.class})
    public void testOverlayTypeAndGeometry() {
        createOverlay();
        mCore.onWindowToken(mWindowToken);
        assertEquals(WindowManager.LayoutParams.TYPE_APPLICATION_MEDIA, layoutParams().type);
        assertEquals(mConfig.rect.x, layoutParams().x);
        assertEquals(mConfig.rect.y, layoutParams().y);
        assertEquals(mConfig.rect.width, layoutParams().width);
        assertEquals(mConfig.rect.height, layoutParams().height);
    }

    // Test that the layout params reflect TYPE_APPLICATION_PANEL when we request it.
    @Test
    @Config(shadows = {MyPhoneWindowShadow.class})
    public void testOverlayAsPanel() {
        mAsPanel = true;
        createOverlay();
        mCore.onWindowToken(mWindowToken);
        assertEquals(layoutParams().type, WindowManager.LayoutParams.TYPE_APPLICATION_PANEL);
    }

    @Test
    @Config(shadows = {MyPhoneWindowShadow.class})
    public void testNoParamsUpdateForSamePositionRect() {
        createOverlay();
        mCore.onWindowToken(mWindowToken);
        assertTrue(getShadowWindow().mDidUpdateParams);

        // Update with the same rect, it should not update the window params.
        getShadowWindow().mDidUpdateParams = false;
        mCore.layoutSurface(mConfig.rect);
        assertFalse(getShadowWindow().mDidUpdateParams);
    }
}
