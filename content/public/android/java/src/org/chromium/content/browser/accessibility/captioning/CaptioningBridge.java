// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility.captioning;

import android.content.Context;
import android.view.accessibility.CaptioningManager;

import org.chromium.base.ContextUtils;

import java.util.Locale;

/** Implementation of SystemCaptioningBridge that uses CaptioningManager. */
public class CaptioningBridge extends CaptioningManager.CaptioningChangeListener
        implements SystemCaptioningBridge {
    private final CaptioningChangeDelegate mCaptioningChangeDelegate;
    private final CaptioningManager mCaptioningManager;
    private static CaptioningBridge sInstance;

    public static CaptioningBridge getInstance() {
        if (sInstance == null) {
            sInstance = new CaptioningBridge();
        }
        return sInstance;
    }

    private CaptioningBridge() {
        mCaptioningChangeDelegate = new CaptioningChangeDelegate();
        mCaptioningManager =
                (CaptioningManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.CAPTIONING_SERVICE);
    }

    @Override
    public void onEnabledChanged(boolean enabled) {
        mCaptioningChangeDelegate.onEnabledChanged(enabled);
    }

    @Override
    public void onFontScaleChanged(float fontScale) {
        mCaptioningChangeDelegate.onFontScaleChanged(fontScale);
    }

    @Override
    public void onLocaleChanged(Locale locale) {
        mCaptioningChangeDelegate.onLocaleChanged(locale);
    }

    @Override
    public void onUserStyleChanged(CaptioningManager.CaptionStyle userStyle) {
        final CaptioningStyle captioningStyle = getCaptioningStyleFrom(userStyle);
        mCaptioningChangeDelegate.onUserStyleChanged(captioningStyle);
    }

    /** Force-sync the current closed caption settings to the delegate */
    private void syncToDelegate() {
        mCaptioningChangeDelegate.onEnabledChanged(mCaptioningManager.isEnabled());
        mCaptioningChangeDelegate.onFontScaleChanged(mCaptioningManager.getFontScale());
        mCaptioningChangeDelegate.onLocaleChanged(mCaptioningManager.getLocale());
        mCaptioningChangeDelegate.onUserStyleChanged(
                getCaptioningStyleFrom(mCaptioningManager.getUserStyle()));
    }

    @Override
    public void syncToListener(SystemCaptioningBridge.SystemCaptioningBridgeListener listener) {
        if (!mCaptioningChangeDelegate.hasActiveListener()) {
            syncToDelegate();
        }
        mCaptioningChangeDelegate.notifyListener(listener);
    }

    @Override
    public void addListener(SystemCaptioningBridge.SystemCaptioningBridgeListener listener) {
        if (!mCaptioningChangeDelegate.hasActiveListener()) {
            mCaptioningManager.addCaptioningChangeListener(this);
            syncToDelegate();
        }
        mCaptioningChangeDelegate.addListener(listener);
        mCaptioningChangeDelegate.notifyListener(listener);
    }

    @Override
    public void removeListener(SystemCaptioningBridge.SystemCaptioningBridgeListener listener) {
        mCaptioningChangeDelegate.removeListener(listener);
        if (!mCaptioningChangeDelegate.hasActiveListener()) {
            mCaptioningManager.removeCaptioningChangeListener(this);
        }
    }

    /**
     * Create a Chromium CaptioningStyle from a platform CaptionStyle
     *
     * @param userStyle the platform CaptionStyle
     * @return a Chromium CaptioningStyle
     */
    private CaptioningStyle getCaptioningStyleFrom(CaptioningManager.CaptionStyle userStyle) {
        return CaptioningStyle.createFrom(userStyle);
    }
}
