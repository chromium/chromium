// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.os.Build;
import android.view.ActionMode;
import android.view.ActionMode.Callback2;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.PopupWindow;

import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * This is a workaround for LG Email app: crbug.com/651706
 * LG Email app runs UI-thread APIs from InputConnection methods. This is not allowable with
 * the change ImeThread introduces, and LG Email app is bundled and cannot be updated without
 * a system update. However, LG Email team is committed to fixing this in the near future.
 * This is a version code limited workaround to avoid crashes in the app.
 */
public final class LGEmailActionModeWorkaroundImpl {
    private static final String TAG = "Ime";

    // This is the last broken version shipped on LG V20/NRD90M.
    public static final int LGEmailWorkaroundMaxVersion = 67502100;

    private LGEmailActionModeWorkaroundImpl() {}

    public static boolean isSafeVersion(int versionCode) {
        return versionCode > LGEmailWorkaroundMaxVersion;
    }

    /**
     * Run this workaround only when it's applicable and absolutely necessary.
     * @param context The context
     * @param actionMode The {@ActionMode} to apply the workaround to.
     */
    public static void runIfNecessary(Context context, ActionMode actionMode) {
        if (shouldAllowActionModeDestroyOnNonUiThread(context)) {
            allowActionModeDestroyOnNonUiThread(actionMode);
        }
    }

    private static boolean shouldAllowActionModeDestroyOnNonUiThread(Context context) {
        String appName = context.getPackageName();
        int versionCode = PackageUtils.getPackageVersion(appName);
        if (versionCode == -1) return false;

        int appTargetSdkVersion = context.getApplicationInfo().targetSdkVersion;
        if (appTargetSdkVersion < Build.VERSION_CODES.M
                || appTargetSdkVersion > Build.VERSION_CODES.N) {
            return false;
        }

        final String lgeMailPackageId = "com.lge.email";
        if (!lgeMailPackageId.equals(appName)) return false;
        if (versionCode > LGEmailWorkaroundMaxVersion) return false;

        Log.w(
                TAG,
                "Working around action mode LG Email bug in WebView (http://crbug.com/651706). "
                        + "APK name: "
                        + lgeMailPackageId
                        + ", versionCode: "
                        + versionCode);
        return true;
    }

    private static void allowActionModeDestroyOnNonUiThread(ActionMode actionMode) {
        // LG Email app dismisses ActionMode whenever InputConnection#setComposingText() or
        // InputConnection#commitText() occurs. But they do on ImeThread, not on UI thread and
        // this causes crashes in two places.
        try {
            // Part I: post ActionMode.Callback2#onDestroyActionMode() on UI thread.
            final ActionMode.Callback2 c = (Callback2) getField(actionMode, "mCallback");
            setField(
                    actionMode,
                    "mCallback",
                    new ActionMode.Callback2() {
                        @Override
                        public boolean onCreateActionMode(ActionMode mode, Menu menu) {
                            return c.onCreateActionMode(mode, menu);
                        }

                        @Override
                        public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
                            return c.onPrepareActionMode(mode, menu);
                        }

                        @Override
                        public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
                            return c.onActionItemClicked(mode, item);
                        }

                        @Override
                        public void onDestroyActionMode(final ActionMode mode) {
                            PostTask.postTask(
                                    TaskTraits.UI_DEFAULT,
                                    new Runnable() {
                                        @Override
                                        public void run() {
                                            c.onDestroyActionMode(mode);
                                        }
                                    });
                        }
                    });

            // Part II: post PopupWindow#dismiss() on UI thread.
            final Object floatingToolbar = getField(actionMode, "mFloatingToolbar");
            final Object popup = getField(floatingToolbar, "mPopup");
            final ViewGroup contentContainer = (ViewGroup) getField(popup, "mContentContainer");
            final PopupWindow popupWindow = (PopupWindow) getField(popup, "mPopupWindow");
            Method createExitAnimation =
                    floatingToolbar
                            .getClass()
                            .getDeclaredMethod(
                                    "createExitAnimation",
                                    View.class,
                                    int.class,
                                    AnimatorListener.class);
            createExitAnimation.setAccessible(true);
            Object newDismissAnimation =
                    createExitAnimation.invoke(
                            null,
                            contentContainer,
                            150,
                            new AnimatorListenerAdapter() {
                                @Override
                                public void onAnimationEnd(Animator animation) {
                                    PostTask.postTask(
                                            TaskTraits.UI_DEFAULT,
                                            new Runnable() {
                                                @Override
                                                public void run() {
                                                    popupWindow.dismiss();
                                                    contentContainer.removeAllViews();
                                                }
                                            });
                                }
                            });
            setField(popup, "mDismissAnimation", newDismissAnimation);
        } catch (NoSuchFieldException
                | IllegalAccessException
                | IllegalArgumentException
                | NoSuchMethodException
                | InvocationTargetException e) {
            // Ignore exception and just return.
        } catch (Exception e) {
            Log.w(TAG, "Error occurred during LGEmailActionModeWorkaround: ", e);
        }
    }

    private static Object getField(Object obj, String fieldName)
            throws NoSuchFieldException, IllegalAccessException, IllegalArgumentException {
        Field f = obj.getClass().getDeclaredField(fieldName);
        f.setAccessible(true);
        return f.get(obj);
    }

    private static void setField(Object obj, String fieldName, Object value)
            throws NoSuchFieldException, IllegalAccessException, IllegalArgumentException {
        Field f = obj.getClass().getDeclaredField(fieldName);
        f.setAccessible(true);
        f.set(obj, value);
    }
}
