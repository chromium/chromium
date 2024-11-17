// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.FrameLayout;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.components.embedder_support.view.ContentViewRenderView;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Container and generator of ShellViews. */
@JNINamespace("content")
public class ShellManager extends FrameLayout {

    public static final String DEFAULT_SHELL_URL = "http://www.google.com";
    private WindowAndroid mWindow;
    private Shell mActiveShell;

    // The target for all content rendering.
    private ContentViewRenderView mContentViewRenderView;

    /** Constructor for inflating via XML. */
    public ShellManager(final Context context, AttributeSet attrs) {
        super(context, attrs);
        ShellManagerJni.get().init(this);
    }

    /**
     * @param window The window used to generate all shells.
     */
    public void setWindow(WindowAndroid window) {
        assert window != null;
        mWindow = window;
        mContentViewRenderView = new ContentViewRenderView(getContext());
        mContentViewRenderView.onNativeLibraryLoaded(window);
    }

    /**
     * @return The window used to generate all shells.
     */
    public WindowAndroid getWindow() {
        return mWindow;
    }

    /** Get the ContentViewRenderView. */
    public ContentViewRenderView getContentViewRenderView() {
        return mContentViewRenderView;
    }

    /**
     * @return The currently visible shell view or null if one is not showing.
     */
    public Shell getActiveShell() {
        return mActiveShell;
    }

    /**
     * Creates a new shell pointing to the specified URL.
     * @param url The URL the shell should load upon creation.
     */
    public void launchShell(String url) {
        ThreadUtils.assertOnUiThread();
        Shell previousShell = mActiveShell;
        ShellManagerJni.get().launchShell(url);
        if (previousShell != null) previousShell.close();
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private Object createShell(long nativeShellPtr) {
        if (mContentViewRenderView == null) {
            mContentViewRenderView = new ContentViewRenderView(getContext());
            mContentViewRenderView.onNativeLibraryLoaded(mWindow);
        }
        LayoutInflater inflater =
                (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        Shell shellView = (Shell) inflater.inflate(R.layout.shell_view, null);
        shellView.initialize(nativeShellPtr, mWindow);

        // TODO(tedchoc): Allow switching back to these inactive shells.
        if (mActiveShell != null) removeShell(mActiveShell);

        showShell(shellView);
        return shellView;
    }

    private void showShell(Shell shellView) {
        shellView.setContentViewRenderView(mContentViewRenderView);
        addView(
                shellView,
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        mActiveShell = shellView;
        WebContents webContents = mActiveShell.getWebContents();
        if (webContents != null) {
            mContentViewRenderView.setCurrentWebContents(webContents);
            webContents.updateWebContentsVisibility(Visibility.VISIBLE);
        }
    }

    @CalledByNative
    private void removeShell(Shell shellView) {
        if (shellView == mActiveShell) mActiveShell = null;
        if (shellView.getParent() == null) return;
        shellView.setContentViewRenderView(null);
        removeView(shellView);
    }

    /**
     * Destroys the Shell manager and associated components.
     * Always called at activity exit, and potentially called by native in cases where we need to
     * control the timing of mContentViewRenderView destruction. Must handle being called twice.
     */
    @CalledByNative
    public void destroy() {
        // Remove active shell (Currently single shell support only available).
        if (mActiveShell != null) {
            removeShell(mActiveShell);
        }
        if (mContentViewRenderView != null) {
            mContentViewRenderView.destroy();
            mContentViewRenderView = null;
        }
    }

    @NativeMethods
    interface Natives {
        void init(Object shellManagerInstance);

        void launchShell(String url);
    }
}
