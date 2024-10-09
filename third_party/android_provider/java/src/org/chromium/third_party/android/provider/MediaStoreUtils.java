/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

package org.chromium.third_party.android.provider;

import android.annotation.TargetApi;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.provider.MediaStore;
import android.provider.MediaStore.DownloadColumns;
import android.provider.MediaStore.MediaColumns;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.io.FileNotFoundException;
import java.io.OutputStream;
import java.util.Objects;

/**
 * Utility class to contribute download to the public download collection using
 * MediaStore API from Q.
 */
public class MediaStoreUtils {
    private static final String TAG = "MediaStoreUtils";

    /**
     * Creates a new pending media item using the given parameters. Pending items
     * are expected to have a short lifetime, and owners should either
     * {@link PendingSession#publish()} or {@link PendingSession#abandon()} a
     * pending item within a few hours after first creating it.
     *
     * @param context Application context.
     * @param params Parameters used to configure the item.
     * @return token which can be passed to {@link #openPending(Context, Uri)}
     *         to work with this pending item.
     */
    public static @NonNull Uri createPending(
            @NonNull Context context, @NonNull PendingParams params) {
        return context.getContentResolver().insert(params.mInsertUri, params.mInsertValues);
    }

    /**
     * Opens a pending media item to make progress on it. You can open a pending
     * item multiple times before finally calling either
     * {@link PendingSession#publish()} or {@link PendingSession#abandon()}.
     *
     * @param uri token which was previously returned from
     *            {@link #createPending(Context, PendingParams)}.
     * @return pending session that was opened.
     */
    public static @NonNull PendingSession openPending(@NonNull Context context, @NonNull Uri uri) {
        return new PendingSession(context, uri);
    }

    /**
     * Parameters that describe a pending media item.
     */
    public static class PendingParams {
        final Uri mInsertUri;
        final ContentValues mInsertValues;

        /**
         * Creates parameters that describe a pending media item.
         *
         * @param insertUri the {@code content://} Uri where this pending item
         *            should be inserted when finally published. For example, to
         *            publish an image, use
         *            {@link MediaStore.Images.Media#getContentUri(String)}.
         * @param displayName Display name of the item.
         * @param mimeType MIME type of the item.
         */
        public PendingParams(
                @NonNull Uri insertUri, @NonNull String displayName, @NonNull String mimeType) {
            mInsertUri = Objects.requireNonNull(insertUri);
            final long now = System.currentTimeMillis() / 1000;
            mInsertValues = new ContentValues();
            mInsertValues.put(MediaColumns.DISPLAY_NAME, Objects.requireNonNull(displayName));
            mInsertValues.put(MediaColumns.MIME_TYPE, Objects.requireNonNull(mimeType));
            mInsertValues.put(MediaColumns.DATE_ADDED, now);
            mInsertValues.put(MediaColumns.DATE_MODIFIED, now);
            try {
                setPendingContentValues(this.mInsertValues, true);
            } catch (Exception e) {
                Log.e(TAG, "Unable to set pending content values.", e);
            }
        }

        /**
         * Optionally sets the Uri from where the file has been downloaded. This is used
         * for files being added to {@link Downloads} table.
         *
         * @see DownloadColumns#DOWNLOAD_URI
         */
        @TargetApi(Build.VERSION_CODES.Q)
        public void setDownloadUri(@Nullable Uri downloadUri) {
            if (downloadUri == null) {
                mInsertValues.remove(DownloadColumns.DOWNLOAD_URI);
            } else {
                mInsertValues.put(DownloadColumns.DOWNLOAD_URI, downloadUri.toString());
            }
        }

        /**
         * Optionally set the Uri indicating HTTP referer of the file. This is used for
         * files being added to {@link Downloads} table.
         *
         * @see DownloadColumns#REFERER_URI
         */
        @TargetApi(Build.VERSION_CODES.Q)
        public void setRefererUri(@Nullable Uri refererUri) {
            if (refererUri == null) {
                mInsertValues.remove(DownloadColumns.REFERER_URI);
            } else {
                mInsertValues.put(DownloadColumns.REFERER_URI, refererUri.toString());
            }
        }

        /**
         * Sets the expiration time of the download.
         *
         * @param time Epoch time in seconds.
         */
        public void setExpirationTime(long time) {
            mInsertValues.put("date_expires", time);
        }
    }

    /**
     * Session actively working on a pending media item. Pending items are
     * expected to have a short lifetime, and owners should either
     * {@link PendingSession#publish()} or {@link PendingSession#abandon()} a
     * pending item within a few hours after first creating it.
     */
    public static class PendingSession implements AutoCloseable {
        private final Context mContext;
        private final Uri mUri;

        /**
         * Create a new pending session item to be published.
         * @param context Contexxt of the application.
         * @param uri Token which was previously returned from
         *            {@link #createPending(Context, PendingParams)}.
         */
        PendingSession(Context context, Uri uri) {
            mContext = Objects.requireNonNull(context);
            mUri = Objects.requireNonNull(uri);
        }

        /**
         * Open the underlying file representing this media item. When a media
         * item is successfully completed, you should
         * {@link ParcelFileDescriptor#close()} and then {@link #publish()} it.
         *
         * @return ParcelFileDescriptor to be written into.
         */
        public @NonNull ParcelFileDescriptor open() throws FileNotFoundException {
            return mContext.getContentResolver().openFileDescriptor(mUri, "rw");
        }

        /**
         * Open the underlying file representing this media item. When a media
         * item is successfully completed, you should
         * {@link OutputStream#close()} and then {@link #publish()} it.
         *
         * @return OutputStream to be written into.
         */
        public @NonNull OutputStream openOutputStream() throws FileNotFoundException {
            return mContext.getContentResolver().openOutputStream(mUri);
        }

        /**
         * When this media item is successfully completed, call this method to
         * publish and make the final item visible to the user.
         *
         * @return the final {@code content://} Uri representing the newly
         *         published media.
         */
        public @NonNull Uri publish() {
            try {
                final ContentValues values = new ContentValues();
                setPendingContentValues(values, false);
                values.putNull("date_expires");
                mContext.getContentResolver().update(mUri, values, null, null);
            } catch (Exception e) {
                Log.e(TAG, "Unable to publish pending session.", e);
            }
            return mUri;
        }

        /**
         * When this media item has failed to be completed, call this method to
         * destroy the pending item record and any data related to it.
         */
        public void abandon() {
            try {
                mContext.getContentResolver().delete(mUri, null, null);
            } catch (Exception e) {
                Log.e(TAG, "Unable to delete pending session.", e);
            }
        }

        @Override
        public void close() {
            // No resources to close, but at least we can inform people that no
            // progress is being actively made.
        }
    }

    /**
     * Helper method to set the ContentValues to pending or non-pending.
     * @param values ContentValues to be set.
     * @param isPending Whether the item is pending.
     */
    @TargetApi(Build.VERSION_CODES.Q)
    private static void setPendingContentValues(ContentValues values, boolean isPending)
            throws Exception {
        values.put(MediaColumns.IS_PENDING, isPending ? 1 : 0);
    }
}
