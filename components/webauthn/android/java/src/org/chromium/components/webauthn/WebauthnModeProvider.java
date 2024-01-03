// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import androidx.annotation.IntDef;

import org.chromium.components.webauthn.cred_man.AppCredManRequestDecorator;
import org.chromium.components.webauthn.cred_man.BrowserCredManRequestDecorator;
import org.chromium.components.webauthn.cred_man.CredManRequestDecorator;
import org.chromium.components.webauthn.cred_man.GpmCredManRequestDecorator;

public class WebauthnModeProvider {
    @IntDef({WebauthnMode.NONE, WebauthnMode.BROWSER, WebauthnMode.APP, WebauthnMode.CHROME})
    public @interface WebauthnMode {
        int NONE = 0;
        int APP = 1;
        int BROWSER = 2;
        int CHROME = 3;
    }

    private static WebauthnModeProvider sInstance;
    private @WebauthnMode int mMode;

    public CredManRequestDecorator getCredManRequestDecorator() {
        if (mMode == WebauthnMode.APP) {
            return AppCredManRequestDecorator.getInstance();
        } else if (mMode == WebauthnMode.BROWSER) {
            return BrowserCredManRequestDecorator.getInstance();
        } else if (mMode == WebauthnMode.CHROME) {
            return GpmCredManRequestDecorator.getInstance();
        } else {
            assert false : "WebAuthnMode not set! Please set using WebAuthnModeProvider.setMode()";
        }
        return null;
    }

    public @WebauthnMode int getWebAuthnMode() {
        return mMode;
    }

    public void setWebAuthnMode(@WebauthnMode int mode) {
        mMode = mode;
    }

    public static WebauthnModeProvider getInstance() {
        if (sInstance == null) sInstance = new WebauthnModeProvider();
        return sInstance;
    }

    public static void setInstanceForTesting(WebauthnModeProvider webAuthnModeProvider) {
        sInstance = webAuthnModeProvider;
    }

    private WebauthnModeProvider() {}
}
