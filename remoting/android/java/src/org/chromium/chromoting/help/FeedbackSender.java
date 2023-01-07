// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.help;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.graphics.Bitmap;
import android.os.Binder;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.view.View;

import org.chromium.base.Log;
import org.chromium.ui.UiUtils;

/**
 * This is a helper class for sending feedback.
 */
public class FeedbackSender {
    private static final String TAG = "Chromoting";

    /**
     * Maximum dimension for the screenshot to be sent to the Send Feedback handler.  This size
     * ensures the size of bitmap < 1MB, which is a requirement of the handler.
     */
    private static final int MAX_FEEDBACK_SCREENSHOT_DIMENSION = 600;

    private static final String FEEDBACK_PACKAGE = "com.google.android.gms";

    private static final String FEEDBACK_CLASS =
            "com.google.android.gms.feedback.LegacyBugReportService";

    private static final int SEND_FEEDBACK_INFO = Binder.FIRST_CALL_TRANSACTION;

    /**
     * Opens the feedback activity with a generated screenshot.
     * @param activity The activity for grabbing the screenshot and acting as the context.
     */
    public static void sendFeedback(Activity activity) {
        View rootView = activity.getWindow().getDecorView().getRootView();
        Bitmap screenshot = UiUtils.generateScaledScreenshot(rootView,
                MAX_FEEDBACK_SCREENSHOT_DIMENSION,
                Bitmap.Config.ARGB_8888);
        sendFeedback(activity, screenshot);
    }

    /**
     * Opens the feedback activity with the given screenshot image.
     * @param context The context for resource.
     * @param screenshot The screenshot image.
     */
    public static void sendFeedback(Context context, final Bitmap screenshot) {
        Intent intent = new Intent(Intent.ACTION_BUG_REPORT);
        intent.setComponent(new ComponentName(FEEDBACK_PACKAGE, FEEDBACK_CLASS));
        if (context.getPackageManager().resolveService(intent, 0) == null) {
            Log.e(TAG, "Unable to resolve Feedback service.");
            return;
        }

        ServiceConnection conn = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                try {
                    Parcel parcel = Parcel.obtain();
                    if (screenshot != null) {
                        screenshot.writeToParcel(parcel, 0);
                    }
                    service.transact(SEND_FEEDBACK_INFO, parcel, null, 0);
                    parcel.recycle();
                } catch (RemoteException ex) {
                    Log.e(TAG, "Unexpected error sending feedback: ", ex);
                }
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {}
        };

        context.bindService(intent, conn, Context.BIND_AUTO_CREATE);
    }
}
