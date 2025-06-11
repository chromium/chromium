// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.content.browser.webcontents.WebContentsImpl;
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
                /* vkPolicy= */ 0);
    }
}
