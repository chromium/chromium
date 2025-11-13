// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

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
import android.view.KeyEvent;
import android.view.ViewGroup;

import androidx.test.core.app.ApplicationProvider;

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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink.mojom.EventType;
import org.chromium.blink_public.web.WebInputEventModifier;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ImeEventObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ime.TextInputType;
import org.chromium.ui.test.util.TestViewAndroidDelegate;

/** Unit tests for {@link ImeAdapterImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ImeAdapterImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContentsImpl mWebContentsImpl;
    @Mock private ViewGroup mContainerView;
    @Mock private ImeEventObserver mImeEventObserver;
    @Mock private ImeAdapterImpl.Natives mImeAdapterImplJni;

    @Before
    public void setUp() {
        ImeAdapterImplJni.setInstanceForTesting(mImeAdapterImplJni);
        when(mImeAdapterImplJni.init(any(), any())).thenReturn(1L);
        Mockito.doAnswer(
                        (Answer<Object>)
                                invocation -> {
                                    WebContents.UserDataFactory factory = invocation.getArgument(1);
                                    return factory.create(mWebContentsImpl);
                                })
                .when(mWebContentsImpl)
                .getOrSetUserData(any(), any());

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
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        adapter.onConnectedToRenderProcess();

        adapter.commitContent(/* dataUrl= */ "atestingdataurl");

        verify(mImeAdapterImplJni).insertMediaFromURL(anyLong(), eq("atestingdataurl"));
    }

    @Test
    public void testPerformSpellCheck() {
        ImeAdapterImpl adapter = new ImeAdapterImpl(mWebContentsImpl);
        adapter.onConnectedToRenderProcess();

        adapter.performSpellCheck();

        verify(mImeAdapterImplJni).performSpellCheck(anyLong());
    }
}
