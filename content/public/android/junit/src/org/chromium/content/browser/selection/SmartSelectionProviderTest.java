// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.Build;
import android.os.Looper;
import android.view.textclassifier.TextClassification;
import android.view.textclassifier.TextClassificationManager;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextSelection;

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

import org.chromium.base.AconfigFlaggedApiDelegate;
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
public class SmartSelectionProviderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SelectionClient.ResultCallback mResultCallback;
    @Mock private TextClassifier mTextClassifier;
    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private LibraryLoader mLibraryLoader;
    @Mock private FeatureList.Natives mFeatureListNatives;
    @Captor private ArgumentCaptor<SelectionClient.Result> mResultCaptor;

    private SmartSelectionProvider mProvider;

    @Before
    public void setUp() {
        // Mock LibraryLoader to say native is loaded.
        when(mLibraryLoader.isInitialized()).thenReturn(true);
        LibraryLoader.setLibraryLoaderForTesting(mLibraryLoader);

        // Mock FeatureListJni to say FeatureList is initialized.
        when(mFeatureListNatives.isInitialized()).thenReturn(true);
        FeatureListJni.setInstanceForTesting(mFeatureListNatives);

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

    @Test
    @Config(sdk = {Build.VERSION_CODES.S, BaseRobolectricTestRunner.MAX_SDK})
    @DisableFeatures(ContentFeatureList.TEXT_CLASSIFIER_TIMEOUT)
    public void testSendClassifyRequest_onAndroidS_doesNotThrowNpe() {
        when(mTextClassifier.classifyText(any(), anyInt(), anyInt(), any()))
                .thenReturn(new TextClassification.Builder().setText("classified").build());

        mProvider.sendClassifyRequest("test text", 0, 9);
        RobolectricUtil.runOneBackgroundTask();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mResultCallback).onClassified(mResultCaptor.capture());
        SelectionClient.Result result = mResultCaptor.getValue();
        assertNotNull(result);
        assertEquals("test text", result.text);
        assertNull(result.textSelection);
        assertNotNull(result.textClassification);
        assertEquals("classified", result.textClassification.getText());
        verify(mTextClassifier).classifyText(any(), anyInt(), anyInt(), any());
    }

    @Test
    @Config(sdk = {Build.VERSION_CODES.S, BaseRobolectricTestRunner.MAX_SDK})
    @DisableFeatures(ContentFeatureList.TEXT_CLASSIFIER_TIMEOUT)
    public void testSendSuggestAndClassifyRequest_onAndroidS_usesClassificationFromSelection() {
        TextClassification tc =
                new TextClassification.Builder().setText("classified_in_selection").build();
        TextSelection ts = new TextSelection.Builder(0, 9).setTextClassification(tc).build();
        when(mTextClassifier.suggestSelection(any(TextSelection.Request.class))).thenReturn(ts);

        mProvider.sendSuggestAndClassifyRequest("test text", 0, 4);
        RobolectricUtil.runOneBackgroundTask();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mResultCallback).onClassified(mResultCaptor.capture());
        SelectionClient.Result result = mResultCaptor.getValue();
        assertNotNull(result);
        assertEquals("test text", result.text);
        assertEquals(ts, result.textSelection);
        assertEquals(tc, result.textClassification);
        verify(mTextClassifier, never()).classifyText(any(), anyInt(), anyInt(), any());
    }

    @Test
    @Config(sdk = {Build.VERSION_CODES.S, BaseRobolectricTestRunner.MAX_SDK})
    @DisableFeatures(ContentFeatureList.TEXT_CLASSIFIER_TIMEOUT)
    public void testSendSuggestAndClassifyRequest_onAndroidS_fallbackToClassifyTextWhenNull() {
        TextSelection ts = new TextSelection.Builder(0, 9).build();
        TextClassification tc =
                new TextClassification.Builder().setText("fallback_classified").build();
        when(mTextClassifier.suggestSelection(any(TextSelection.Request.class))).thenReturn(ts);
        when(mTextClassifier.classifyText(any(), anyInt(), anyInt(), any())).thenReturn(tc);

        mProvider.sendSuggestAndClassifyRequest("test text", 0, 4);
        RobolectricUtil.runOneBackgroundTask();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mResultCallback).onClassified(mResultCaptor.capture());
        SelectionClient.Result result = mResultCaptor.getValue();
        assertNotNull(result);
        assertEquals(ts, result.textSelection);
        assertEquals(tc, result.textClassification);
        verify(mTextClassifier).classifyText(any(), anyInt(), anyInt(), any());
    }

    @Test
    @Config(sdk = {BaseRobolectricTestRunner.MIN_SDK, Build.VERSION_CODES.R})
    @DisableFeatures(ContentFeatureList.TEXT_CLASSIFIER_TIMEOUT)
    public void testSendSuggestAndClassifyRequest_preAndroidS() {
        TextSelection ts = new TextSelection.Builder(0, 9).build();
        TextClassification tc =
                new TextClassification.Builder().setText("pre_s_classified").build();
        when(mTextClassifier.suggestSelection(any(CharSequence.class), anyInt(), anyInt(), any()))
                .thenReturn(ts);
        when(mTextClassifier.classifyText(any(), anyInt(), anyInt(), any())).thenReturn(tc);

        mProvider.sendSuggestAndClassifyRequest("test text", 0, 4);
        RobolectricUtil.runOneBackgroundTask();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mResultCallback).onClassified(mResultCaptor.capture());
        SelectionClient.Result result = mResultCaptor.getValue();
        assertNotNull(result);
        assertEquals(ts, result.textSelection);
        assertEquals(tc, result.textClassification);
        verify(mTextClassifier)
                .suggestSelection(any(CharSequence.class), anyInt(), anyInt(), any());
        verify(mTextClassifier).classifyText(any(), anyInt(), anyInt(), any());
    }

    @Test
    @Config(sdk = {Build.VERSION_CODES.S, BaseRobolectricTestRunner.MAX_SDK})
    @DisableFeatures(ContentFeatureList.TEXT_CLASSIFIER_TIMEOUT)
    public void testSendClassifyRequest_forwardTextSelectionToSystemUi_skipsTextClassifier() {
        AconfigFlaggedApiDelegate delegate = mock(AconfigFlaggedApiDelegate.class);
        when(delegate.isForwardTextSelectionToSystemUiEnabled()).thenReturn(true);
        AconfigFlaggedApiDelegate.setInstanceForTesting(delegate);

        // Clear custom TextClassifier so system TextClassifier is used.
        mProvider.setTextClassifier(null);
        TextClassificationManager tcm =
                ApplicationProvider.getApplicationContext()
                        .getSystemService(TextClassificationManager.class);
        tcm.setTextClassifier(mTextClassifier);

        mProvider.sendClassifyRequest("surrounding test text", 12, 16);

        verify(mResultCallback).onClassified(mResultCaptor.capture());
        SelectionClient.Result result = mResultCaptor.getValue();
        assertNotNull(result);
        assertEquals("surrounding test text", result.text);
        assertEquals(12, result.start);
        assertEquals(16, result.end);
        assertNull(result.textClassification);
        verify(mTextClassifier, never()).classifyText(any(), anyInt(), anyInt(), any());
    }

    @Test
    @Config(sdk = {Build.VERSION_CODES.S, BaseRobolectricTestRunner.MAX_SDK})
    @DisableFeatures(ContentFeatureList.TEXT_CLASSIFIER_TIMEOUT)
    public void
            testSendSuggestAndClassifyRequest_forwardTextSelectionToSystemUi_suggestsWithoutClassification() {
        AconfigFlaggedApiDelegate delegate = mock(AconfigFlaggedApiDelegate.class);
        when(delegate.isForwardTextSelectionToSystemUiEnabled()).thenReturn(true);
        AconfigFlaggedApiDelegate.setInstanceForTesting(delegate);

        // Clear custom TextClassifier so system TextClassifier is used.
        mProvider.setTextClassifier(null);
        TextClassificationManager tcm =
                ApplicationProvider.getApplicationContext()
                        .getSystemService(TextClassificationManager.class);
        tcm.setTextClassifier(mTextClassifier);

        TextSelection ts = new TextSelection.Builder(12, 21).build();
        when(mTextClassifier.suggestSelection(any(TextSelection.Request.class))).thenReturn(ts);

        mProvider.sendSuggestAndClassifyRequest("surrounding test text", 12, 16);
        RobolectricUtil.runOneBackgroundTask();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        ArgumentCaptor<TextSelection.Request> requestCaptor =
                ArgumentCaptor.forClass(TextSelection.Request.class);
        verify(mTextClassifier).suggestSelection(requestCaptor.capture());
        assertFalse(requestCaptor.getValue().shouldIncludeTextClassification());
        verify(mTextClassifier, never()).classifyText(any(), anyInt(), anyInt(), any());

        verify(mResultCallback).onClassified(mResultCaptor.capture());
        SelectionClient.Result result = mResultCaptor.getValue();
        assertNotNull(result);
        assertEquals("surrounding test text", result.text);
        assertEquals(12, result.start);
        assertEquals(21, result.end);
        assertEquals(0, result.startAdjust);
        assertEquals(5, result.endAdjust);
        assertEquals(ts, result.textSelection);
        assertNull(result.textClassification);
    }

    @Test
    @Config(sdk = {Build.VERSION_CODES.S, BaseRobolectricTestRunner.MAX_SDK})
    @DisableFeatures(ContentFeatureList.TEXT_CLASSIFIER_TIMEOUT)
    public void
            testSendClassifyRequest_forwardTextSelectionToSystemUiWithCustomClassifier_usesLocalClassifier() {
        AconfigFlaggedApiDelegate delegate = mock(AconfigFlaggedApiDelegate.class);
        when(delegate.isForwardTextSelectionToSystemUiEnabled()).thenReturn(true);
        AconfigFlaggedApiDelegate.setInstanceForTesting(delegate);

        // mProvider has a custom TextClassifier set in setUp().
        when(mTextClassifier.classifyText(any(), anyInt(), anyInt(), any()))
                .thenReturn(new TextClassification.Builder().setText("classified").build());

        mProvider.sendClassifyRequest("test text", 0, 9);
        RobolectricUtil.runOneBackgroundTask();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mResultCallback).onClassified(mResultCaptor.capture());
        SelectionClient.Result result = mResultCaptor.getValue();
        assertNotNull(result);
        assertEquals("test text", result.text);
        assertNotNull(result.textClassification);
        assertEquals("classified", result.textClassification.getText());
        verify(mTextClassifier).classifyText(any(), anyInt(), anyInt(), any());
    }

    @Test
    @Config(sdk = {Build.VERSION_CODES.S, BaseRobolectricTestRunner.MAX_SDK})
    @DisableFeatures(ContentFeatureList.TEXT_CLASSIFIER_TIMEOUT)
    public void testSendClassifyRequest_destroyedWebContents_doesNotAccessWebContentsUserData() {
        AconfigFlaggedApiDelegate delegate = mock(AconfigFlaggedApiDelegate.class);
        when(delegate.isForwardTextSelectionToSystemUiEnabled()).thenReturn(true);
        AconfigFlaggedApiDelegate.setInstanceForTesting(delegate);

        mProvider.setTextClassifier(null);
        TextClassificationManager tcm =
                ApplicationProvider.getApplicationContext()
                        .getSystemService(TextClassificationManager.class);
        tcm.setTextClassifier(mTextClassifier);
        when(mTextClassifier.classifyText(any(), anyInt(), anyInt(), any()))
                .thenReturn(new TextClassification.Builder().setText("classified").build());

        clearInvocations(mWebContents);
        when(mWebContents.isDestroyed()).thenReturn(true);
        mProvider.sendClassifyRequest("test text", 0, 9);
        RobolectricUtil.runOneBackgroundTask();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mWebContents, never()).getOrSetUserData(any(), any());
        verify(mTextClassifier).classifyText(any(), anyInt(), anyInt(), any());
    }

    @Test
    @Config(sdk = {Build.VERSION_CODES.S, BaseRobolectricTestRunner.MAX_SDK})
    @DisableFeatures(ContentFeatureList.TEXT_CLASSIFIER_TIMEOUT)
    public void testDestroy_clearsWebContentsReference() {
        AconfigFlaggedApiDelegate delegate = mock(AconfigFlaggedApiDelegate.class);
        when(delegate.isForwardTextSelectionToSystemUiEnabled()).thenReturn(true);
        AconfigFlaggedApiDelegate.setInstanceForTesting(delegate);

        mProvider.setTextClassifier(null);
        TextClassificationManager tcm =
                ApplicationProvider.getApplicationContext()
                        .getSystemService(TextClassificationManager.class);
        tcm.setTextClassifier(mTextClassifier);
        when(mTextClassifier.classifyText(any(), anyInt(), anyInt(), any()))
                .thenReturn(new TextClassification.Builder().setText("classified").build());

        mProvider.destroy();
        clearInvocations(mWebContents);
        mProvider.sendClassifyRequest("test text", 0, 9);
        RobolectricUtil.runOneBackgroundTask();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mWebContents, never()).isDestroyed();
        verify(mWebContents, never()).getOrSetUserData(any(), any());
        verify(mTextClassifier).classifyText(any(), anyInt(), anyInt(), any());
    }
}
