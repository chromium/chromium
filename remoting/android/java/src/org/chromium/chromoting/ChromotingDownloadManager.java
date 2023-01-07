// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.app.DownloadManager;
import android.app.DownloadManager.Query;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;

import org.chromium.base.ContextUtils;

import java.io.File;
import java.util.HashSet;
import java.util.Set;

/**
 * Class that manages download operation for Chromoting activity.
 */
public class ChromotingDownloadManager {
    // Undocumented outside of Android source, but held by the Android Download Manager since at
    // least Android M in order to send download completed broadcasts.
    private static final String PERMISSION_SEND_DOWNLOAD_COMPLETED_INTENTS =
            "android.permission.SEND_DOWNLOAD_COMPLETED_INTENTS";
    /**
     * Callback for download manager. This will be executed on application's main thread.
     */
    public interface Callback {
        /**
         * Called when batch download is successfully finished.
         */
        public void onBatchDownloadComplete();
    }

    private Activity mActivity;
    private BroadcastReceiver mDownloadReceiver;
    private DownloadManager mDownloadManager;
    private Set<Long> mUnfinishedDownloadIds;
    private Callback mCallback;
    private String[] mNames;
    private String[] mUris;

    public ChromotingDownloadManager(Activity activity, String[] names, String[] uris,
            Callback callback) {
        assert names.length == uris.length;

        mActivity = activity;
        mCallback = callback;
        mDownloadManager =
                (DownloadManager) mActivity.getSystemService(Context.DOWNLOAD_SERVICE);
        mNames = names.clone();
        mUris = uris.clone();
        mUnfinishedDownloadIds = new HashSet<Long>();
    }

    public ChromotingDownloadManager(Activity activity, String name, String uri,
            Callback callback) {
        this(activity, new String[] {name}, new String[] {uri}, callback);
    }

    /**
     * Download files according to given URIs and store them as given names.
     */
    public void download() {
        for (int i = 0; i < mNames.length; i++) {
            if (needToBeDownloaded(i)) {
                DownloadManager.Request request =
                        new DownloadManager.Request(Uri.parse(mUris[i]));
                request.setDestinationInExternalFilesDir(ContextUtils.getApplicationContext(),
                        Environment.DIRECTORY_DOWNLOADS, mNames[i]);
                mUnfinishedDownloadIds.add(mDownloadManager.enqueue(request));
            }
        }

        if (mUnfinishedDownloadIds.isEmpty()) {
            mActivity.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mCallback.onBatchDownloadComplete();
                }
            });
        } else {
            mDownloadReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    String action = intent.getAction();
                    if (DownloadManager.ACTION_DOWNLOAD_COMPLETE.equals(action)) {
                        Query query = new Query();
                        query.setFilterById(convertToArray(mUnfinishedDownloadIds));
                        query.setFilterByStatus(DownloadManager.STATUS_SUCCESSFUL);
                        Cursor cursor = mDownloadManager.query(query);

                        // Delete finished download id from unfinished download id set.
                        for (int i = 0; i < cursor.getCount(); i++) {
                            cursor.moveToPosition(i);
                            int downloadIdIndex = cursor.getColumnIndex(DownloadManager.COLUMN_ID);
                            mUnfinishedDownloadIds.remove(cursor.getLong(downloadIdIndex));
                        }

                        if (mUnfinishedDownloadIds.isEmpty()) {
                            mCallback.onBatchDownloadComplete();
                        }
                        cursor.close();
                    }
                }
            };

            ContextUtils.registerExportedBroadcastReceiver(mActivity, mDownloadReceiver,
                    new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE),
                    PERMISSION_SEND_DOWNLOAD_COMPLETED_INTENTS);
        }
    }

    /**
     * Perform necessary operations to close the download manager.
     */
    public void close() {
        if (!mUnfinishedDownloadIds.isEmpty()) {
            mDownloadManager.remove(convertToArray(mUnfinishedDownloadIds));
        }
        if (mDownloadReceiver != null) {
            mActivity.unregisterReceiver(mDownloadReceiver);
        }
    }

    /**
     * Check whether the file corresponding to the given index needs to be downloaded.
     */
    private boolean needToBeDownloaded(int index) {
        return !new File(getDownloadDirectory() + "/" + mNames[index]).exists();
    }

    /**
     * Get download directory path.
     */
    public String getDownloadDirectory() {
        return mActivity.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS).toString();
    }

    /**
     * Convert a Set of Long to an array of long.
     */
    private long[] convertToArray(Set<Long> data) {
        long[] result = new long[data.size()];
        int index = 0;
        for (long number : data) {
            result[index] = number;
            index++;
        }
        return result;
    }
}
