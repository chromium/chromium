// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webrtc;

import android.app.PendingIntent;
import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;

import org.chromium.base.ContextUtils;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;

/** Helper to build a notification for Media Capture and Streams. */
public class MediaCaptureNotificationUtil {
    @IntDef({
        MediaType.NO_MEDIA,
        MediaType.AUDIO_AND_VIDEO,
        MediaType.VIDEO_ONLY,
        MediaType.AUDIO_ONLY,
        MediaType.SCREEN_CAPTURE
    })
    public @interface MediaType {
        int NO_MEDIA = 0;
        int AUDIO_AND_VIDEO = 1;
        int VIDEO_ONLY = 2;
        int AUDIO_ONLY = 3;
        int SCREEN_CAPTURE = 4;
    }

    /**
     * Creates a notification for the provided parameters.
     * @param mediaType Media type of the notification.
     * @param url Url of the current webrtc call, or null if no URL should be displayed.
     * @param appName the display name for the app, e.g. "Chromium".
     * @param contentIntent the intent to be sent when the notification is clicked.
     * @param stopIntent if non-null, a stop button that triggers this intent will be added.
     */
    public static NotificationWrapper createNotification(
            NotificationWrapperBuilder builder,
            @MediaType int mediaType,
            @Nullable String url,
            @Nullable String appName,
            @Nullable PendingIntentProvider contentIntent,
            @Nullable PendingIntent stopIntent) {
        Context appContext = ContextUtils.getApplicationContext();
        builder.setAutoCancel(false)
                .setOngoing(true)
                .setLocalOnly(true)
                .setContentIntent(contentIntent)
                .setSmallIcon(getNotificationIconId(mediaType));

        if (stopIntent != null) {
            builder.setPriorityBeforeO(NotificationCompat.PRIORITY_HIGH);
            builder.setVibrate(new long[0]);
            builder.addAction(
                    R.drawable.ic_stop_white_24dp,
                    appContext.getString(R.string.accessibility_stop),
                    stopIntent);
        } else {
            assert mediaType != MediaType.SCREEN_CAPTURE : "SCREEN_CAPTURE requires a stop action";
        }

        // App name is automatically added to the title from Android N.
        builder.setContentTitle(getNotificationTitleText(mediaType));

        String contentText = null;
        if (url == null) {
            contentText =
                    appContext.getString(
                            R.string.media_capture_notification_content_text_incognito);
            builder.setSubText(appContext.getString(R.string.notification_incognito_tab));
        } else {
            String urlForDisplay =
                    UrlFormatter.formatUrlForSecurityDisplay(
                            url, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
            if (contentIntent == null) {
                contentText = urlForDisplay;
            } else {
                contentText =
                        appContext.getString(
                                R.string.media_capture_notification_content_text, urlForDisplay);
            }
        }

        builder.setContentText(contentText);
        return builder.buildWithBigTextStyle(contentText);
    }

    /**
     * @param mediaType Media type of the notification.
     * @return user-facing text for the provided mediaType.
     */
    private static String getNotificationTitleText(@MediaType int mediaType) {
        int notificationContentTextId = 0;
        if (mediaType == MediaType.SCREEN_CAPTURE) {
            notificationContentTextId = R.string.screen_capture_notification_title;
        } else if (mediaType == MediaType.AUDIO_AND_VIDEO) {
            notificationContentTextId = R.string.video_audio_capture_notification_title;
        } else if (mediaType == MediaType.VIDEO_ONLY) {
            notificationContentTextId = R.string.video_capture_notification_title;
        } else if (mediaType == MediaType.AUDIO_ONLY) {
            notificationContentTextId = R.string.audio_capture_notification_title;
        }

        return ContextUtils.getApplicationContext().getString(notificationContentTextId);
    }

    /**
     * @param mediaType Media type of the notification.
     * @return An icon id of the provided mediaType.
     */
    private static int getNotificationIconId(@MediaType int mediaType) {
        int notificationIconId = 0;
        if (mediaType == MediaType.AUDIO_AND_VIDEO) {
            notificationIconId = R.drawable.webrtc_video;
        } else if (mediaType == MediaType.VIDEO_ONLY) {
            notificationIconId = R.drawable.webrtc_video;
        } else if (mediaType == MediaType.AUDIO_ONLY) {
            notificationIconId = R.drawable.webrtc_audio;
        } else if (mediaType == MediaType.SCREEN_CAPTURE) {
            notificationIconId = R.drawable.webrtc_video;
        }
        return notificationIconId;
    }
}
