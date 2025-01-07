// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.view.View;
import android.view.ViewStructure;

import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.WebContents;

/** Implementation of {@link AccessibilityDelegate} based on {@link WebContents}. */
public class WebContentsAccessibilityDelegate implements AccessibilityDelegate {
    private final WebContentsImpl mWebContents;
    private final AccessibilityCoordinatesImpl mAccessibilityCoordinatesImpl;

    WebContentsAccessibilityDelegate(WebContents webContents) {
        mWebContents = (WebContentsImpl) webContents;
        mAccessibilityCoordinatesImpl = new AccessibilityCoordinatesImpl();
    }

    @Override
    public View getContainerView() {
        return mWebContents.getViewAndroidDelegate().getContainerView();
    }

    @Override
    public String getProductVersion() {
        return mWebContents.getProductVersion();
    }

    @Override
    public boolean isIncognito() {
        return mWebContents.isIncognito();
    }

    @Override
    public WebContents getWebContents() {
        return mWebContents;
    }

    @Override
    public AccessibilityCoordinates getAccessibilityCoordinates() {
        return mAccessibilityCoordinatesImpl;
    }

    @Override
    public void requestAccessibilitySnapshot(ViewStructure root, Runnable doneCallback) {
        mWebContents.requestAccessibilitySnapshot(root, doneCallback);
    }

    class AccessibilityCoordinatesImpl implements AccessibilityCoordinates {
        AccessibilityCoordinatesImpl() {}

        @Override
        public float fromLocalCssToPix(float css) {
            return getRenderCoordinates().fromLocalCssToPix(css);
        }

        @Override
        public float getContentOffsetYPix() {
            return getRenderCoordinates().getContentOffsetYPix();
        }

        @Override
        public float getScrollXPix() {
            return getRenderCoordinates().getScrollXPix();
        }

        @Override
        public float getScrollYPix() {
            return getRenderCoordinates().getScrollYPix();
        }

        @Override
        public float getContentWidthCss() {
            return getRenderCoordinates().getContentWidthCss();
        }

        @Override
        public float getContentHeightCss() {
            return getRenderCoordinates().getContentHeightCss();
        }

        @Override
        public float getScrollX() {
            return getRenderCoordinates().getScrollX();
        }

        @Override
        public float getScrollY() {
            return getRenderCoordinates().getScrollY();
        }

        @Override
        public int getLastFrameViewportWidthPixInt() {
            return getRenderCoordinates().getLastFrameViewportWidthPixInt();
        }

        @Override
        public int getLastFrameViewportHeightPixInt() {
            return getRenderCoordinates().getLastFrameViewportHeightPixInt();
        }

        private RenderCoordinatesImpl getRenderCoordinates() {
            return mWebContents.getRenderCoordinates();
        }
    }
}
