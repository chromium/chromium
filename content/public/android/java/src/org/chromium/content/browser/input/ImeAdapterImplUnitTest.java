// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNotNull;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.SystemClock;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.SuggestionSpan;
import android.view.KeyEvent;
import android.view.ViewGroup;
import android.view.inputmethod.CorrectionInfo;
import android.view.inputmethod.EditorInfo;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink.mojom.EventType;
import org.chromium.blink_public.web.WebInputEventModifier;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ImeEventObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.ime.TextInputType;
import org.chromium.ui.mojom.ImeTextSpanType;
import org.chromium.ui.test.util.TestViewAndroidDelegate;

/** Unit tests for {@link ImeAdapterImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
    ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE,
    ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2,
    ContentFeatureList.ANDROID_BLOCK_GRAMMAR_SUGGESTION_SPAN_IN_COMPOSITION_MODE,
    ContentFeatureList.ANDROID_BLOCK_MISSPELLING_SUGGESTION_SPAN_IN_COMPOSITION_MODE,
    ContentFeatureList.ANDROID_MEDIA_INSERTION
})
public class ImeAdapterImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContentsImpl mWebContentsImpl;
    @Mock private ViewGroup mContainerView;
    @Mock private ImeEventObserver mImeEventObserver;
    @Mock private ImeAdapterImpl.Natives mImeAdapterImplJni;
    @Mock private CorrectionInfo mCorrectionInfo;
    @Mock private AutocorrectManager mAutocorrectManager;

    @Before
    public void setUp() {
        ImeAdapterImplJni.setInstanceForTesting(mImeAdapterImplJni);
        when(mImeAdapterImplJni.create(any())).thenReturn(1L);
        Mockito.doAnswer(
                        (Answer<Object>)
                                invocation -> {
                                    WebContents.UserDataFactory factory = invocation.getArgument(1);
                                    return factory.create(mWebContentsImpl);
                                })
                .when(mWebContentsImpl)
                .getOrSetUserData(any(), any());

        when(mContainerView.getContext()).thenReturn(ApplicationProvider.getApplicationContext());
        when(mContainerView.getResources())
                .thenReturn(ApplicationProvider.getApplicationContext().getResources());
        when(mWebContentsImpl.getViewAndroidDelegate())
                .thenReturn(new TestViewAndroidDelegate(mContainerView));
    }

    @Test
    public void testOnNodeAttributeUpdated() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        adapter.addEventObserver(mImeEventObserver);

        updateTextInputType(adapter, TextInputType.TEXT);
        verify(mImeEventObserver).onNodeAttributeUpdated(eq(true), eq(false));
        reset(mImeEventObserver);

        updateTextInputType(adapter, TextInputType.TEXT);
        verify(mImeEventObserver, never()).onNodeAttributeUpdated(anyBoolean(), anyBoolean());

        updateTextInputType(adapter, TextInputType.NONE);
        verify(mImeEventObserver).onNodeAttributeUpdated(eq(false), eq(false));
        reset(mImeEventObserver);

        adapter.resetAndHideKeyboard();
        verify(mImeEventObserver, never()).onNodeAttributeUpdated(anyBoolean(), anyBoolean());

        updateTextInputType(adapter, TextInputType.TEXT);
        verify(mImeEventObserver).onNodeAttributeUpdated(eq(true), eq(false));
        reset(mImeEventObserver);

        adapter.resetAndHideKeyboard();
        verify(mImeEventObserver).onNodeAttributeUpdated(eq(false), eq(false));
        reset(mImeEventObserver);
    }

    private void updateTextInputType(ImeAdapterImpl adapter, @TextInputType int textInputType) {
        adapter.updateState(
                /* textInputType= */ textInputType,
                /* textInputFlags= */ 0,
                /* textInputMode= */ 0,
                /* textInputAction= */ 0,
                /* showIfNeeded= */ false,
                /* alwaysHide= */ false,
                /* text= */ "",
                /* selectionStart= */ 0,
                /* selectionEnd= */ 0,
                /* compositionStart= */ 0,
                /* compositionEnd= */ 0,
                /* replyToRequest= */ false,
                /* lastVkVisibilityRequest= */ 0,
                /* vkPolicy= */ 0,
                /* imeTextSpans= */ null);
    }

    @Test
    @EnableFeatures(ContentFeatureList.ANDROID_CAPTURE_KEY_EVENTS)
    public void testSendCompositionToNative() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        adapter.onConnectedToRenderProcess();

        // sendCompositionToNative() sends the committed text to native.
        adapter.sendCompositionToNative("abc", 3, true, 0);
        verify(mImeAdapterImplJni)
                .sendKeyEvent(
                        anyLong(),
                        isNull(),
                        eq(EventType.RAW_KEY_DOWN),
                        eq(0),
                        anyLong(),
                        eq(ImeAdapterImpl.COMPOSITION_KEY_CODE),
                        eq(0),
                        eq(false),
                        eq(0));
        verify(mImeAdapterImplJni).commitText(anyLong(), any(), eq("abc"), eq("abc"), eq(3));
        verify(mImeAdapterImplJni)
                .sendKeyEvent(
                        anyLong(),
                        isNull(),
                        eq(EventType.KEY_UP),
                        eq(0),
                        anyLong(),
                        eq(ImeAdapterImpl.COMPOSITION_KEY_CODE),
                        eq(0),
                        eq(false),
                        eq(0));
        reset(mImeAdapterImplJni);

        // If there was a corresponding key down event, call sendKeyEvent() instead.
        long time1 = SystemClock.uptimeMillis();
        KeyEvent event1 =
                new KeyEvent(
                        time1,
                        time1,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_A,
                        0,
                        KeyEvent.META_SHIFT_ON);
        adapter.onKeyPreIme(event1.getKeyCode(), event1);
        adapter.sendCompositionToNative("A", 1, true, 0);
        verify(mImeAdapterImplJni)
                .sendKeyEvent(
                        anyLong(),
                        isNotNull(),
                        eq(EventType.KEY_DOWN),
                        eq(WebInputEventModifier.SHIFT_KEY),
                        eq(time1),
                        eq(event1.getKeyCode()),
                        eq(event1.getScanCode()),
                        eq(false),
                        eq(event1.getUnicodeChar()));
        verify(mImeAdapterImplJni, never()).commitText(anyLong(), any(), any(), any(), anyInt());
        reset(mImeAdapterImplJni);

        // Test ALT_RIGHT_ON (AltGr).
        long timeAltGr = SystemClock.uptimeMillis();
        KeyEvent eventAltGr =
                new KeyEvent(
                        timeAltGr,
                        timeAltGr,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_A,
                        0,
                        KeyEvent.META_ALT_RIGHT_ON);
        adapter.onKeyPreIme(eventAltGr.getKeyCode(), eventAltGr);
        adapter.sendCompositionToNative("a", 1, true, 0);
        verify(mImeAdapterImplJni)
                .sendKeyEvent(
                        anyLong(),
                        isNotNull(),
                        eq(EventType.KEY_DOWN),
                        eq(WebInputEventModifier.ALT_GR_KEY),
                        eq(timeAltGr),
                        eq(eventAltGr.getKeyCode()),
                        eq(eventAltGr.getScanCode()),
                        eq(false),
                        eq(eventAltGr.getUnicodeChar()));
        verify(mImeAdapterImplJni, never()).commitText(anyLong(), any(), any(), any(), anyInt());
        reset(mImeAdapterImplJni);

        // Ignore events that are too old or don't match with the committed text.
        long time2 = SystemClock.uptimeMillis() - 60 * 1000;
        KeyEvent event2 =
                new KeyEvent(
                        time2,
                        time2,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_B,
                        0,
                        KeyEvent.META_SHIFT_ON);
        long time3 = SystemClock.uptimeMillis();
        KeyEvent event3 =
                new KeyEvent(
                        time3,
                        time3,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_C,
                        0,
                        KeyEvent.META_SHIFT_ON);
        long time4 = SystemClock.uptimeMillis();
        KeyEvent event4 =
                new KeyEvent(
                        time4,
                        time4,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_B,
                        0,
                        KeyEvent.META_SHIFT_ON);
        adapter.onKeyPreIme(event2.getKeyCode(), event2);
        adapter.onKeyPreIme(event3.getKeyCode(), event3);
        adapter.onKeyPreIme(event4.getKeyCode(), event4);
        adapter.sendCompositionToNative("B", 1, true, 0);
        verify(mImeAdapterImplJni)
                .sendKeyEvent(
                        anyLong(),
                        isNotNull(),
                        eq(EventType.KEY_DOWN),
                        eq(WebInputEventModifier.SHIFT_KEY),
                        eq(time4),
                        eq(event4.getKeyCode()),
                        eq(event4.getScanCode()),
                        eq(false),
                        eq(event4.getUnicodeChar()));
        verify(mImeAdapterImplJni, never()).commitText(anyLong(), any(), any(), any(), anyInt());
    }

    @Test
    public void testCommitContent() {
        when(mImeAdapterImplJni.insertMediaFromBytes(anyLong(), any(), any())).thenReturn(true);

        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        adapter.onConnectedToRenderProcess();

        Assert.assertTrue(
                adapter.commitContent(/* bytes= */ new byte[] {1, 2, 3}, /* extension= */ "png"));

        verify(mImeAdapterImplJni)
                .insertMediaFromBytes(anyLong(), eq(new byte[] {1, 2, 3}), eq("png"));
    }

    @Test
    public void testCommitContent_Failure() {
        when(mImeAdapterImplJni.insertMediaFromBytes(anyLong(), any(), any())).thenReturn(false);

        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        adapter.onConnectedToRenderProcess();

        Assert.assertFalse(adapter.commitContent(new byte[] {1, 2, 3}, "png"));
    }

    @Test
    public void testPerformSpellCheck() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        adapter.onConnectedToRenderProcess();

        adapter.performSpellCheck();

        verify(mImeAdapterImplJni).performSpellCheck(anyLong());
    }

    @Test
    public void testCommitCorrection() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        adapter.setAutocorrectManagerForTesting(mAutocorrectManager);
        adapter.onConnectedToRenderProcess();

        adapter.commitCorrection(mCorrectionInfo);

        verify(mAutocorrectManager).handlePendingCorrection(mCorrectionInfo);
    }

    @Test
    public void testSendKeyEventKeyDownCallsonCommitTextOrSendKeyEvent() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        adapter.setAutocorrectManagerForTesting(mAutocorrectManager);
        adapter.onConnectedToRenderProcess();

        KeyEvent event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A);
        adapter.sendKeyEvent(event);

        verify(mAutocorrectManager).onCommitTextOrSendKeyEvent();
    }

    @Test
    public void testSendKeyEventKeyUpDoesNotCallonCommitTextOrSendKeyEvent() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        adapter.setAutocorrectManagerForTesting(mAutocorrectManager);
        adapter.onConnectedToRenderProcess();

        KeyEvent event = new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_A);
        adapter.sendKeyEvent(event);

        verify(mAutocorrectManager, never()).onCommitTextOrSendKeyEvent();
    }

    @Test
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    public void testAutocorrectManagerInitialisationWhenFlagEnabled() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);

        assertNotNull(adapter.getAutocorrectManagerForTesting());
    }

    @Test
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2)
    public void testAutocorrectManagerInitialisationWhenV2FlagEnabled() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);

        assertNotNull(adapter.getAutocorrectManagerForTesting());
    }

    @Test
    public void testAutocorrectManagerInitialisationWhenFlagDisabled() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);

        assertNull(adapter.getAutocorrectManagerForTesting());
    }

    @Test
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    public void testAppendAutocorrectUnderlineSpan() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        adapter.onConnectedToRenderProcess();

        adapter.appendAutocorrectUnderlineSpan(0, 8);

        verify(mImeAdapterImplJni).appendAutocorrectUnderlineSpan(anyLong(), eq(0), eq(8));
    }

    @Test
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    public void testClearAutocorrectUnderlineSpan() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        adapter.onConnectedToRenderProcess();

        adapter.clearAllAutocorrectUnderlineSpans();

        verify(mImeAdapterImplJni).clearAllAutocorrectUnderlineSpans(anyLong());
    }

    @Test
    public void testPopulateImeTextSpansFromJava_MisspellingSpanNotBlocked() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        SpannableString text = new SpannableString("hello");
        String[] suggestions = new String[] {"suggestion"};
        SuggestionSpan span =
                new SuggestionSpan(
                        ApplicationProvider.getApplicationContext(),
                        suggestions,
                        SuggestionSpan.FLAG_MISSPELLED);
        text.setSpan(span, 0, 5, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        adapter.populateImeTextSpansFromJava(text, 123L);

        verify(mImeAdapterImplJni)
                .appendSuggestionSpan(
                        /* spanPtr= */ eq(123L),
                        /* start= */ eq(0),
                        /* end= */ eq(5),
                        /* type= */ eq(ImeTextSpanType.MISSPELLING_SUGGESTION),
                        /* removeOnFinishComposing= */ eq(false),
                        /* underlineColor= */ anyInt(),
                        /* suggestionHighlightColor= */ anyInt(),
                        /* suggestions= */ eq(suggestions),
                        /* shouldHideSuggestionMenu= */ eq(true));
    }

    @Test
    public void testPopulateImeTextSpansFromJava_GrammarSpanNotBlocked() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        SpannableString text = new SpannableString("hello");
        String[] suggestions = new String[] {"suggestion"};
        SuggestionSpan span =
                new SuggestionSpan(
                        ApplicationProvider.getApplicationContext(),
                        suggestions,
                        SuggestionSpan.FLAG_GRAMMAR_ERROR);
        text.setSpan(span, 0, 5, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        adapter.populateImeTextSpansFromJava(text, 123L);

        verify(mImeAdapterImplJni)
                .appendSuggestionSpan(
                        /* spanPtr= */ eq(123L),
                        /* start= */ eq(0),
                        /* end= */ eq(5),
                        /* type= */ eq(ImeTextSpanType.GRAMMAR_SUGGESTION),
                        /* removeOnFinishComposing= */ eq(false),
                        /* underlineColor= */ anyInt(),
                        /* suggestionHighlightColor= */ anyInt(),
                        /* suggestions= */ eq(suggestions),
                        /* shouldHideSuggestionMenu= */ eq(true));
    }

    @Test
    public void testPopulateImeTextSpansFromJava_AutoCorrectionSpan() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        SpannableString text = new SpannableString("hello");
        SuggestionSpan span =
                new SuggestionSpan(
                        ApplicationProvider.getApplicationContext(),
                        new String[] {"suggestion"},
                        SuggestionSpan.FLAG_AUTO_CORRECTION);
        text.setSpan(span, 0, 5, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        adapter.populateImeTextSpansFromJava(text, 123L);

        verify(mImeAdapterImplJni)
                .appendSuggestionSpan(
                        /* spanPtr= */ eq(123L),
                        /* start= */ eq(0),
                        /* end= */ eq(5),
                        /* type= */ eq(ImeTextSpanType.AUTOCORRECT),
                        /* removeOnFinishComposing= */ eq(false),
                        /* underlineColor= */ anyInt(),
                        /* suggestionHighlightColor= */ anyInt(),
                        /* suggestions= */ eq(new String[0]),
                        /* shouldHideSuggestionMenu= */ eq(true));
    }

    @Test
    @EnableFeatures(
            ContentFeatureList.ANDROID_BLOCK_MISSPELLING_SUGGESTION_SPAN_IN_COMPOSITION_MODE)
    public void testPopulateImeTextSpansFromJava_MisspellingSpanBlocked() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        SpannableString text = new SpannableString("hello");
        SuggestionSpan span =
                new SuggestionSpan(
                        ApplicationProvider.getApplicationContext(),
                        new String[] {"suggestion"},
                        SuggestionSpan.FLAG_MISSPELLED);
        text.setSpan(span, 0, 5, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        adapter.populateImeTextSpansFromJava(text, 123L);

        verify(mImeAdapterImplJni, never())
                .appendSuggestionSpan(
                        /* spanPtr= */ anyLong(),
                        /* start= */ anyInt(),
                        /* end= */ anyInt(),
                        /* type= */ anyInt(),
                        /* removeOnFinishComposing= */ anyBoolean(),
                        /* underlineColor= */ anyInt(),
                        /* suggestionHighlightColor= */ anyInt(),
                        /* suggestions= */ any(),
                        /* shouldHideSuggestionMenu= */ anyBoolean());
    }

    @Test
    @EnableFeatures(ContentFeatureList.ANDROID_BLOCK_GRAMMAR_SUGGESTION_SPAN_IN_COMPOSITION_MODE)
    public void testPopulateImeTextSpansFromJava_GrammarSpanBlocked() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        SpannableString text = new SpannableString("hello");
        SuggestionSpan span =
                new SuggestionSpan(
                        ApplicationProvider.getApplicationContext(),
                        new String[] {"suggestion"},
                        SuggestionSpan.FLAG_GRAMMAR_ERROR);
        text.setSpan(span, 0, 5, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        adapter.populateImeTextSpansFromJava(text, 123L);

        verify(mImeAdapterImplJni, never())
                .appendSuggestionSpan(
                        /* spanPtr= */ anyLong(),
                        /* start= */ anyInt(),
                        /* end= */ anyInt(),
                        /* type= */ anyInt(),
                        /* removeOnFinishComposing= */ anyBoolean(),
                        /* underlineColor= */ anyInt(),
                        /* suggestionHighlightColor= */ anyInt(),
                        /* suggestions= */ any(),
                        /* shouldHideSuggestionMenu= */ anyBoolean());
    }

    @Test
    public void testOnCreateInputConnection_Default() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        updateTextInputType(adapter, TextInputType.TEXT);
        EditorInfo outAttrs = new EditorInfo();
        adapter.onCreateInputConnection(outAttrs);

        Assert.assertTrue((outAttrs.imeOptions & EditorInfo.IME_FLAG_NO_FULLSCREEN) != 0);
        Assert.assertTrue((outAttrs.imeOptions & EditorInfo.IME_FLAG_NO_EXTRACT_UI) != 0);
    }

    @Test
    public void testOnCreateInputConnection_AllowFullscreenIme() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        updateTextInputType(adapter, TextInputType.TEXT);
        adapter.setAllowFullscreenIme(true);
        EditorInfo outAttrs = new EditorInfo();
        adapter.onCreateInputConnection(outAttrs);

        Assert.assertTrue((outAttrs.imeOptions & EditorInfo.IME_FLAG_NO_FULLSCREEN) == 0);
        Assert.assertTrue((outAttrs.imeOptions & EditorInfo.IME_FLAG_NO_EXTRACT_UI) == 0);
    }
}
