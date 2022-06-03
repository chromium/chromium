// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.InputMethodManagerWrapper;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/**
 * A robolectric test for {@link InputMethodManagerWrapperImpl} class.
 */
@RunWith(BaseRobolectricTestRunner.class)
// Any VERSION_CODE >= O is fine.
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.O)
public class InputMethodManagerWrapperImplTest {
    private static final boolean DEBUG = false;

    private class TestInputMethodManagerWrapperImpl extends InputMethodManagerWrapperImpl {
        public TestInputMethodManagerWrapperImpl(
                Context context, WindowAndroid windowAndroid, Delegate delegate) {
            super(context, windowAndroid, delegate);
        }

        @Override
        protected int getDisplayId(Context context) {
            if (context == mContext) {
                assert mContextDisplayId != -1;
                return mContextDisplayId;
            }
            if (context == mActivity) {
                assert mActivityDisplayId != -1;
                return mActivityDisplayId;
            }
            return super.getDisplayId(context);
        }
    }

    @Mock
    private Context mContext;
    @Mock
    private Activity mActivity;
    @Mock
    private Window mWindow;
    @Mock
    private WindowAndroid mWindowAndroid;
    @Mock
    private InputMethodManagerWrapper.Delegate mDelegate;
    @Mock
    private View mView;
    @Mock
    private InputMethodManager mInputMethodManager;
    @Mock
    private WindowManager mContextWindowManager;
    @Mock
    private WindowManager mActivityWindowManager;

    private int mContextDisplayId = -1; // uninitialized
    private int mActivityDisplayId = -1; // uninitialized

    private InOrder mInOrder;

    private InputMethodManagerWrapperImpl mImmw;

    public InputMethodManagerWrapperImplTest() {
        if (DEBUG) ShadowLog.stream = System.out;
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mImmw = new TestInputMethodManagerWrapperImpl(mContext, mWindowAndroid, mDelegate);
        when(mContext.getSystemService(Context.INPUT_METHOD_SERVICE))
                .thenReturn(mInputMethodManager);
        when(mActivity.getSystemService(Context.INPUT_METHOD_SERVICE))
                .thenReturn(mInputMethodManager);
        when(mActivity.getWindow()).thenReturn(mWindow);

        mInOrder = inOrder(mInputMethodManager, mWindow);
    }

    @After
    public void tearDown() throws Exception {
        mInOrder.verifyNoMoreInteractions();
    }

    @Test
    public void testWebViewHasNoActivity() throws Exception {
        when(mWindowAndroid.getActivity()).thenReturn(null);

        mImmw.showSoftInput(mView, 0, null);

        mInOrder.verify(mInputMethodManager).showSoftInput(mView, 0, null);
    }

    private void setDisplayIds(int contextDisplayId, int activityDisplayId) {
        mContextDisplayId = contextDisplayId;
        mActivityDisplayId = activityDisplayId;
    }

    @Test
    public void testSingleDisplay() throws Exception {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<Activity>(mActivity));
        setDisplayIds(0, 0);

        mImmw.showSoftInput(mView, 0, null);

        mInOrder.verify(mInputMethodManager).showSoftInput(mView, 0, null);
    }

    @Test
    public void testMultiDisplaysWithInputConnection() throws Exception {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<Activity>(mActivity));
        setDisplayIds(0, 1); // context and activity have different display IDs
        when(mDelegate.hasInputConnection()).thenReturn(true);

        mImmw.showSoftInput(mView, 0, null);

        // Run a workaround.
        mInOrder.verify(mWindow).setLocalFocus(true, true);

        // When InputConnection is available, then show soft input immediately.
        mInOrder.verify(mInputMethodManager).showSoftInput(mView, 0, null);
    }

    @Test
    public void testMultiDisplaysWithoutInputConnection() throws Exception {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<Activity>(mActivity));
        setDisplayIds(0, 1); // context and activity have different display Ids
        when(mDelegate.hasInputConnection()).thenReturn(false);
        when(mInputMethodManager.isActive(mView)).thenReturn(true);

        mImmw.showSoftInput(mView, 0, null);

        // Run a workaround.
        mInOrder.verify(mWindow).setLocalFocus(true, true);

        // InputConnection is not available, then wait for onInputConnectionCreated().
        mInOrder.verifyNoMoreInteractions();

        mImmw.onInputConnectionCreated();

        // Post task: note that PostTask actually does not require
        // Robolectric.getForegroundThreadScheduler().runOneTask() to be called.

        // Check first if input method is still valid on the current view.
        mInOrder.verify(mInputMethodManager).isActive(mView);

        mInOrder.verify(mInputMethodManager).showSoftInput(mView, 0, null);
    }

    @Test
    public void testMultiDisplaysWithoutInputConnection_hideKeyboard() throws Exception {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<Activity>(mActivity));
        setDisplayIds(0, 1); // context and activity have different display Ids
        when(mDelegate.hasInputConnection()).thenReturn(false);
        when(mInputMethodManager.isActive(mView)).thenReturn(true);

        mImmw.showSoftInput(mView, 0, null);

        // Run a workaround.
        mInOrder.verify(mWindow).setLocalFocus(true, true);

        // InputConnection is not available, then wait for onInputConnectionCreated().
        mInOrder.verifyNoMoreInteractions();

        // Hide called before input connection is created.
        mImmw.hideSoftInputFromWindow(null, 0, null);

        mImmw.onInputConnectionCreated();

        mInOrder.verify(mInputMethodManager).hideSoftInputFromWindow(null, 0, null);
        // Do not call showSoftInput.
    }

    @Test
    public void testMultiDisplaysWithoutInputConnection_notActive() throws Exception {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<Activity>(mActivity));
        setDisplayIds(0, 1); // context and activity have different display Ids
        when(mDelegate.hasInputConnection()).thenReturn(false);
        when(mInputMethodManager.isActive(mView)).thenReturn(false);

        mImmw.showSoftInput(mView, 0, null);

        // Run a workaround.
        mInOrder.verify(mWindow).setLocalFocus(true, true);

        // InputConnection is not available, then wait for onInputConnectionCreated().
        mInOrder.verifyNoMoreInteractions();

        // Another showSoftInput before input connection gets created.
        mImmw.showSoftInput(mView, 1, null);

        mImmw.onInputConnectionCreated();

        // Post task: note that PostTask actually does not require
        // Robolectric.getForegroundThreadScheduler().runOneTask() to be called.

        // Check first if input method is still valid on the current view.
        mInOrder.verify(mInputMethodManager).isActive(mView);

        // Do not call showSoftInput since it is not active.
    }

    @Test
    public void testMultiDisplaysWithoutInputConnection_showSoftInputAgain() throws Exception {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<Activity>(mActivity));
        setDisplayIds(0, 1); // context and activity have different display Ids
        when(mDelegate.hasInputConnection()).thenReturn(false);
        when(mInputMethodManager.isActive(mView)).thenReturn(true);

        mImmw.showSoftInput(mView, 0, null);

        // Run a workaround.
        mInOrder.verify(mWindow).setLocalFocus(true, true);

        // InputConnection is not available, then wait for onInputConnectionCreated().
        mInOrder.verifyNoMoreInteractions();

        // Another showSoftInput before input connection gets created.
        mImmw.showSoftInput(mView, 1, null);

        mImmw.onInputConnectionCreated();

        // Post task: note that PostTask actually does not require
        // Robolectric.getForegroundThreadScheduler().runOneTask() to be called.

        // Check first if input method is still valid on the current view.
        mInOrder.verify(mInputMethodManager).isActive(mView);

        // Note that the first call to showSoftInput was ignored.
        mInOrder.verify(mInputMethodManager).showSoftInput(mView, 1, null);
    }
}
