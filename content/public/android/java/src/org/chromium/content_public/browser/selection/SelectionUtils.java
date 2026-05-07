// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.selection;

import android.app.SearchManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.provider.Browser;
import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.content.R;

/** Shared utility class for text selection. */
@NullMarked
public class SelectionUtils {
    private static final String TAG = "SelectionUtils";

    /**
     * Android Intent size limitations prevent sending over a megabyte of data. Limit query lengths
     * to 100kB because other things may be added to the Intent.
     */
    public static final int MAX_SHARE_QUERY_LENGTH = 100000;

    /** Google search doesn't support requests slightly larger than this. */
    public static final int MAX_SEARCH_QUERY_LENGTH = 1000;

    /**
     * Trim a given string query to be processed safely.
     *
     * @param query a raw query to sanitize.
     * @param maxLength maximum length to which the query will be truncated.
     */
    public static String sanitizeQuery(String query, int maxLength) {
        if (TextUtils.isEmpty(query) || query.length() < maxLength) return query;
        Log.w(TAG, "Truncating oversized query (" + query.length() + ").");
        return query.substring(0, maxLength) + "…";
    }

    /**
     * Perform a share action.
     *
     * @param context Context used to start activity.
     * @param text The selected text to share.
     */
    public static void share(Context context, String text) {
        String query = sanitizeQuery(text, MAX_SHARE_QUERY_LENGTH);
        if (TextUtils.isEmpty(query)) return;

        Intent send = new Intent(Intent.ACTION_SEND);
        send.setType("text/plain");
        send.putExtra(Intent.EXTRA_TEXT, query);
        try {
            Intent i =
                    Intent.createChooser(
                            send, context.getString(R.string.actionbar_share));
            i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(i);
        } catch (ActivityNotFoundException ex) {
            // If no app handles it, do nothing.
        }
    }

    /**
     * Perform a search action.
     *
     * @param context Context used to start activity.
     * @param text The selected text to search.
     */
    @SuppressWarnings(value = "UnsafeImplicitIntentLaunch")
    public static void webSearch(Context context, String text) {
        String query = sanitizeQuery(text, MAX_SEARCH_QUERY_LENGTH);
        if (TextUtils.isEmpty(query)) return;

        Intent i = new Intent(Intent.ACTION_WEB_SEARCH);
        i.putExtra(SearchManager.EXTRA_NEW_SEARCH, true);
        i.putExtra(SearchManager.QUERY, query);
        i.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        i.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            context.startActivity(i);
        } catch (ActivityNotFoundException ex) {
            // If no app handles it, do nothing.
        }
    }

    /**
     * Perform a translate action.
     *
     * @param context Context used to start activity.
     * @param text The selected text to translate.
     */
    public static void translate(Context context, String text) {
        String query = sanitizeQuery(text, MAX_SHARE_QUERY_LENGTH);
        if (TextUtils.isEmpty(query)) return;

        Intent intent = new Intent(Intent.ACTION_TRANSLATE);
        intent.putExtra(Intent.EXTRA_TEXT, query);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            context.startActivity(intent);
        } catch (ActivityNotFoundException ex) {
            // If no app handles it, do nothing.
        }
    }
}
