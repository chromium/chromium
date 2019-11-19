// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.support.v7.app.AlertDialog;
import android.view.View;
import android.widget.CheckBox;

import androidx.annotation.IntDef;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chromoting.Preconditions;
import org.chromium.chromoting.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/** Helper for fetching and presenting a notification */
@JNINamespace("remoting")
public final class NotificationPresenter {
    @IntDef({State.NOT_FETCHED, State.FETCHING, State.FETCHED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface State {
        int NOT_FETCHED = 0;
        int FETCHING = 1;
        int FETCHED = 2;
    }

    @IntDef({NotificationUiState.NOT_SHOWN, NotificationUiState.SHOWN,
            NotificationUiState.SILENCED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface NotificationUiState {
        int NOT_SHOWN = 0;
        int SHOWN = 1;
        int SILENCED = 2;
    }

    private static final String PREFERENCE_UI_STATE = "notification_ui_state";
    private static final String PREFERENCE_LAST_SEEN_MESSAGE_ID =
            "notification_last_seen_message_id";

    private final long mNativeJniNotificationPresenter;
    private final Activity mActivity;

    private @State int mState;

    /**
     * @param activity The activity on which the notification will be shown.
     */
    public NotificationPresenter(Activity activity) {
        mNativeJniNotificationPresenter = NotificationPresenterJni.get().init(this);
        mActivity = activity;
        mState = State.NOT_FETCHED;
    }

    @Override
    public void finalize() {
        NotificationPresenterJni.get().destroy(mNativeJniNotificationPresenter);
    }

    /**
     * Presents notification for the given |username| if no previous notification has been shown,
     * and the user is selected for the notification.
     * @param username String that can uniquely identify a user
     */
    public void presentIfNecessary(String username) {
        if (mState != State.NOT_FETCHED) {
            return;
        }
        mState = State.FETCHING;
        NotificationPresenterJni.get().fetchNotification(mNativeJniNotificationPresenter, username);
    }

    @CalledByNative
    void onNotificationFetched(String messageId, String messageText, String linkText,
            String linkUrl, boolean allowSilence) {
        Preconditions.isTrue(mState == State.FETCHING);
        mState = State.FETCHED;

        SharedPreferences preferences = mActivity.getPreferences(Context.MODE_PRIVATE);
        @NotificationUiState
        int uiState = preferences.getInt(PREFERENCE_UI_STATE, NotificationUiState.NOT_SHOWN);
        String lastSeenMessageId = preferences.getString(PREFERENCE_LAST_SEEN_MESSAGE_ID, "");

        if (allowSilence && uiState == NotificationUiState.SILENCED
                && Objects.equals(lastSeenMessageId, messageId)) {
            return;
        }

        if (!Objects.equals(lastSeenMessageId, messageId)) {
            preferences.edit()
                    .putInt(PREFERENCE_UI_STATE, NotificationUiState.NOT_SHOWN)
                    .putString(PREFERENCE_LAST_SEEN_MESSAGE_ID, messageId)
                    .apply();
        }

        final AlertDialog.Builder builder = new AlertDialog.Builder(mActivity);
        builder.setMessage(messageText);
        builder.setPositiveButton(linkText, (DialogInterface dialog, int id) -> {
            Intent openLink = new Intent(Intent.ACTION_VIEW, Uri.parse(linkUrl));
            mActivity.startActivity(openLink);
        });
        builder.setNegativeButton(R.string.dismiss,
                (DialogInterface dialog, int id)
                        -> {
                                // Do nothing
                        });
        CheckBox silenceCheckBox;
        if (allowSilence && uiState == NotificationUiState.SHOWN) {
            View notificationDialog =
                    mActivity.getLayoutInflater().inflate(R.layout.notification_dialog, null);
            builder.setView(notificationDialog);
            silenceCheckBox = notificationDialog.findViewById(R.id.silence_checkbox);
        } else {
            silenceCheckBox = null;
        }
        builder.setOnDismissListener((DialogInterface dialog) -> {
            @NotificationUiState
            int newState = (silenceCheckBox != null && silenceCheckBox.isChecked())
                    ? NotificationUiState.SILENCED
                    : NotificationUiState.SHOWN;
            preferences.edit().putInt(PREFERENCE_UI_STATE, newState).apply();
        });

        final AlertDialog dialog = builder.create();
        dialog.show();
    }

    @CalledByNative
    void onNoNotification() {
        Preconditions.isTrue(mState == State.FETCHING);
        mState = State.NOT_FETCHED;
    }

    @NativeMethods
    interface Natives {
        long init(NotificationPresenter javaPresenter);
        void fetchNotification(long nativeJniNotificationPresenter, String username);
        void destroy(long nativeJniNotificationPresenter);
    }
}
