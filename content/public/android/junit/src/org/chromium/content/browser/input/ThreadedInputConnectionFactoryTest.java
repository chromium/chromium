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

import android.content.Context;
import android.os.Handler;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.InputMethodManagerWrapper;

import java.util.concurrent.Callable;

/** Unit tests for {@ThreadedInputConnectionFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ContentFeatureList.OPTIMIZE_IMM_HIDE_CALLS})
public class ThreadedInputConnectionFactoryTest {
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

    @Mock private ImeAdapterImpl mImeAdapter;
    @Mock private View mContainerView;
    @Mock private ThreadedInputConnectionProxyView mProxyView;
    @Mock private InputMethodManager mInputMethodManager;
    @Mock private Context mContext;

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
        MockitoAnnotations.initMocks(this);

        mEditorInfo = new EditorInfo();
        mUiHandler = new Handler();

        mContext = Mockito.mock(Context.class);
        mContainerView = Mockito.mock(View.class);
        mImeAdapter = Mockito.mock(ImeAdapterImpl.class);
        mInputMethodManager = Mockito.mock(InputMethodManager.class);

        mFactory = new TestFactory(new InputMethodManagerWrapperImpl(mContext, null, null));
        mFactory.onWindowFocusChanged(true);
        mImeHandler = mFactory.getHandler();
        mImeShadowLooper = (ShadowLooper) Shadow.extract(mImeHandler.getLooper());

        when(mContext.getSystemService(Context.INPUT_METHOD_SERVICE))
                .thenReturn(mInputMethodManager);
        // ThreadedInputConnectionFactory#initializeAndGet() logic is activated when the package is
        // "com.htc.android.mail"
        when(mContext.getPackageName()).thenReturn("com.htc.android.mail");
        when(mContainerView.getContext()).thenReturn(mContext);
        when(mContainerView.getHandler()).thenReturn(mUiHandler);
        when(mContainerView.hasFocus()).thenReturn(true);
        when(mContainerView.hasWindowFocus()).thenReturn(true);

        mProxyView = Mockito.mock(ThreadedInputConnectionProxyView.class);
        when(mProxyView.getContext()).thenReturn(mContext);
        when(mProxyView.requestFocus()).thenReturn(true);
        when(mProxyView.getHandler()).thenReturn(mImeHandler);
        final Callable<InputConnection> callable =
                new Callable<InputConnection>() {
                    @Override
                    public InputConnection call() {
                        return mFactory.initializeAndGet(
                                mContainerView, mImeAdapter, 1, 0, 0, 0, 0, 0, "", mEditorInfo);
                    }
                };
        when(mProxyView.onCreateInputConnection(any(EditorInfo.class)))
                .thenAnswer(
                        (InvocationOnMock invocation) -> {
                            mFactory.setTriggerDelayedOnCreateInputConnection(false);
                            InputConnection connection =
                                    ThreadUtils.runOnUiThreadBlocking(callable);
                            mFactory.setTriggerDelayedOnCreateInputConnection(true);
                            return connection;
                        });

        when(mInputMethodManager.isActive(mContainerView))
                .thenAnswer(
                        new Answer<Boolean>() {
                            private int mCount;

                            @Override
                            public Boolean answer(InvocationOnMock invocation) {
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
        when(mInputMethodManager.isActive(mProxyView))
                .thenAnswer(
                        new Answer<Boolean>() {
                            @Override
                            public Boolean answer(InvocationOnMock invocation) {
                                return mInputConnection != null;
                            }
                        });

        mInOrder = inOrder(mImeAdapter, mInputMethodManager, mContainerView, mProxyView);
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

        mInOrder.verify(mContainerView).hasFocus();
        mInOrder.verify(mContainerView).hasWindowFocus();
        mInOrder.verify(mProxyView).requestFocus();
        mInOrder.verify(mContainerView).getHandler();
        mInOrder.verifyNoMoreInteractions();
        assertNull(mInputConnection);

        // The second onCreateInputConnection().
        runOneUiTask();
        mInOrder.verify(mProxyView).onWindowFocusChanged(true);
        mInOrder.verify(mInputMethodManager).isActive(mContainerView);
        mInOrder.verify(mProxyView).onCreateInputConnection(any(EditorInfo.class));
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

        mInOrder.verify(mContainerView).hasFocus();
        mInOrder.verify(mContainerView).hasWindowFocus();
        mInOrder.verify(mProxyView).requestFocus();
        mInOrder.verify(mContainerView).getHandler();
        mInOrder.verifyNoMoreInteractions();
        assertNull(mInputConnection);

        // Now window focus was lost before the second onCreateInputConnection().
        mFactory.onWindowFocusChanged(false);
        mInOrder.verify(mProxyView).onOriginalViewWindowFocusChanged(false);

        // The second onCreateInputConnection().
        runOneUiTask();
        mInOrder.verify(mProxyView).onWindowFocusChanged(true);
        mInOrder.verify(mInputMethodManager).isActive(mContainerView);
        mInOrder.verifyNoMoreInteractions();

        // Window focus is lost and we fail to activate.
        assertNull(mInputConnection);

        // Verification process.
        mImeShadowLooper.runOneTask();
        mInOrder.verify(mContainerView).getHandler();
        runOneUiTask();
        mInOrder.verify(mInputMethodManager).isActive(mProxyView);

        // Wait one more UI loop.
        mInOrder.verify(mContainerView).getHandler();
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

        mInOrder.verify(mContainerView).hasFocus();
        mInOrder.verify(mContainerView).hasWindowFocus();
        mInOrder.verify(mProxyView).requestFocus();
        mInOrder.verify(mContainerView).getHandler();
        mInOrder.verifyNoMoreInteractions();
        assertNull(mInputConnection);

        // The second onCreateInputConnection().
        runOneUiTask();
        mInOrder.verify(mProxyView).onWindowFocusChanged(true);
        mInOrder.verify(mInputMethodManager).isActive(mContainerView);
        mInOrder.verify(mProxyView).onCreateInputConnection(any(EditorInfo.class));
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
