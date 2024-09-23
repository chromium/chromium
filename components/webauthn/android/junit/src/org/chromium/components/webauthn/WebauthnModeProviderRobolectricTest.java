// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.webauthn.cred_man.AppCredManRequestDecorator;
import org.chromium.components.webauthn.cred_man.BrowserCredManRequestDecorator;
import org.chromium.components.webauthn.cred_man.GpmCredManRequestDecorator;
import org.chromium.content_public.browser.WebContents;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {})
public class WebauthnModeProviderRobolectricTest {
    private WebauthnModeProvider mProvider = WebauthnModeProvider.getInstance();
    @Mock WebContents mWebContents;
    @Mock WebauthnModeProvider.Natives mNatives;

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebauthnModeProviderJni.TEST_HOOKS, mNatives);
    }

    @Test
    @SmallTest
    public void testGetCredManRequestDecorator_whenGlobalModeApp_thenAppDecorator() {
        mProvider.setGlobalWebauthnMode(WebauthnMode.APP);

        assertThat(mProvider.getCredManRequestDecorator(mWebContents))
                .isInstanceOf(AppCredManRequestDecorator.class);
    }

    @Test
    @SmallTest
    public void testGetCredManRequestDecorator_whenGlobalModeBrowser_thenBrowserDecorator() {
        mProvider.setGlobalWebauthnMode(WebauthnMode.BROWSER);

        assertThat(mProvider.getCredManRequestDecorator(mWebContents))
                .isInstanceOf(BrowserCredManRequestDecorator.class);
    }

    @Test
    @SmallTest
    public void testGetCredManRequestDecorator_whenGlobalModeChrome_thenChromeDecorator() {
        mProvider.setGlobalWebauthnMode(WebauthnMode.CHROME);

        assertThat(mProvider.getCredManRequestDecorator(mWebContents))
                .isInstanceOf(GpmCredManRequestDecorator.class);
    }

    @Test
    @SmallTest
    public void testGetFido2ApiCallParams_whenGlobalModeApp_thenAppApi() {
        mProvider.setGlobalWebauthnMode(WebauthnMode.APP);

        assertThat(mProvider.getFido2ApiCallParams(mWebContents)).isEqualTo(Fido2ApiCall.APP_API);
    }

    @Test
    @SmallTest
    public void testGetFido2ApiCallParams_whenGlobalModeBrowser_thenBrowserApi() {
        mProvider.setGlobalWebauthnMode(WebauthnMode.BROWSER);

        assertThat(mProvider.getFido2ApiCallParams(mWebContents))
                .isEqualTo(Fido2ApiCall.BROWSER_API);
    }

    @Test
    @SmallTest
    public void testGetFido2ApiCallParams_whenGlobalModeChrome_thenBrowserApi() {
        mProvider.setGlobalWebauthnMode(WebauthnMode.CHROME);

        assertThat(mProvider.getFido2ApiCallParams(mWebContents))
                .isEqualTo(Fido2ApiCall.BROWSER_API);
    }

    @Test
    @SmallTest
    public void testGetWebauthnMode_whenNoGlobalMode_thenJniCalled() {
        mProvider.setGlobalWebauthnMode(WebauthnMode.NONE);

        mProvider.getWebauthnMode(mWebContents);

        verify(mNatives).getWebauthnModeForWebContents(eq(mWebContents));
    }
}
