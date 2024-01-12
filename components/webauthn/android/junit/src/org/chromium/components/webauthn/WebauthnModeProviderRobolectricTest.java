// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.webauthn.WebauthnModeProvider.WebauthnMode;
import org.chromium.components.webauthn.cred_man.AppCredManRequestDecorator;
import org.chromium.components.webauthn.cred_man.BrowserCredManRequestDecorator;
import org.chromium.components.webauthn.cred_man.GpmCredManRequestDecorator;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {})
public class WebauthnModeProviderRobolectricTest {
    private WebauthnModeProvider mProvider = WebauthnModeProvider.getInstance();

    @Test
    @SmallTest
    public void testGetCredManRequestDecorator_whenModeApp_thenAppDecorator() {
        mProvider.setWebauthnMode(WebauthnMode.APP);

        assertThat(mProvider.getCredManRequestDecorator())
                .isInstanceOf(AppCredManRequestDecorator.class);
    }

    @Test
    @SmallTest
    public void testGetCredManRequestDecorator_whenModeBrowser_thenBrowserDecorator() {
        mProvider.setWebauthnMode(WebauthnMode.BROWSER);

        assertThat(mProvider.getCredManRequestDecorator())
                .isInstanceOf(BrowserCredManRequestDecorator.class);
    }

    @Test
    @SmallTest
    public void testGetCredManRequestDecorator_whenModeChrome_thenChromeDecorator() {
        mProvider.setWebauthnMode(WebauthnMode.CHROME);

        assertThat(mProvider.getCredManRequestDecorator())
                .isInstanceOf(GpmCredManRequestDecorator.class);
    }

    @Test
    @SmallTest
    public void testGetFido2ApiCallParams_whenModeApp_thenAppApi() {
        mProvider.setWebauthnMode(WebauthnMode.APP);

        assertThat(mProvider.getFido2ApiCallParams()).isEqualTo(Fido2ApiCall.APP_API);
    }

    @Test
    @SmallTest
    public void testGetFido2ApiCallParams_whenModeBrowser_thenBrowserApi() {
        mProvider.setWebauthnMode(WebauthnMode.BROWSER);

        assertThat(mProvider.getFido2ApiCallParams()).isEqualTo(Fido2ApiCall.BROWSER_API);
    }

    @Test
    @SmallTest
    public void testGetFido2ApiCallParams_whenModeChrome_thenBrowserApi() {
        mProvider.setWebauthnMode(WebauthnMode.CHROME);

        assertThat(mProvider.getFido2ApiCallParams()).isEqualTo(Fido2ApiCall.BROWSER_API);
    }
}
