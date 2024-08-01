// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.webauthn.Fido2ApiCall.Fido2ApiCallParams;
import org.chromium.components.webauthn.cred_man.AppCredManRequestDecorator;
import org.chromium.components.webauthn.cred_man.BrowserCredManRequestDecorator;
import org.chromium.components.webauthn.cred_man.CredManRequestDecorator;
import org.chromium.components.webauthn.cred_man.GpmCredManRequestDecorator;
import org.chromium.content_public.browser.WebContents;

/**
 * This class is responsible for returning the correct {@link WebauthnMode} for a given {@link
 * WebContents}. It supports:
 *
 * <ul>
 *   <li>1. A global {@link WebauthnMode} for the singleton. This is the preferred method. To set
 *       the global state, use {@code setGlobalWebauthnMode} API. Chrome uses this API.
 *   <li>2. Per-WebContents {@link WebauthnMode}. This method is used by WebView. In order to set
 *       the mode, initialize a {@link WebauthnMode} using {@code WebauthnMode.setWebContents} and
 *       use {@code WebauthnMode.setMode} API.
 * </ul>
 *
 * <p>Note: if both methods are called, global mode has priority.
 */
@JNINamespace("webauthn")
public class WebauthnModeProvider {
    private static WebauthnModeProvider sInstance;
    private @WebauthnMode int mGlobalMode;

    public CredManRequestDecorator getCredManRequestDecorator(WebContents webContents) {
        int mode = getWebauthnMode(webContents);
        if (mode == WebauthnMode.APP) {
            return AppCredManRequestDecorator.getInstance();
        } else if (mode == WebauthnMode.BROWSER || mode == WebauthnMode.CHROME_3PP_ENABLED) {
            return BrowserCredManRequestDecorator.getInstance();
        } else if (mode == WebauthnMode.CHROME) {
            return GpmCredManRequestDecorator.getInstance();
        } else {
            assert false : "WebauthnMode not set! See this class's JavaDoc.";
        }
        return null;
    }

    public Fido2ApiCallParams getFido2ApiCallParams(WebContents webContents) {
        int mode = getWebauthnMode(webContents);
        if (mode == WebauthnMode.APP) {
            return Fido2ApiCall.APP_API;
        } else if (mode == WebauthnMode.BROWSER
                || mode == WebauthnMode.CHROME
                || mode == WebauthnMode.CHROME_3PP_ENABLED) {
            return Fido2ApiCall.BROWSER_API;
        } else {
            assert false : "WebauthnMode not set! See this class's JavaDoc.";
        }
        return null;
    }

    public @WebauthnMode int getWebauthnMode(WebContents webContents) {
        if (mGlobalMode != WebauthnMode.NONE) return mGlobalMode;
        return WebauthnModeProviderJni.get().getWebauthnModeForWebContents(webContents);
    }

    public @WebauthnMode int getGlobalWebauthnMode() {
        return mGlobalMode;
    }

    public void setGlobalWebauthnMode(@WebauthnMode int mode) {
        mGlobalMode = mode;
    }

    public void setWebauthnModeForWebContents(WebContents webContents, @WebauthnMode int mode) {
        if (webContents == null) return;
        WebauthnModeProviderJni.get().setWebauthnModeForWebContents(webContents, mode);
    }

    public static boolean isChrome(WebContents webContents) {
        @WebauthnMode int mode = getInstance().getWebauthnMode(webContents);
        return mode == WebauthnMode.CHROME || mode == WebauthnMode.CHROME_3PP_ENABLED;
    }

    public static boolean is(WebContents webContents, @WebauthnMode int expectedMode) {
        return getInstance().getWebauthnMode(webContents) == expectedMode;
    }

    public static WebauthnModeProvider getInstance() {
        if (sInstance == null) sInstance = new WebauthnModeProvider();
        return sInstance;
    }

    public static void setInstanceForTesting(WebauthnModeProvider webAuthnModeProvider) {
        // Do not forget to unset the instance to avoid leaking into other test suites.
        sInstance = webAuthnModeProvider;
    }

    private WebauthnModeProvider() {}

    @NativeMethods
    interface Natives {
        long setWebauthnModeForWebContents(WebContents webContents, @WebauthnMode int mode);

        @WebauthnMode
        int getWebauthnModeForWebContents(WebContents webContents);
    }
}
