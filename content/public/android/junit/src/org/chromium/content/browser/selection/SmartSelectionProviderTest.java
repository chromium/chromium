// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.Looper;
import android.view.textclassifier.TextClassification;
import android.view.textclassifier.TextClassificationManager;
import android.view.textclassifier.TextClassifier;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureListJni;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link SmartSelectionProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SmartSelectionProviderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SelectionClient.ResultCallback mResultCallback;
    @Mock private TextClassifier mTextClassifier;
    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Captor private ArgumentCaptor<SelectionClient.Result> mResultCaptor;

    private SmartSelectionProvider mProvider;

    @Before
    public void setUp() {
        // Mock LibraryLoader to say native is loaded.
        LibraryLoader libraryLoader = mock(LibraryLoader.class);
        when(libraryLoader.isInitialized()).thenReturn(true);
        LibraryLoader.setLibraryLoaderForTesting(libraryLoader);

        // Mock FeatureListJni to say FeatureList is initialized.
        FeatureList.Natives featureListNatives = mock(FeatureList.Natives.class);
        when(featureListNatives.isInitialized()).thenReturn(true);
        FeatureListJni.setInstanceForTesting(featureListNatives);

        FeatureOverrides.overrideParam(
                ContentFeatures.TEXT_CLASSIFIER_TIMEOUT, "timeout_ms", "200");

        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getContext())
                .thenReturn(
                        new WeakReference<Context>(ApplicationProvider.getApplicationContext()));

        TextClassificationManager tcm =
                ApplicationProvider.getApplicationContext()
                        .getSystemService(TextClassificationManager.class);
        tcm.setTextClassifier(mTextClassifier);

        mProvider = new SmartSelectionProvider(mResultCallback, mWebContents, null);
        mProvider.setExecutorForTesting(RobolectricUtil.getPausedExecutor());
        mProvider.setTextClassifier(mTextClassifier);
    }

    @Test
    @EnableFeatures(ContentFeatureList.TEXT_CLASSIFIER_TIMEOUT)
    public void testTimeoutEnabled_callsCallbackOnTimeout() throws Exception {
        when(mTextClassifier.classifyText(any(), anyInt(), anyInt(), any()))
                .thenReturn(new TextClassification.Builder().setText("classified").build());

        mProvider.sendClassifyRequest("test text", 0, 9);

        // Advance looper by 250ms to trigger timeout (timeout is 200ms).
        Shadows.shadowOf(Looper.getMainLooper()).idleFor(250, TimeUnit.MILLISECONDS);

        // Verify callback was called with empty result.
        verify(mResultCallback).onClassified(mResultCaptor.capture());

        SelectionClient.Result result = mResultCaptor.getValue();
        assertNotNull(result);
        assertNull(result.text);
    }

    @Test
    @DisableFeatures(ContentFeatureList.TEXT_CLASSIFIER_TIMEOUT)
    public void testTimeoutDisabled_doesNotCallCallbackOnTimeout() throws Exception {
        when(mTextClassifier.classifyText(any(), anyInt(), anyInt(), any()))
                .thenReturn(new TextClassification.Builder().setText("classified").build());

        mProvider.sendClassifyRequest("test text", 0, 9);

        // Advance looper by 250ms.
        Shadows.shadowOf(Looper.getMainLooper()).idleFor(250, TimeUnit.MILLISECONDS);

        // Verify callback was NOT called.
        verify(mResultCallback, never()).onClassified(any());

        // Run the background task.
        RobolectricUtil.runOneBackgroundTask();

        // Now it must have posted to main looper.
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        // Verify callback was called with real result.
        verify(mResultCallback).onClassified(mResultCaptor.capture());

        SelectionClient.Result result = mResultCaptor.getValue();
        assertNotNull(result);
        assertEquals("test text", result.text);
        assertEquals("classified", result.textClassification.getText());
    }

    @Test
    @EnableFeatures(ContentFeatureList.TEXT_CLASSIFIER_TIMEOUT)
    public void testTimeoutEnabled_callsCallbackLateOnTaskCompletionAfterTimeout()
            throws Exception {
        when(mTextClassifier.classifyText(any(), anyInt(), anyInt(), any()))
                .thenAnswer(
                        invocation -> {
                            // Simulate timeout firing while task is running.
                            mProvider.cancelAllRequests();
                            return new TextClassification.Builder().setText("classified").build();
                        });

        mProvider.sendClassifyRequest("test text", 0, 9);

        // Run the background task. It will call cancelAllRequests inside.
        RobolectricUtil.runOneBackgroundTask();

        // Advance looper to trigger onCancelled.
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        // Verify onClassifiedLate was called with real result.
        verify(mResultCallback).onClassifiedLate(mResultCaptor.capture());

        SelectionClient.Result lateResult = mResultCaptor.getValue();
        assertNotNull(lateResult);
        assertEquals("test text", lateResult.text);
        assertEquals("classified", lateResult.textClassification.getText());

        // Verify onClassified (normal) was NOT called (because timeout was cancelled before it
        // ran).
        verify(mResultCallback, never()).onClassified(any());
    }
}
