// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.os.Handler;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.InputMethodManagerWrapper;

/** Unit tests for {@link ThreadedInputConnectionFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ThreadedInputConnectionFactoryTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    /** A testable version of ThreadedInputConnectionFactory. */
    private class TestFactory extends ThreadedInputConnectionFactory {

        private boolean mSucceeded;
        private boolean mFailed;
        private long mDelayMs;

        TestFactory(InputMethodManagerWrapper inputMethodManagerWrapper) {
            super(inputMethodManagerWrapper);
        }

        @Override
        protected ThreadedInputConnectionProxyView createProxyView(
                Handler handler, View containerView) {
            mProxyView = super.createProxyView(handler, containerView);
            return mProxyView;
        }

        @Override
        protected void onRegisterProxyViewSuccess() {
            mSucceeded = true;
        }

        @Override
        protected void onRegisterProxyViewFailure() {
            mFailed = true;
        }

        public boolean hasFailed() {
            return mFailed;
        }

        public boolean hasSucceeded() {
            return mSucceeded;
        }

        public long delayMs() {
            return mDelayMs;
        }

        @Override
        public void onWindowFocusChanged(boolean gainFocus) {
            mHasWindowFocus = gainFocus;
            super.onWindowFocusChanged(gainFocus);
        }

        @Override
        protected void postDelayed(View view, Runnable r, long delayMs) {
            mDelayMs = delayMs;
            // Note that robolectric will run this immediately in runOneTask(). We can only test
            // the delay MS value.
            super.postDelayed(view, r, delayMs);
        }
    }

    /**
     * A container view that creates its InputConnection through the factory, as ContentView does.
     * The real ThreadedInputConnectionProxyView calls back into this method.
     */
    private class TestContainerView extends View {
        TestContainerView(Context context) {
            super(context);
        }

        @Override
        public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
            return mFactory.initializeAndGet(this, mImeAdapter, 1, 0, 0, 0, 0, 0, "", outAttrs);
        }
    }

    @Mock private ImeAdapterImpl mImeAdapter;
    @Mock private InputMethodManager mInputMethodManager;
    @Mock private Context mContext;

    private View mContainerView;
    private ThreadedInputConnectionProxyView mProxyView;
    private EditorInfo mEditorInfo;
    private Handler mImeHandler;
    private Handler mUiHandler;
    private ShadowLooper mImeShadowLooper;
    private TestFactory mFactory;
    private InputConnection mInputConnection;
    private InOrder mInOrder;
    private boolean mHasWindowFocus;

    @Before
    public void setUp() {

        mEditorInfo = new EditorInfo();
        mUiHandler = new Handler();

        mFactory = new TestFactory(new InputMethodManagerWrapperImpl(mContext, null, null));
        mFactory.onWindowFocusChanged(true);
        mImeHandler = mFactory.getHandler();
        mImeShadowLooper = (ShadowLooper) Shadow.extract(mImeHandler.getLooper());

        when(mContext.getSystemService(Context.INPUT_METHOD_SERVICE))
                .thenReturn(mInputMethodManager);

        ActivityController<Activity> activityController =
                Robolectric.buildActivity(Activity.class).setup();
        Activity activity = activityController.get();
        // ThreadedInputConnectionFactory#initializeAndGet() logic is activated when the package is
        // "com.htc.android.mail"
        Context htcContext =
                new ContextWrapper(activity) {
                    @Override
                    public String getPackageName() {
                        return "com.htc.android.mail";
                    }
                };
        mContainerView = new TestContainerView(htcContext);
        mContainerView.setFocusable(true);
        mContainerView.setFocusableInTouchMode(true);
        activity.setContentView(mContainerView);
        assertTrue(mContainerView.requestFocus());
        activityController.windowFocusChanged(true);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(mContainerView.hasFocus());
        assertTrue(mContainerView.hasWindowFocus());
        // Focusing the container must not have triggered the delayed proxy logic yet.
        assertNull(mProxyView);

        when(mInputMethodManager.isActive(any(View.class)))
                .thenAnswer(
                        new Answer<Boolean>() {
                            private int mCount;

                            @Override
                            public Boolean answer(InvocationOnMock invocation) {
                                View view = invocation.getArgument(0);
                                if (view == mProxyView) {
                                    return mInputConnection != null;
                                }
                                assertEquals(mContainerView, view);
                                mCount++;
                                // To simplify IMM's behavior, let's say that it succeeds input
                                // method activation only when the view has a window focus.
                                if (!mHasWindowFocus) return false;
                                if (mCount == 1) {
                                    mInputConnection =
                                            mProxyView.onCreateInputConnection(mEditorInfo);
                                    return false;
                                }
                                return mHasWindowFocus;
                            }
                        });

        mInOrder = inOrder(mImeAdapter, mInputMethodManager);
    }

    private void activateInput() {
        mUiHandler.post(
                new Runnable() {
                    @Override
                    public void run() {
                        assertNull(
                                mFactory.initializeAndGet(
                                        mContainerView,
                                        mImeAdapter,
                                        1,
                                        0,
                                        0,
                                        0,
                                        0,
                                        0,
                                        "",
                                        mEditorInfo));
                    }
                });
    }

    private void runOneUiTask() {
        assertTrue(Robolectric.getForegroundThreadScheduler().runOneTask());
    }

    @Test
    @Feature({"TextInput"})
    public void testCreateInputConnection_Success() {
        // Pause all the loopers.
        Robolectric.getForegroundThreadScheduler().pause();
        mImeShadowLooper.pause();

        activateInput();

        // The first onCreateInputConnection().
        runOneUiTask();
        assertEquals(0, mFactory.delayMs());

        assertNotNull("Proxy view should have been created.", mProxyView);
        mInOrder.verifyNoMoreInteractions();
        assertNull(mInputConnection);

        // The second onCreateInputConnection().
        runOneUiTask();
        mInOrder.verify(mInputMethodManager).isActive(mContainerView);
        assertNotNull(mInputConnection);
        assertTrue(ThreadedInputConnection.class.isInstance(mInputConnection));

        // Verification process.
        mImeShadowLooper.runOneTask();
        runOneUiTask();

        mInOrder.verify(mInputMethodManager).isActive(mProxyView);
        mInOrder.verifyNoMoreInteractions();

        assertTrue(mFactory.hasSucceeded());
        assertFalse(mFactory.hasFailed());
    }

    @Test
    @Feature({"TextInput"})
    public void testCreateInputConnection_Failure() {
        // Pause all the loopers.
        Robolectric.getForegroundThreadScheduler().pause();
        mImeShadowLooper.pause();

        activateInput();

        // The first onCreateInputConnection().
        runOneUiTask();
        assertEquals(0, mFactory.delayMs());

        assertNotNull("Proxy view should have been created.", mProxyView);
        mInOrder.verifyNoMoreInteractions();
        assertNull(mInputConnection);

        // Now window focus was lost before the second onCreateInputConnection().
        mFactory.onWindowFocusChanged(false);
        assertFalse(mProxyView.hasWindowFocus());

        // The second onCreateInputConnection().
        runOneUiTask();
        mInOrder.verify(mInputMethodManager).isActive(mContainerView);
        mInOrder.verifyNoMoreInteractions();

        // Window focus is lost and we fail to activate.
        assertNull(mInputConnection);

        // Verification process.
        mImeShadowLooper.runOneTask();
        runOneUiTask();
        mInOrder.verify(mInputMethodManager).isActive(mProxyView);

        // Wait one more UI loop.
        runOneUiTask();
        mInOrder.verify(mInputMethodManager).isActive(mProxyView);

        mInOrder.verifyNoMoreInteractions();
        // Failed, but no logging because check has been invalidated.
        assertNull(mInputConnection);
        assertFalse(mFactory.hasSucceeded());
        assertFalse(mFactory.hasFailed());
    }

    // Test for https://crbug.com/1108237
    @Test
    @Feature({"TextInput"})
    public void testCreateInputConnection_Delayed() {
        // Pause all the loopers.
        Robolectric.getForegroundThreadScheduler().pause();
        mImeShadowLooper.pause();

        mFactory.onViewFocusChanged(false);
        mFactory.onWindowFocusChanged(false);

        // Note that we gained view focus before gaining window focus.
        // We will delay the keyboard activation.
        mFactory.onViewFocusChanged(true);
        mFactory.onWindowFocusChanged(true);

        activateInput();

        // The first onCreateInputConnection().
        runOneUiTask();

        // We delay the keyboard activation when view gets focused before window does.
        assertEquals(1000, mFactory.delayMs());

        assertNotNull("Proxy view should have been created.", mProxyView);
        mInOrder.verifyNoMoreInteractions();
        assertNull(mInputConnection);

        // The second onCreateInputConnection().
        runOneUiTask();
        mInOrder.verify(mInputMethodManager).isActive(mContainerView);
        assertNotNull(mInputConnection);
        assertTrue(ThreadedInputConnection.class.isInstance(mInputConnection));

        // Verification process.
        mImeShadowLooper.runOneTask();
        runOneUiTask();

        mInOrder.verify(mInputMethodManager).isActive(mProxyView);
        mInOrder.verifyNoMoreInteractions();

        assertTrue(mFactory.hasSucceeded());
        assertFalse(mFactory.hasFailed());
    }
}
